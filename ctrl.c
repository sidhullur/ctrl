#include "bsp/board_api.h"
#include "tusb.h"
#include <stdint.h>

void hid_task(void);

struct controllerPacket {
    uint8_t reportID;
    uint8_t sequenceID;
    uint8_t connectionBattery;
    uint8_t rightButtons;
    uint8_t middleButtons;
    uint8_t leftButtons;
    uint8_t leftStick[3];
    uint8_t rightStick[3];
    int  padding[13];
};

int main(void)
{
    board_init();
    tusb_init();

    while(1) {
        tud_task(); // tinyUSB method for processing USB events
        hid_task(); // user defined
    }
}

void A_buttonSpam(struct controllerPacket* packet, uint8_t counter) {
    packet->reportID = 0x30;
    packet->sequenceID = counter;
    packet->connectionBattery = 0x91; //replace later
    packet->rightButtons = 0x08; // A button spam
    packet->middleButtons = 0x00;
    packet->leftButtons = 0x00;

    packet->leftStick[0] = 0x00;
    packet->leftStick[1] = 0x08;
    packet->leftStick[2] = 0x80;

    packet->rightStick[0] = 0x00;
    packet->rightStick[1] = 0x08;
    packet->rightStick[2] = 0x80;
}

void hid_task(void) {

    const uint32_t interval_ms = 10;
    static uint32_t start_ms = 0;

    if (board_millis() - start_ms < interval_ms) return;
    start_ms += interval_ms;

    if (!tud_hid_ready()) return; 

    static uint8_t counter = 0;

    struct controllerPacket packet;
    memset(&packet, 0, sizeof(packet)); 
    A_buttonSpam(&packet, counter);
    counter += 1;

    tud_hid_report(0, &packet, 64);
}
