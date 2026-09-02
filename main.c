#include "pico/stdlib.h"
#include "tusb.h"
#include <stdint.h>
#include <string.h>

#include "bt_hid_bridge.h"

#define REPORT_INTERVAL 5
#define STICK_CENTER 0x80

static void neutral_report(void) {
    memset(&curr_report, 0, sizeof(curr_report));
    curr_report.d_pad_pos = N;
    curr_report.ls_x = curr_report.ls_y = STICK_CENTER;
    curr_report.rs_x = curr_report.rs_y = STICK_CENTER;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                hid_report_type_t report_type, uint8_t* buffer,
                                uint16_t reqlen) {
    (void) instance; (void) report_id; (void) report_type;
    uint16_t len = sizeof(curr_report);
    if (len > reqlen) len = reqlen;
    memcpy(buffer, &curr_report, len);
    return len;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                            hid_report_type_t report_type,
                            uint8_t const* buffer, uint16_t bufsize) {}

void hid_task(void) {
    if (!tud_mounted()) return; 

    static uint32_t last_report = 0;
    uint32_t now = to_ms_since_boot(get_absolute_time());

    if (now - last_report < REPORT_INTERVAL ||
        !tud_hid_ready()) return;
    
    last_report = now;

    // fill_input_report(&curr_report);
    if (tud_suspended()) tud_remote_wakeup();
    tud_hid_report(0, &curr_report, sizeof(curr_report));
}

int main(void)
{
    stdio_init_all();

    neutral_report(); // sane state until the first BT report lands

    bt_hid_init();
    tusb_init();

    while (1) {
        tud_task();
        hid_task();
    }
}
