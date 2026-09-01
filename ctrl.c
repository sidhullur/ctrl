//#include "bsp/board_api.h"
#include "pico/stdlib.h"
#include "tusb.h"
#include <stdint.h>
#include <string.h>

#define REPORT_INTERVAL 5
#define STICK_CENTER 0x80

typedef enum {
    U, UR, R, DR, D, DL, L, UL, N
} DPad;
typedef struct __attribute__((packed)) {
    unsigned int button_y : 1;
    unsigned int button_b : 1;
    unsigned int button_a : 1;
    unsigned int button_x : 1;
    unsigned int button_l : 1;
    unsigned int button_r : 1;
    unsigned int button_zl : 1;
    unsigned int button_zr : 1;
    unsigned int button_minus : 1;
    unsigned int button_plus : 1;
    unsigned int button_l3 : 1;
    unsigned int button_r3 : 1;
    unsigned int button_home : 1;
    unsigned int button_capture : 1;
    unsigned int padding : 2;

    DPad d_pad_pos : 8;
    
    uint8_t ls_x, ls_y;
    uint8_t rs_x, rs_y;

    uint8_t vendor;
} InputReport;
_Static_assert(sizeof(InputReport) == 8, "InputReport must be 8 bytes");

static InputReport curr_report;

// Some hosts (and possibly the Switch during setup) issue a control-pipe
// GET_REPORT before they start polling the interrupt IN endpoint. Returning
// 0 makes TinyUSB STALL it; answer with the current 8-byte report instead.
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                hid_report_type_t report_type, uint8_t* buffer,
                                uint16_t reqlen) {
    (void) instance; (void) report_id; (void) report_type;
    uint16_t len = sizeof(curr_report);
    if (len > reqlen) len = reqlen;
    memcpy(buffer, &curr_report, len);
    return len;
}

// No OUT endpoint; console rumble (if any) would arrive here via EP0. Ignore.
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                            hid_report_type_t report_type,
                            uint8_t const* buffer, uint16_t bufsize) {}

void fill_input_report(InputReport* report) {
    memset(report, 0, sizeof(curr_report));

    report->button_b = 1;
    report->d_pad_pos = N;
    report->ls_x = 0xFF;
    report-> ls_y = report->rs_x = report->rs_y = STICK_CENTER;
}

void hid_task(void) {
    static uint32_t last_report = 0;
    uint32_t now = to_ms_since_boot(get_absolute_time());

    if (now - last_report < REPORT_INTERVAL ||
        !tud_hid_ready()) return;
    
    last_report = now;

    fill_input_report(&curr_report);
    tud_hid_report(0, &curr_report, sizeof(curr_report));
}

int main(void)
{
    stdio_init_all();
    tusb_init();

    while(1) {
        tud_task(); // tinyUSB method for processing USB events
        hid_task(); // user defined
    }
}
