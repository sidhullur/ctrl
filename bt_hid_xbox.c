#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "btstack.h"
#include "btstack_config.h"

#define REMOTE_DEVICE_DESCRIPTOR_BUFSIZE 512

#include "bt_hid_bridge.h"

static uint8_t remote_descriptor_storage[REMOTE_DEVICE_DESCRIPTOR_BUFSIZE];
InputReport curr_report;

// tracking current BT status
typedef enum { APP_SCANNING, APP_CONNECTING, APP_CONNECTED } app_state_t;
static app_state_t app_state = APP_SCANNING;

static const char *target_name_substring = "Xbox";
static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;

uint16_t connection_id = 0;
static bool hid_descriptor_available = false;

static void scan() {
    // HAVE TO CALL gap_stop_scan() later
    app_state = APP_SCANNING;
    gap_set_scan_parameters(1, 48, 48);
    gap_start_scan();
}

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
static bool get_device_name(char* buffer, uint8_t* packet, uint8_t bufsize) {
    const uint8_t* advertisement = gap_event_advertising_report_get_data(packet);
    uint8_t ad_len = gap_event_advertising_report_get_data_length(packet);

    ad_context_t context;

    for(ad_iterator_init(&context, ad_len, advertisement);
        ad_iterator_has_more(&context);
        ad_iterator_next(&context)
    ) {
        uint8_t data_type = ad_iterator_get_data_type(&context);
        uint8_t data_len = ad_iterator_get_data_len(&context);
        const uint8_t* data = ad_iterator_get_data(&context);

        if (data_type == 0x09) {
            // leave one space for null char
            int num_to_copy = (data_len < bufsize) ? data_len : (bufsize - 1);

            memcpy(buffer, data, num_to_copy);
            buffer[num_to_copy] = '\0';
            return true;
        }
    }
    return false;
}


// released-everything state (sticks centred, dpad neutral)
static void curr_report_neutral(void) {
    InputReport r;
    memset(&r, 0, sizeof(r));
    r.d_pad_pos = N;
    r.ls_x = r.ls_y = 0x80;
    r.rs_x = r.rs_y = 0x80;
    curr_report = r;
}

static void gatt_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    (void) channel; (void) size; // unused params
    if (packet_type != HCI_EVENT_PACKET) return; // HCI connection

    uint8_t event_type = hci_event_packet_get_type(packet);
    bd_addr_t event_address;
    uint8_t status;

    switch(event_type) {
        case BTSTACK_EVENT_STATE:

            int state = btstack_event_state_get_state(packet);
            if (state == HCI_STATE_WORKING) scan();
            break;

        case GAP_EVENT_ADVERTISING_REPORT:
            char name_buffer[240];
            bool res = get_device_name(name_buffer, packet, sizeof(name_buffer));

            if (res) {
                if (name_contains(name_buffer, target_name_substring)) {
                    bd_addr_t dev_addr;
                    bd_addr_type_t dev_addr_type;

                    gap_event_advertising_report_get_address(packet, dev_addr);
                    dev_addr_type = gap_event_advertising_report_get_address_type(packet);

                    gap_stop_scan();
                    app_state = APP_CONNECTING;
                    if (gap_connect(dev_addr, dev_addr_type) != ERROR_CODE_SUCCESS) scan();
                }
            }
            break;

        case HCI_EVENT_META_GAP:
            switch (hci_event_gap_meta_get_subevent_code(packet)) {
                case GAP_SUBEVENT_LE_CONNECTION_COMPLETE:
                    if (gap_subevent_le_connection_complete_get_status(packet) != ERROR_CODE_SUCCESS) scan();
                    else {
                        gap_stop_scan();
                        app_state = APP_CONNECTING;

                        hci_con_handle_t handler = hci_subevent_le_connection_complete_get_connection_handle(packet);
                        sm_request_pairing(handler);
                        break;
                    }

                default:
                    break;
            }
            break;
        
        case GATT_EVENT_NOTIFICATION:
            // uint16_t value_handle = gatt_event_notification_get_value_handle(packet);
            // uint16_t length       = gatt_event_notification_get_value_length(packet);
            // const uint8_t *report = gatt_event_notification_get_value(packet);
            
            
            break;
        
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            scan();
            break;    

        default:
            break;

    }
    
}

static void sm_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    (void) channel; (void) size; // suppressing unused params
    if (packet_type != HCI_EVENT_PACKET) return;

    uint8_t event_type = hci_event_packet_get_type(packet);

    switch(event_type) {
        case SM_EVENT_JUST_WORKS_REQUEST:
            hci_con_handle_t handler = sm_event_just_works_request_get_handle(packet);
            sm_just_works_confirm(handler);
            break;
        
        case SM_EVENT_PAIRING_COMPLETE:
            uint8_t status = sm_event_pairing_complete_get_status(packet);
            if (status != ERROR_CODE_SUCCESS) scan();
            else {
                app_state = APP_CONNECTING;
                gap_stop_scan();
            }
        
        default:
            break;
    }
}

void bt_host_setup() {
    l2cap_init();

    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);

    gatt_client_init();

    // adding our event handler
    hci_event_callback_registration.callback = &gatt_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_BONDING);

    sm_event_callback_registration.callback = &sm_event_handler;
    sm_add_event_handler(&sm_event_callback_registration);
    
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