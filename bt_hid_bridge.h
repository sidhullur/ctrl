// place all BT methods that we want to expose to ctrl.c here.
#ifndef BT_HID_BRIDGE_H
#define BT_HID_BRIDGE_H

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

typedef enum {U, UR, R, DR, D, DL, L, UL, N} DPad;

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

extern InputReport curr_report;
int bt_hid_init(void);

#endif
