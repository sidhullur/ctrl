//#include "bsp/board_api.h"
#include "pico/stdlib.h"
#include "tusb.h"
#include <stdint.h>
#include "spi_flash_data.h"

#define PACKET_LEN 64
#define REPORT_INTERVAL_MS 17 // change later
#define SUBCMD_RID 0x21
#define INPUT_RID 0x30
#define HANDSHAKE_RESPONSE_RID 0x81
#define SPI_READ_MAX_LEN 0x1D // protocol cap on bytes per SPI flash read

static bool sr_pending = false;
static uint8_t sr_response[63]; // first byte is report ID
static uint8_t pending_rid = 0;
typedef enum {
    CONTROLLER_STATE_HANDSHAKE,
    CONTROLLER_STATE_SUBCMD_INIT,
    CONTROLLER_STATE_STREAM_INPUT
} ControllerState;

// typedef enum {
//     INPUT_MODE_STANDARD,//
//     INPUT_MODE_HID
// } InputMode;

typedef struct __attribute__((packed)){
    uint8_t timer;
    uint8_t battery_con;

    uint8_t button_status[3];
    uint8_t left_stick[3];
    uint8_t right_stick[3];
    uint8_t vibrator_byte;

    uint8_t imu_data[36];
    uint8_t padding[15];

} inputPacket;

static ControllerState global_state = CONTROLLER_STATE_HANDSHAKE;
static uint8_t global_counter = 0;
// static InputMode global_input_mode = INPUT_MODE_STANDARD;

void set_controller_haptics(const uint8_t* rumble_data) {
    // TODO: read 8 bytes in a row and control actual hardware
}

void set_controller_LEDs(const uint8_t* led_data) {
    // TODO: read this singular byte and set the controller's LEDs
}

void handle_handshake(const uint8_t* buffer) {
    // CONTROLLER_STATE_HANDSHAKE - send immediately, don't queue
    pending_rid = HANDSHAKE_RESPONSE_RID;
    sr_response[0] = buffer[1];

    // skipped assignments are already 0x00
    switch(buffer[1]) {
        case 0x01:
            sr_response[2] = 0x03;

            sr_response[3] = mac_addr[5];
            sr_response[4] = mac_addr[4];
            sr_response[5] = mac_addr[3];
            sr_response[6] = mac_addr[2];
            sr_response[7] = mac_addr[1];
            sr_response[8] = mac_addr[0];
            // sr_response[3] = mac_addr[0];
            // sr_response[4] = mac_addr[1];
            // sr_response[5] = mac_addr[2];
            // sr_response[6] = mac_addr[3];
            // sr_response[7] = mac_addr[4];
            // sr_response[8] = mac_addr[5];

            sr_pending = true;
            break;
        
        case 0x04:

            global_state = CONTROLLER_STATE_SUBCMD_INIT;
            sr_pending = true;
            break;
        
        case 0x05:

            global_state = CONTROLLER_STATE_HANDSHAKE;
            sr_pending = true;
            break;
        
        // just ack
        case 0x02:
        case 0x03:
            sr_pending = true;
            break;

        // case 0x06:
        //     break;
        
        default: // 91/92 ignored
            break;
    }

    // tud_hid_report(HANDSHAKE_RESPONSE_RID, &sr_response, sizeof(sr_response));
}

void toggle_imu(const uint8_t choice) {
    // TODO: Allow IMU values from controller to pass through to the system
}

void toggle_vibration(const uint8_t choice) {
    // TODO: Globally decides whether vibration data (sent from console)
    // should pass through to the physical controller.
}

void fill_subcommand_response_payload(
    uint8_t* ackSlot, uint8_t* payload_buffer, const uint8_t* input_packet) {
    // srPending set to true immediately after this is called.

    *ackSlot = 0x80;

    switch(input_packet[10]) { // subcommand

        case 0x02:
            *ackSlot = 0x82;

            payload_buffer[0] = 0x03;
            payload_buffer[1] = 0x48;

            payload_buffer[2] = 0x03;

            payload_buffer[3] = 0x02;

            payload_buffer[4] = mac_addr[0];
            payload_buffer[5] = mac_addr[1];
            payload_buffer[6] = mac_addr[2];
            payload_buffer[7] = mac_addr[3];
            payload_buffer[8] = mac_addr[4];
            payload_buffer[9] = mac_addr[5];

            payload_buffer[10] = 0x01;
            payload_buffer[11] = 0x01;
            break;
        
        case 0x03:
            // switch(input_packet[11]) { // input type
            //     case 0x30:
            //         global_input_mode = INPUT_MODE_STANDARD;
            //         break;
                
            //     case 0x3F:
            //         global_input_mode = INPUT_MODE_HID;
            //         break;
            // }
            if (input_packet[11] == 0x30) {
                global_state = CONTROLLER_STATE_STREAM_INPUT;
            }
            break;
        
        case 0x10:
            *ackSlot = 0x90;

            // Clamp to the protocol max so a malformed request can't write
            // past sr_response (payload_buffer is &sr_response[14], data
            // lands at &payload_buffer[5]; only 44 bytes remain there).
            uint8_t size = input_packet[15];
            if (size > SPI_READ_MAX_LEN) size = SPI_READ_MAX_LEN;

            payload_buffer[0] = input_packet[11];
            payload_buffer[1] = input_packet[12];
            payload_buffer[2] = input_packet[13];
            payload_buffer[3] = input_packet[14];

            payload_buffer[4] = size;

            uint32_t base_addr = (input_packet[11]) | (input_packet[12] << 8) |
            (input_packet[13] << 16) | (input_packet[14] << 24);

            spi_flash_read(base_addr, size, &payload_buffer[5]);
            break;

        case 0x30:
            set_controller_LEDs(&input_packet[11]);
            break;
        
        case 0x40:
            toggle_imu(input_packet[11]);
            break;
        
        case 0x48:
            toggle_vibration(input_packet[11]);
            break;
        
        case 0x04:
            // TODO: Implement later
            break;

        case 0x05:
            // TODO: Implement Maybe
            *ackSlot = 0x83;
            break;
        
        case 0x31:
            *ackSlot = 0xB0;
            break;

        case 0x43:
            *ackSlot = 0xC0;
            break;
        
        case 0x06:
        case 0x07:
        case 0x11:
        case 0x12:
        case 0x41:
        case 0x42:
            // TODO: Implement later
            break;
        
        case 0x08:
        default:
            break;

    }
}

uint8_t get_battery_con() {
    // TODO: Implement real battery logging
    // second digit will always be 1.
    return 0x91;
}

void set_button_status(uint8_t* buffer) {
    // TODO: sets three bytes from controller
    buffer[0] = 0x08; // A button pressed
    buffer[1] = 0x00;
    buffer[2] = 0x00;
}
void set_left_stick_status(uint8_t* buffer) {
    // TODO: sets three bytes from controller
    buffer[0] = 0x00; // neutral
    buffer[1] = 0x08;
    buffer[2] = 0x80;
}
void set_right_stick_status(uint8_t* buffer) {
    // TODO: sets three bytes from controller
    buffer[0] = 0x00; // neutral
    buffer[1] = 0x08;
    buffer[2] = 0x80;
}

void handle_rumble(const uint8_t* input_packet, uint8_t report_type) {

    set_controller_haptics(&input_packet[2]);

    switch(report_type) {
        case 0x10: // no response needed
            break;

        case 0x01:
            pending_rid = SUBCMD_RID;

            sr_response[0] = global_counter++;
            sr_response[1] = get_battery_con(); // SET BATTERY CON;

            set_button_status(&sr_response[2]);
            set_left_stick_status(&sr_response[5]);
            set_right_stick_status(&sr_response[8]);

            sr_response[11] = 0x80;
            sr_response[13] = input_packet[10]; // echo subcommand
            // byte 12 is the ack format that varies -> filled in handler

            fill_subcommand_response_payload(
                &sr_response[12], &sr_response[14], input_packet);
            sr_pending = true;
            break;
    }
}

void fill_input_packet(inputPacket* packet) {
    // takes controller input and fills packet accordingly.
    // for now, just spam A.

    memset(packet, 0, sizeof(inputPacket));

    packet->timer = global_counter++;
    packet->battery_con = get_battery_con();
    
    packet->button_status[0] = 0x08;

    set_left_stick_status(&(*packet->left_stick));
    set_right_stick_status(&(*packet->right_stick));

    packet->vibrator_byte = 0x80; // constant
}

// host asks about report state - switch never calls this
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                hid_report_type_t report_type, uint8_t* buffer,
                                uint16_t reqlen) {return 0;}

// host trying to set device state
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                            hid_report_type_t report_type,
                            uint8_t const* buffer, uint16_t bufsize) 
{
    //if (report_type != HID_REPORT_TYPE_OUTPUT) return;
    if (report_type != HID_REPORT_TYPE_OUTPUT && 
        report_type != HID_REPORT_TYPE_INVALID) return;

    if (!sr_pending)
        memset(&sr_response, 0, sizeof(sr_response));

    switch(buffer[0]) {
        case 0x80:
            handle_handshake(buffer);
            break;
        
        case 0x01:
        case 0x10:
            handle_rumble(buffer, buffer[0]);
            break;
        
        default:
            break;
    }
}

void hid_task(void) {
    if (!tud_hid_ready()) return; // line ready to accept new input

    if (sr_pending) { // prioritize subcommand responses
        tud_hid_report(pending_rid, &sr_response, sizeof(sr_response));
        sr_pending = false;
        return;
    }

    if (global_state != CONTROLLER_STATE_STREAM_INPUT) return; 

    static uint32_t last_report_time = 0;
    uint32_t current_time = to_ms_since_boot(get_absolute_time());

    if (current_time - last_report_time < REPORT_INTERVAL_MS) return;
    last_report_time = current_time; // send now

    inputPacket packet;
    fill_input_packet(&packet);

    tud_hid_report(INPUT_RID, &packet, sizeof(packet));
}

int main(void)
{
    //board_init();
    stdio_init_all();
    tusb_init();

    while(1) {
        tud_task(); // tinyUSB method for processing USB events
        hid_task(); // user defined
    }
}
