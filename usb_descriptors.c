/*
 * usb_descriptors.c
 *
 * TinyUSB descriptors for a device that enumerates as a HORI Pokken
 * Tournament DX Pro Pad (VID 0x0F0D / PID 0x0092) -- a Nintendo-licensed
 * wired USB controller on the Switch's accepted-controller whitelist.
 *
 * Matched to OGX-Mini's verified SwitchWired.h
 * (Firmware/RP2040/src/Descriptors/SwitchWired.h), reported working on
 * real Switch hardware. The Switch fingerprints more than VID/PID, and a
 * PC will accept almost anything -- so copy a known-good pad exactly.
 *
 * Reference values:
 *   device:  bcdUSB 0x0200, class 0/0/0, bMaxPacketSize0 64,
 *            VID 0x0F0D, PID 0x0092, bcdDevice 0x0100, no serial string
 *   config:  wTotalLength 34, 1 interface, bmAttributes 0xA0
 *            (remote wakeup), MaxPower 500 mA (0xFA)
 *   iface:   HID class 3, sub 0, proto 0, bNumEndpoints 1
 *   HID:     bcdHID 0x0111, country 0, report descriptor length 86 (0x56)
 *   EP IN:   0x81 interrupt / 64 / bInterval 1   (NO OUT endpoint)
 *
 * This profile needs NO handshake, NO subcommands, NO SPI-flash emulation
 * and NO crypto. Keep a fresh 8-byte input report queued via
 * tud_hid_report(0, ...). Tradeoffs: no rumble, no gyro/motion, no
 * amiibo/NFC, no analog triggers (ZL/ZR are digital). HOME/Capture work.
 *
 * Requires in tusb_config.h:
 *   #define CFG_TUD_HID              1
 *   #define CFG_TUD_HID_EP_BUFSIZE   64   (must be >= 8)
 */

#include <string.h>
#include "tusb.h"

#define USB_VID   0x0F0D   // HORI CO., LTD.
#define USB_PID   0x0092   // Pokken Tournament DX Pro Pad

//--------------------------------------------------------------------+
// Device Descriptor
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,

    // HID class is declared at the *interface*, not here.
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,   // GP2040-CE / real Pokken pad

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x00,     // no serial string (matches real device)

    .bNumConfigurations = 0x01
};

// Invoked when GET DEVICE DESCRIPTOR is received
uint8_t const * tud_descriptor_device_cb(void)
{
    return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// HID Report Descriptor
//
// EXACT copy of GP2040-CE's switch_report_descriptor (86 bytes / 0x56).
// Do not "tidy" it -- the compressed form (relying on Report Size / Usage
// Page state carrying across items) is what the real pad sends, and the
// Switch is believed to hash/length-check this blob.
//
// Layout of the 8-byte INPUT report it describes:
//   byte 0..1  16 buttons (bit0 = Y; full map in ctrl.c)
//   byte 2     low nibble = D-pad hat (0..7 dir, 8 = neutral); high nibble pad
//   byte 3     left  stick X   (0x00..0xFF, 0x80 = center)
//   byte 4     left  stick Y
//   byte 5     right stick X
//   byte 6     right stick Y
//   byte 7     vendor byte (0x00 filler)
// Plus an 8-byte OUTPUT report (rumble/LED) that we accept and ignore.
//--------------------------------------------------------------------+
uint8_t const desc_hid_report[] =
{
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x05,        // Usage (Game Pad)
    0xA1, 0x01,        // Collection (Application)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x35, 0x00,        //   Physical Minimum (0)
    0x45, 0x01,        //   Physical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x10,        //   Report Count (16)
    0x05, 0x09,        //   Usage Page (Button)
    0x19, 0x01,        //   Usage Minimum (Button 1)
    0x29, 0x10,        //   Usage Maximum (Button 16)
    0x81, 0x02,        //   Input (Data,Var,Abs)      -> bytes 0..1 (16 buttons)
    0x05, 0x01,        //   Usage Page (Generic Desktop)
    0x25, 0x07,        //   Logical Maximum (7)
    0x46, 0x3B, 0x01,  //   Physical Maximum (315)
    0x75, 0x04,        //   Report Size (4)
    0x95, 0x01,        //   Report Count (1)
    0x65, 0x14,        //   Unit (Degrees)
    0x09, 0x39,        //   Usage (Hat switch)
    0x81, 0x42,        //   Input (Data,Var,Abs,Null) -> byte 2 low nibble (hat)
    0x65, 0x00,        //   Unit (None)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x01,        //   Input (Const)             -> byte 2 high nibble pad
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x46, 0xFF, 0x00,  //   Physical Maximum (255)
    0x09, 0x30,        //   Usage (X)   left  stick X
    0x09, 0x31,        //   Usage (Y)   left  stick Y
    0x09, 0x32,        //   Usage (Z)   right stick X
    0x09, 0x35,        //   Usage (Rz)  right stick Y
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x04,        //   Report Count (4)
    0x81, 0x02,        //   Input (Data,Var,Abs)      -> bytes 3..6 (sticks)
    0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
    0x09, 0x20,        //   Usage (0x20)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x02,        //   Input (Data,Var,Abs)      -> byte 7 (vendor)
    0x0A, 0x21, 0x26,  //   Usage (0x2621)
    0x95, 0x08,        //   Report Count (8)
    0x91, 0x02,        //   Output (Data,Var,Abs)     -> 8-byte rumble (ignored)
    0xC0               // End Collection
};

// Invoked when GET HID REPORT DESCRIPTOR is received
uint8_t const * tud_hid_descriptor_report_cb(uint8_t itf)
{
    (void) itf;
    return desc_hid_report;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//
// Built with the SAME TinyUSB macros and args as OGX-Mini's verified
// SwitchWired.h:
//   TUD_CONFIG_DESCRIPTOR(1, 1, 0, LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 500)
//   TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, sizeof(report), 0x81,
//                      CFG_TUD_HID_EP_BUFSIZE, 1)
//
// Note vs. earlier attempts:
//   * TUD_HID_DESCRIPTOR (not _INOUT) -> IN endpoint 0x81 ONLY, no OUT EP,
//     interface bNumEndpoints = 1, total length 34.
//   * bmAttributes = 0xA0 (remote wakeup set), NOT 0x80.
//--------------------------------------------------------------------+
enum
{
    ITF_NUM_HID = 0,
    ITF_NUM_TOTAL
};

#define EPNUM_HID_IN    0x81

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

uint8_t const desc_configuration[] =
{
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 500),

    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, HID_ITF_PROTOCOL_NONE,
                       sizeof(desc_hid_report), EPNUM_HID_IN,
                       CFG_TUD_HID_EP_BUFSIZE, 1),
};

_Static_assert(sizeof(desc_configuration) == 34, "config descriptor must be 34 bytes (IN-only)");
_Static_assert(sizeof(desc_hid_report) == 86, "report descriptor must be 86 bytes (wDescriptorLength)");

// Invoked when GET CONFIGURATION DESCRIPTOR is received
uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
    (void) index;
    return desc_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+
char const *string_desc_arr[] =
{
    (const char[]) { 0x09, 0x04 }, // 0: English (US), langid 0x0409
    "HORI CO.,LTD.",               // 1: iManufacturer
    "POKKEN CONTROLLER",           // 2: iProduct  (PID 0x0092)
    "1.0"
};

static uint16_t _desc_str[32];

// Invoked when GET STRING DESCRIPTOR is received
uint16_t const * tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void) langid;
    size_t chr_count;

    if (index == 0)
    {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    }
    else
    {
        if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))
            return NULL;

        const char *str = string_desc_arr[index];
        chr_count = strlen(str);
        size_t max_count = sizeof(_desc_str) / sizeof(_desc_str[0]) - 1;
        if (chr_count > max_count) chr_count = max_count;

        // Naive ASCII -> UTF-16LE (fine for the plain-ASCII strings here)
        for (size_t i = 0; i < chr_count; i++)
            _desc_str[1 + i] = str[i];
    }

    // Byte 0: length (header + string), Byte 1: descriptor type (0x03)
    _desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));

    return _desc_str;
}

/*
 * Still needed elsewhere in your project (ctrl.c):
 *
 *   // build + queue the 8-byte input report every few ms
 *   tud_hid_report(0, &report, sizeof(report));   // report_id MUST be 0
 *
 *   // console may push an 8-byte rumble/LED report; accept and ignore it
 *   void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id,
 *       hid_report_type_t type, uint8_t const* buffer, uint16_t bufsize) {}
 *
 *   // Switch does not GET_REPORT a gamepad; returning 0 is fine
 *   uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id,
 *       hid_report_type_t type, uint8_t* buffer, uint16_t reqlen) { return 0; }
 */
