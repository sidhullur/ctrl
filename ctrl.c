#include "bsp/board_api.h"
#include "tusb.h"
#include <stdint.h>

#define PACKET_LEN 64
#define REPORT_INTERVAL_MS 8

// --------------------- Used purely for subcommand handling

// 1. Factory Analog Stick Calibration (Address 0x603D, 18 bytes)
// Standard center (0x800), min (0x100), max (0xF00), deadzone (~0x064)
static const uint8_t factory_stick_cal[18] = {
    // Left Stick: Max X/Y, Center X/Y, Min X/Y
    0xE0, 0x07, 0x7E, 0x00, 0x08, 0x80, 0x00, 0x00, 0x60,
    // Right Stick: Center X/Y, Min X/Y, Max X/Y
    0x00, 0x08, 0x80, 0x00, 0x00, 0x60, 0xE0, 0x07, 0x7E
};

// 2. Factory 6-Axis IMU Sensor Calibration (Address 0x6020, 24 bytes)
static const uint8_t factory_imu_cal[24] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Accel Origin (X, Y, Z)
    0x00, 0x40, 0x00, 0x40, 0x00, 0x40, // Accel Sens: ±8G sensitivity
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Gyro Origin (X, Y, Z)
    0x3B, 0x34, 0x3B, 0x34, 0x3B, 0x34  // Gyro Sens: ±2000dps sensitivity
};

// 3. Controller Body / Button Colors (Address 0x6050, 13 bytes)
static const uint8_t body_colors[13] = {
    0x32, 0x32, 0x32, // Body: Dark Charcoal (#323232)
    0xFF, 0xFF, 0xFF, // Buttons: White (#FFFFFF)
    0x20, 0x20, 0x20, // Left Grip (#202020)
    0x20, 0x20, 0x20, // Right Grip (#202020)
    0x01              // 0x01 = Custom colors active
};

// -----------------------------------------------------------------

void spi_flash_read(uint32_t fullAddr, uint8_t length, uint8_t *destBuffer) {
    // fullAddr indicates what type of info is requested, needs to be filled in
    switch(fullAddr) {

        case 0x6020: // imu motion calibration requested
            if (length <= sizeof(factory_imu_cal))
                memcpy(destBuffer, factory_imu_cal, length);
            break;
        
        case 0x603D: // factory stick calibration requested
            if (length <= sizeof(factory_stick_cal))
                memcpy(destBuffer, factory_stick_cal, length);
            break;
        
        case 0x6050: // colors requested
            if (length <= sizeof(body_colors))
                memcpy(destBuffer, body_colors, length);
            break;
        
        default:
            break;

    }
}

// volatile provides compiler optimizations
static volatile bool srResponsePending = false;
static uint8_t pendingResponse[64];
// already global, but declaring static is a good practice for the linker
// exposed only within the file; global vars in other files can have same name

typedef enum {
    CONTROLLER_STATE_HANDSHAKE,
    CONTROLLER_STATE_SUBCOMMAND_INIT,
    CONTROLLER_STATE_STREAM_INPUT
} ControllerState;

static volatile ControllerState global_state;
static volatile uint8_t global_counter = 0;
struct InputPacket {
    uint8_t reportID; // always 0x30
    uint8_t sequenceID; // increment by one for each packet
    uint8_t connectionBattery; // 4 bytes connection, 4 bytes battery
    uint8_t rightButtons; // ABXY
    uint8_t middleButtons; // Home, Share
    uint8_t leftButtons; // D-pad
    uint8_t leftStick[3]; // XYZ (or something like that)
    uint8_t rightStick[3];
    int  padding[13]; // all 0s
};

void A_buttonSpam(struct InputPacket* packet) {
    memset(packet, 0, PACKET_LEN); 

    packet->reportID = 0x30;
    packet->sequenceID = global_counter;
    packet->connectionBattery = 0x91; //replace later
    packet->rightButtons = 0x08; // A button spam

    packet->leftStick[1] = 0x08;
    packet->leftStick[2] = 0x80;

    packet->rightStick[1] = 0x08;
    packet->rightStick[2] = 0x80;
}

void hid_task(void) {
    if (!tud_hid_ready()) return;

    if (srResponsePending) {
        tud_hid_report(0, &pendingResponse, PACKET_LEN);
        srResponsePending = false;
        return;
    }

    if (global_state != CONTROLLER_STATE_STREAM_INPUT) return; 

    static uint32_t last_report_time = 0;
    uint32_t current_time = board_millis();

    if (current_time - last_report_time < REPORT_INTERVAL_MS) return;
    last_report_time = current_time; // send now

    struct InputPacket packet;
    A_buttonSpam(&packet);
    global_counter += 1;

    tud_hid_report(0, &packet, PACKET_LEN);
}


// host asks about report state - switch never calls this
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                hid_report_type_t report_type, uint8_t* buffer,
                                uint16_t reqlen) {return 0;}

// host trying to set device state
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                            hid_report_type_t report_type, uint8_t const* buffer,
                            uint16_t bufsize) 
{
    if (report_type ==  HID_REPORT_TYPE_OUTPUT) {
        // can clear it, since switch only sends next SR after last one
        // got a response.

        memset(&pendingResponse, 0, PACKET_LEN);
        switch(buffer[0]) { 

            // ---------- HANDSHAKE PHASE ------------
            case 0x80:

                pendingResponse[0] = 0x81;

                switch(buffer[1]) {
                    case 0x01:
                        pendingResponse[1] = 0x01;
                        // pendingResponse[2] = 0x00;
                        pendingResponse[3] = 0x03;
                        
                        // little endian mac address
                        pendingResponse[4] = 0x33;
                        pendingResponse[5] = 0x22;
                        pendingResponse[6] = 0x11;
                        pendingResponse[7] = 0x8A;
                        pendingResponse[8] = 0xBB;
                        pendingResponse[9] = 0x7C;

                        tud_hid_report(0, &pendingResponse, PACKET_LEN);
                        break;
                    
                    case 0x02:
                        pendingResponse[1] = 0x02;

                        tud_hid_report(0, &pendingResponse, PACKET_LEN);
                        break;
                    
                    case 0x03:
                        pendingResponse[1] = 0x03;

                        tud_hid_report(0, &pendingResponse, PACKET_LEN);
                        break;
                    
                    case 0x04:
                    
                        global_state = CONTROLLER_STATE_SUBCOMMAND_INIT;
                        break;
                }
                break;
            // -----------------------------------------
            
            case 0x01:

                pendingResponse[0] = 0x21;
                pendingResponse[1] = global_counter;
                pendingResponse[2] = 0x91; // batter + conn

                pendingResponse[7] = 0x08; // neutral stick
                pendingResponse[8] = 0x80;

                pendingResponse[10] = 0x08; // newutral stick
                pendingResponse[11] = 0x80;
                pendingResponse[12] = 0x80; // vibrator ack (standard)

                pendingResponse[13] = 0x80 | buffer[10]; // 0x80 | subcommand
                pendingResponse[14] = buffer[10]; // subcommand


                switch(buffer[10]) {
                    // ---------- SUBCMD INIT PHASE ------------
                    case 0x02:

                        pendingResponse[15] = 0x03; // firmware (standard)
                        pendingResponse[16] = 0x48;

                        pendingResponse[17] = 0x03; // device type (standard)
                        pendingResponse[18] = 0x02;

                        // mac address little endian
                        pendingResponse[19] = 0x7C;
                        pendingResponse[20] = 0xBB;
                        pendingResponse[21] = 0x8A;
                        pendingResponse[22] = 0x11;
                        pendingResponse[23] = 0x22;
                        pendingResponse[24] = 0x33;

                        pendingResponse[25] = 0x01; // color and factory loc
                        pendingResponse[26] = 0x01; // (standard)

                        tud_hid_report(0, &pendingResponse, PACKET_LEN);
                        break;
                    
                    case 0x08:
                        // TODO: TURN ON LOW POWER STATE

                        tud_hid_report(0, &pendingResponse, PACKET_LEN);
                        break;
                    
                    // -------------------------------------------------------

                    // ---------- PARSE EVEN DURING INPUT STREAM MODE ---------

                    case 0x10:
                        // addr given in msg
                        pendingResponse[15] = buffer[11];
                        pendingResponse[16] = buffer[12];
                        pendingResponse[17] = buffer[13];
                        pendingResponse[18] = buffer[14];

                        uint32_t fullAddr = (buffer[14] << 24) | 
                        (buffer[13] << 16) | (buffer[12] << 8) | buffer[11];

                        pendingResponse[19] = buffer[15]; // read len from msg
                        spi_flash_read(
                            fullAddr, 
                            buffer[15], 
                            &pendingResponse[20]
                        );

                        if (global_state == CONTROLLER_STATE_SUBCOMMAND_INIT) {
                            tud_hid_report(0, &pendingResponse, PACKET_LEN);
                        } else {
                            srResponsePending = true;
                        }

                        // queue the packet if in input stream mode
                        // otherwise send immediately
                        break;

                    case 0x40:
                        // TODO: TURN ON INTERNAL GYRO/ACCELEROMETER

                        // queue the packet if in input stream mode
                        // otherwise send immediately

                        if (global_state == CONTROLLER_STATE_SUBCOMMAND_INIT) {
                            tud_hid_report(0, &pendingResponse, PACKET_LEN);
                        } else {
                            srResponsePending = true;
                        }

                        break;
                    
                    case 0x48:
                        // TODO: ENABLE VIBRATION

                        // queue the packet if in input stream mode
                        // otherwise send immediately

                        if (global_state == CONTROLLER_STATE_SUBCOMMAND_INIT) {
                            tud_hid_report(0, &pendingResponse, PACKET_LEN);
                        } else {
                            srResponsePending = true;
                        }

                        break;

                    case 0x03:

                        // queue the packet if in input stream mode
                        // otherwise send immediately

                        // tud_hid_report(0, &pendingResponse, PACKET_LEN);
                        global_state = CONTROLLER_STATE_STREAM_INPUT;
                        break;

                    case 0x00:
                        // TODO: TRIGGER VIBRATION (RUMBLE)
                        // NO RESPONSE SENT/QUEUED
                        break;
                    
                    case 0x30:
                        // TODO: SET PLAYER LED
                        break;

                    case 0x38:
                        // TODO: Set HOME Button LED
                        break;
                    
                    case 0x04:
                        // TODO: Trigger Elapsed Time
                        break;
                    
                }
            break;
        }
            
    }
}

int main(void)
{
    state = HANDSHAKE;
    board_init();
    tusb_init();

    while(1) {
        tud_task(); // tinyUSB method for processing USB events
        hid_task(); // user defined
    }
}
