#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "btstack.h"
#include "btstack_config.h"

// DualSense HID descriptor is ~273 bytes over USB and larger over BT; give
// btstack room so the SDP fetch doesn't truncate/fail (we don't parse it, but
// btstack still needs the fetch to complete before it delivers reports)
#define REMOTE_DEVICE_DESCRIPTOR_BUFSIZE 512
#define INQUIRY_DURATION 5
#define MAX_DISCOVERED_DEVICES 20 // up to 20 in a singular scan

#include "bt_hid_bridge.h"

static uint8_t remote_descriptor_storage[REMOTE_DEVICE_DESCRIPTOR_BUFSIZE];
InputReport curr_report;

typedef enum {
    DEVICE_NAME_UNKNOWN,      // haven't asked for the name yet
    DEVICE_NAME_REQUESTED,    // gap_remote_name_request() sent, waiting
    DEVICE_NAME_KNOWN         // name fetched (or came free via EIR)
} device_name_state_t; // used when we have to manually request name

typedef struct {
    bd_addr_t addr; // device address
    uint8_t page_scan_repition_mode; // how often the other device scans to pair
    uint16_t clock_offset; // timestamp of discovery
    device_name_state_t name_state; // has name been discovered yet?
} device;

// tracking current BT status
typedef enum { APP_SCANNING, APP_CONNECTING, APP_CONNECTED } app_state_t;
static app_state_t app_state = APP_SCANNING;

static const char *target_name_substring = "DualSense";
static btstack_packet_callback_registration_t hci_event_callback_registration;
static device discovered_devices[MAX_DISCOVERED_DEVICES];
static int num_devices_discovered = 0;
static bool target_found = false;
uint16_t connection_id = 0;
static bool hid_descriptor_available = false;

// additional helper method strcasestr() equivalent code.
static bool name_contains(const char *haystack, const char *needle) {
    size_t hlen = strlen(haystack);
    size_t nlen = strlen(needle);
    if (nlen == 0 || nlen > hlen) return false;
    for (size_t i = 0; i + nlen <= hlen; i++) {
        size_t j = 0;
        while (j < nlen) {
            char a = haystack[i + j];
            char b = needle[j];
            if (a >= 'A' && a <= 'Z') a += 32;
            if (b >= 'A' && b <= 'Z') b += 32;
            if (a != b) break;
            j++;
        }
        if (j == nlen) return true;
    }
    return false;
}

// ---------------- LED STATUS --------------------------
// AI-GENERATED
// the on-board LED is our only observable output (no serial console), so it
// doubles as a status readout, refreshed on a repeating btstack timer:
//   scanning       -> slow blink   (~1 Hz)
//   connecting     -> medium blink (~2.5 Hz)
//   connected      -> solid on
//   recent failure -> fast blink   (~5 Hz) for ~1.5 s, then the current state
#define LED_TICK_MS 50
#define LED_ERROR_TICKS (1500 / LED_TICK_MS)

static btstack_timer_source_t led_timer;
static uint32_t led_tick = 0;
static uint32_t led_error_ticks_left = 0;

// call from any failure path to flash the LED fast for a short window
static void led_signal_error(void) {
    led_error_ticks_left = LED_ERROR_TICKS;
}

static void led_timer_handler(btstack_timer_source_t *ts) {
    led_tick++;

    bool led_on;
    if (led_error_ticks_left > 0) {
        led_error_ticks_left--;
        led_on = (led_tick & 1) == 0;       // ~5 Hz
    } else if (app_state == APP_CONNECTED) {
        led_on = true;                       // solid
    } else if (app_state == APP_CONNECTING) {
        led_on = (led_tick % 4) < 2;         // ~2.5 Hz
    } else {
        led_on = (led_tick % 20) < 10;       // ~1 Hz (scanning)
    }
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);

    // re-arm: btstack timers are one-shot
    btstack_run_loop_set_timer(ts, LED_TICK_MS);
    btstack_run_loop_add_timer(ts);
}

static void led_status_start(void) {
    btstack_run_loop_set_timer_handler(&led_timer, led_timer_handler);
    btstack_run_loop_set_timer(&led_timer, LED_TICK_MS);
    btstack_run_loop_add_timer(&led_timer);
}

// ---------------- METHODS --------------------------
static void connect(bd_addr_t target) {
    target_found = true;
    app_state = APP_CONNECTING;
    gap_inquiry_stop(); // stop scanning

    uint8_t status = hid_host_connect(target, HID_PROTOCOL_MODE_REPORT, &connection_id);

    if (status != ERROR_CODE_SUCCESS) { // failed to connect
        target_found = false;
        app_state = APP_SCANNING;
        led_signal_error();
    }
}

static bool request_next(void) {
    for (int i = 0; i < num_devices_discovered; i++) {
        if (discovered_devices[i].name_state == DEVICE_NAME_UNKNOWN) {

            discovered_devices[i].name_state = DEVICE_NAME_REQUESTED;
            gap_remote_name_request(
                discovered_devices[i].addr,
                discovered_devices[i].page_scan_repition_mode,
                discovered_devices[i].clock_offset | 0x8000
            );
            return true;
        }
    }
    return false;
}


int get_device_index(const bd_addr_t addr) { // pointer to first byte in struct
    for (int i = 0; i < num_devices_discovered; i++) {
        device curr_device = discovered_devices[i];
        if (bd_addr_cmp(addr, curr_device.addr) == 0) return i;
    }
    return -1;
}

void continue_discovery() {
    // if names to be requested, requests them
    // otherwise restarts device discovery
    if (request_next()) return;
    
    num_devices_discovered = 0;
    gap_inquiry_start(INQUIRY_DURATION);

}

// The DualSense HID report descriptor exposed over Bluetooth only maps the
// stick axes as real HID usages; the buttons/hat/triggers live in a
// vendor-defined blob, so btstack_hid_parser can't reach them. Every other
// project (Linux hid-playstation, DS4Windows, ...) parses the DualSense by
// fixed byte offsets instead -- so do we.
//
// Two input report layouts arrive over BT:
//   id 0x01 : minimal    -> [id][LX][LY][RX][RY][btn0][btn1][btn2][L2][R2]
//   id 0x31 : full/rich   -> [id][seq][LX][LY][RX][RY][L2][R2][seq#][btn0][btn1][btn2]...
// The pad only starts sending 0x31 after the host reads feature report 0x05
// (see HID_SUBEVENT_DESCRIPTOR_AVAILABLE below).
//
// btn0: bits 0-3 = hat (0-7 dir, 8 = neutral), 0x10 Square, 0x20 Cross,
//       0x40 Circle, 0x80 Triangle
// btn1: 0x01 L1, 0x02 R1, 0x04 L2, 0x08 R2, 0x10 Create, 0x20 Options,
//       0x40 L3, 0x80 R3
// btn2: 0x01 PS, 0x02 Touchpad click, 0x04 Mic

// released-everything state (sticks centred, dpad neutral)
static void curr_report_neutral(void) {
    InputReport r;
    memset(&r, 0, sizeof(r));
    r.d_pad_pos = N;
    r.ls_x = r.ls_y = 0x80;
    r.rs_x = r.rs_y = 0x80;
    curr_report = r;
}

static void parse_dualsense(uint8_t lx, uint8_t ly, uint8_t rx, uint8_t ry,
                            uint8_t btn0, uint8_t btn1, uint8_t btn2) {
    InputReport r;
    memset(&r, 0, sizeof(r));

    r.ls_x = lx;
    r.ls_y = ly;
    r.rs_x = rx;
    r.rs_y = ry;

    uint8_t hat = btn0 & 0x0f;
    r.d_pad_pos = (hat <= UL) ? (DPad)hat : N;

    r.button_y = (btn0 & 0x10) ? 1 : 0; // Square   -> Y
    r.button_b = (btn0 & 0x20) ? 1 : 0; // Cross    -> B
    r.button_a = (btn0 & 0x40) ? 1 : 0; // Circle   -> A
    r.button_x = (btn0 & 0x80) ? 1 : 0; // Triangle -> X

    r.button_l     = (btn1 & 0x01) ? 1 : 0; // L1
    r.button_r     = (btn1 & 0x02) ? 1 : 0; // R1
    r.button_zl    = (btn1 & 0x04) ? 1 : 0; // L2 click
    r.button_zr    = (btn1 & 0x08) ? 1 : 0; // R2 click
    r.button_minus = (btn1 & 0x10) ? 1 : 0; // Create -> -
    r.button_plus  = (btn1 & 0x20) ? 1 : 0; // Options -> +
    r.button_l3    = (btn1 & 0x40) ? 1 : 0;
    r.button_r3    = (btn1 & 0x80) ? 1 : 0;

    r.button_home    = (btn2 & 0x01) ? 1 : 0; // PS       -> Home
    r.button_capture = (btn2 & 0x02) ? 1 : 0; // Touchpad -> Capture

    // single struct assignment: narrows the window in which hid_task() (main
    // loop) can read a half-updated report built in the btstack context
    curr_report = r;
}

static void handle_hid_report(const uint8_t *report, uint16_t report_len) {
    // BT interrupt-channel HID data is prefixed with 0xa1 (DATA | INPUT)
    if (report_len < 1 || *report != 0xa1) return;
    report++;
    report_len--;

    if (report_len < 1) return;
    uint8_t report_id = report[0];

    if (report_id == 0x31) {
        // [0]=id [1]=seq/flags [2]=LX [3]=LY [4]=RX [5]=RY [6]=L2 [7]=R2
        // [8]=seq# [9]=btn0 [10]=btn1 [11]=btn2
        if (report_len < 12) return;
        parse_dualsense(report[2], report[3], report[4], report[5],
                        report[9], report[10], report[11]);
    } else if (report_id == 0x01) {
        // [0]=id [1]=LX [2]=LY [3]=RX [4]=RY [5]=btn0 [6]=btn1 [7]=btn2
        if (report_len < 8) return;
        parse_dualsense(report[1], report[2], report[3], report[4],
                        report[5], report[6], report[7]);
    }
    // other report ids (0x02 touchpad-only, feature echoes, ...) are ignored
}



static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    (void) channel; (void) size; // unused params
    if (packet_type != HCI_EVENT_PACKET) return; // HCI connection

    uint8_t event_type = hci_event_packet_get_type(packet);
    bd_addr_t event_address;
    uint8_t status;

    switch(event_type) {
        case BTSTACK_EVENT_STATE:

            int state = btstack_event_state_get_state(packet);
            if (state == HCI_STATE_WORKING) {
                // radio done initializing; idle ready state
                gap_inquiry_start(INQUIRY_DURATION); // how many units of time
            }
            break;

        case GAP_EVENT_INQUIRY_RESULT:
            // a result from the scan
            // stop parsing scan results if we've already connected to our 
            // target or scanned the max possible devices

            if (target_found || num_devices_discovered >= MAX_DISCOVERED_DEVICES) break;

            // get this device's addr; checking if we've alr seen it
            bd_addr_t curr_address; // an array of bytes
            gap_event_inquiry_result_get_bd_addr(packet, curr_address);
            // fills in the array of bytes

            
            // uint32_t cod = gap_event_inquiry_result_get_class_of_device(packet);
            // if ((cod & 0x1F00) != 0x0500) break; // only populate game pads

            if(get_device_index(curr_address) >= 0) break; // we stored this alr
            device* d_slot = &discovered_devices[num_devices_discovered++];
            memcpy(d_slot->addr, curr_address, 6);
            d_slot->page_scan_repition_mode = gap_event_inquiry_result_get_page_scan_repetition_mode(packet);
            d_slot->clock_offset = gap_event_inquiry_result_get_clock_offset(packet);


            // handling name state
            // if name matches, try to connect
            if (gap_event_inquiry_result_get_name_available(packet)) {
                char name_buffer[240];
                int name_len = gap_event_inquiry_result_get_name_len(packet);
                name_len = (name_len >= (int) sizeof(name_buffer)) ?
                    sizeof(name_buffer) - 1 : name_len;
                // one bit reserved for a 0 

                memcpy(name_buffer, gap_event_inquiry_result_get_name(packet), name_len);
                name_buffer[name_len] = 0;
                d_slot->name_state = DEVICE_NAME_KNOWN;

                if (name_contains(name_buffer, target_name_substring))
                    connect(curr_address);
            } else {
                d_slot -> name_state = DEVICE_NAME_UNKNOWN;
            }
            break;
        

        case GAP_EVENT_INQUIRY_COMPLETE:
            // scan elapsed full duration
            if (!target_found) continue_discovery();
            break;

        case HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE:
            // response to our name request
            if (target_found) break; // no need to act

            bd_addr_t device_addr;
            reverse_bd_addr(&packet[3], device_addr);

            int idx = get_device_index(device_addr); // which slot?
            uint8_t name_rq_status = packet[2];

            if(idx >= 0 && name_rq_status == ERROR_CODE_SUCCESS) {
                // name successfully retrieved for a device in our array
                char* name = (char*) &packet[9];
                discovered_devices[idx].name_state = DEVICE_NAME_KNOWN;

                if (name_contains(name, target_name_substring)) {
                    // this is our target device, connect to it
                    connect(device_addr);
                    break;
                }

            } else if (idx >= 0) {
                // this is a device in our array but
                // name not found successfully
                discovered_devices[idx].name_state = DEVICE_NAME_KNOWN;
                // we're giving up on trying to find the name
            }

            // if we're here, we know that we ONLY sent a name request,
            // we didn't start a new scan session
            // calling continue_discovery here will either file another name
            // req, or restart the scan
            continue_discovery();
            break;

        case HCI_EVENT_PIN_CODE_REQUEST:
            // tried to pair via pin
            // reject that pin code request

            hci_event_pin_code_request_get_bd_addr(packet, event_address);
            gap_pin_code_negative(event_address); // reject
            break;
        
        case HCI_EVENT_USER_CONFIRMATION_REQUEST:
            // modern confirmationr request to pair
            hci_event_user_confirmation_request_get_bd_addr(packet, event_address);
            gap_ssp_confirmation_response(event_address); // accept and pair
            break;

        case HCI_EVENT_HID_META: // hid specific events
            int subevent_code = hci_event_hid_meta_get_subevent_code(packet);
            switch(subevent_code) {
                
                case HID_SUBEVENT_CONNECTION_OPENED:
                    status = hid_subevent_connection_opened_get_status(packet);
                    if (status != ERROR_CODE_SUCCESS) {
                        // need to rescan
                        target_found = false;
                        app_state = APP_SCANNING;
                        num_devices_discovered = 0; // fill out array again
                        led_signal_error();
                        gap_inquiry_start(INQUIRY_DURATION);
                        break;
                    }
                    // succeeded

                    connection_id = hid_subevent_connection_opened_get_hid_cid(packet);
                    hid_descriptor_available = false;
                    app_state = APP_CONNECTED;
                    break;
                
                case HID_SUBEVENT_DESCRIPTOR_AVAILABLE:
                    status = hid_subevent_descriptor_available_get_status(packet);
                    hid_descriptor_available = (status == ERROR_CODE_SUCCESS);

                    // Kick the DualSense into "full" mode: once the host reads
                    // feature report 0x05 (calibration), the pad switches from
                    // the minimal 0x01 report to the rich 0x31 report that
                    // carries every button. Harmless if it fails.
                    hid_host_send_get_report(connection_id, HID_REPORT_TYPE_FEATURE, 0x05);
                    break;

                case HID_SUBEVENT_GET_REPORT_RESPONSE:
                    // response to the feature-report request above; nothing to
                    // do with the payload, the side effect is what we wanted
                    break;

                case HID_SUBEVENT_REPORT:
                    // parsed by fixed byte offset, so the HID descriptor is not
                    // required (the BT descriptor doesn't describe the buttons)
                    handle_hid_report(
                        hid_subevent_report_get_report(packet),
                        hid_subevent_report_get_report_len(packet)
                    );
                    break;

                case HID_SUBEVENT_SET_PROTOCOL_RESPONSE:
                    // setting which protocol that device will communicate with
                    // need to handshake and agree
                    break;
                
                case HID_SUBEVENT_CONNECTION_CLOSED:
                    connection_id = 0;
                    hid_descriptor_available = false;
                    curr_report_neutral(); // don't leave inputs stuck held

                    // rescan
                    target_found = false;
                    app_state = APP_SCANNING;
                    num_devices_discovered = 0;
                    led_signal_error();
                    gap_inquiry_start(INQUIRY_DURATION);
                    break;
                
                default:
                    break;
            }
            break;
        
        default:
            break;

    }
    
}

void bt_host_setup() {
    l2cap_init();

    // SDP client/server: the hid host uses SDP to query the controller's
    // HID SDP record (the report descriptor + PSMs) after L2CAP comes up
    sdp_init(); // allows us to get HID descriptors

    // require an encrypted link (Security Mode 4 / Level 2). the DualSense
    // will not open the HID channels on an unencrypted connection
    gap_set_security_level(LEVEL_2); // encrypted connection

    // allow bonding: store the link key from SSP pairing so the controller
    // stays paired instead of re-pairing (or failing) on every connect
    gap_set_bondable_mode(1); // remember for future pairing

    hid_host_init(remote_descriptor_storage, sizeof(remote_descriptor_storage));
    hid_host_register_packet_handler(packet_handler);

    // enable sniff mode and role switching; might be needed to connect
    // to the controller
    gap_set_default_link_policy_settings(
        LM_LINK_POLICY_ENABLE_SNIFF_MODE | LM_LINK_POLICY_ENABLE_ROLE_SWITCH
    );

    // the rpi is initiating teh connection
    hci_set_master_slave_policy(HCI_ROLE_MASTER);

    // when scanning nearby devices, they now send their name as well.
    hci_set_inquiry_mode(INQUIRY_MODE_RSSI_AND_EIR);

    // adding our event handler
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
    
}

int bt_hid_init(void) {
    if (cyw43_arch_init()) { // initializing wireless hardware
        printf("cyw43_arch_init() failed.\n");
        return -1;
    }

    bt_host_setup();
    led_status_start(); // LED now reflects app_state / errors
    hci_power_control(HCI_POWER_ON);
    return 0;
}