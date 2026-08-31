//#include "bsp/board_api.h"
#include "pico/stdlib.h"
#include "tusb.h"
#include <stdint.h>

// Never called by the console.
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                hid_report_type_t report_type, uint8_t* buffer,
                                uint16_t reqlen) {return 0;}

// Effectively useless.
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                            hid_report_type_t report_type,
                            uint8_t const* buffer, uint16_t bufsize) {}

void hid_task(void) {}

int main(void)
{
    stdio_init_all();
    tusb_init();

    while(1) {
        tud_task(); // tinyUSB method for processing USB events
        hid_task(); // user defined
    }
}
