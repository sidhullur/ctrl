/*
 * usb_descriptors.c
 *
 * TinyUSB device / configuration / HID-report / string descriptors for a
 * device that enumerates as a genuine Nintendo Switch Pro Controller
 * (VID 0x057E / PID 0x2009), matching a real Wireshark / lsusb /
 * usbhid-dump capture of an actual Pro Controller byte-for-byte.
 *
 * Scope: enumeration/descriptors ONLY. The actual output-report parsing
 * (0x80 handshake, 0x01 rumble+subcommand, SPI-flash emulation, building
 * 0x21/0x30 input reports, etc.) belongs in your tud_hid_set_report_cb()
 * / tud_hid_get_report_cb() implementations and your main report-sending
 * loop, elsewhere in your project — see the companion protocol reference
 * for the full command-by-command breakdown.
 *
 * Requires in tusb_config.h:
 *   #define CFG_TUD_HID              1
 *   #define CFG_TUD_HID_EP_BUFSIZE   64
 * and a HID class build that supports a bidirectional (IN + OUT)
 * interrupt HID interface — i.e. TUD_HID_INOUT_DESCRIPTOR must be
 * available (same pattern as TinyUSB's official "hid_generic_inout"
 * example).
 */

#include <string.h>
#include "tusb.h"

//--------------------------------------------------------------------+
// Device Descriptor
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,

    // Real Pro Controller reports class/subclass/protocol 0 at the
    // device level — HID class is declared at the *interface*, not here.
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = 64,

    .idVendor           = 0x057E, // Nintendo Co., Ltd.
    .idProduct          = 0x2009, // Pro Controller
    .bcdDevice          = 0x0200,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

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
// Byte-for-byte capture from a genuine Pro Controller (usbhid-dump).
// 203 bytes total — matches wDescriptorLength in the real HID
// descriptor exactly. Report IDs declared inside:
//   Input:  0x30 (full controller state), 0x21 / 0x81 / 0x01
//           (subcommand replies, vendor-defined page)
//   Output: 0x01 (rumble+subcmd), 0x10 (rumble only), 0x80 (USB
//           handshake), 0x82
//--------------------------------------------------------------------+
uint8_t const desc_hid_report[] =
{
    0x05, 0x01, 0x15, 0x00, 0x09, 0x04, 0xA1, 0x01, 0x85, 0x30,
    0x05, 0x01, 0x05, 0x09, 0x19, 0x01, 0x29, 0x0A, 0x15, 0x00,
    0x25, 0x01, 0x75, 0x01, 0x95, 0x0A, 0x55, 0x00, 0x65, 0x00,
    0x81, 0x02, 0x05, 0x09, 0x19, 0x0B, 0x29, 0x0E, 0x15, 0x00,
    0x25, 0x01, 0x75, 0x01, 0x95, 0x04, 0x81, 0x02, 0x75, 0x01,
    0x95, 0x02, 0x81, 0x03, 0x0B, 0x01, 0x00, 0x01, 0x00, 0xA1,
    0x00, 0x0B, 0x30, 0x00, 0x01, 0x00, 0x0B, 0x31, 0x00, 0x01,
    0x00, 0x0B, 0x32, 0x00, 0x01, 0x00, 0x0B, 0x35, 0x00, 0x01,
    0x00, 0x15, 0x00, 0x27, 0xFF, 0xFF, 0x00, 0x00, 0x75, 0x10,
    0x95, 0x04, 0x81, 0x02, 0xC0, 0x0B, 0x39, 0x00, 0x01, 0x00,
    0x15, 0x00, 0x25, 0x07, 0x35, 0x00, 0x46, 0x3B, 0x01, 0x65,
    0x14, 0x75, 0x04, 0x95, 0x01, 0x81, 0x02, 0x05, 0x09, 0x19,
    0x0F, 0x29, 0x12, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95,
    0x04, 0x81, 0x02, 0x75, 0x08, 0x95, 0x34, 0x81, 0x03, 0x06,
    0x00, 0xFF, 0x85, 0x21, 0x09, 0x01, 0x75, 0x08, 0x95, 0x3F,
    0x81, 0x03, 0x85, 0x81, 0x09, 0x02, 0x75, 0x08, 0x95, 0x3F,
    0x81, 0x03, 0x85, 0x01, 0x09, 0x03, 0x75, 0x08, 0x95, 0x3F,
    0x91, 0x83, 0x85, 0x10, 0x09, 0x04, 0x75, 0x08, 0x95, 0x3F,
    0x91, 0x83, 0x85, 0x80, 0x09, 0x05, 0x75, 0x08, 0x95, 0x3F,
    0x91, 0x83, 0x85, 0x82, 0x09, 0x06, 0x75, 0x08, 0x95, 0x3F,
    0x91, 0x83, 0xC0
};

// Invoked when GET HID REPORT DESCRIPTOR is received
uint8_t const * tud_hid_descriptor_report_cb(uint8_t itf)
{
    (void) itf;
    return desc_hid_report;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+
enum
{
    ITF_NUM_HID = 0,
    ITF_NUM_TOTAL
};

// EP1 OUT (console -> controller) and EP1 IN (controller -> console) —
// matches the real device's two interrupt endpoints exactly.
#define EPNUM_HID_OUT   0x01
#define EPNUM_HID_IN    0x81
#define HID_EP_SIZE     64
#define HID_EP_INTERVAL 8

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_INOUT_DESC_LEN)

uint8_t const desc_configuration[] =
{
    // Config: itf count, string index, total length, attribute, power (mA)
    // Real device reports bmAttributes 0xA0 = reserved(1) + bus-powered
    // + remote wakeup, and MaxPower 500mA — reproduced exactly here.
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN,
                           TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 500),

    // Interface: number, string index, protocol, report descriptor len,
    // OUT endpoint, IN endpoint, endpoint size, polling interval
    TUD_HID_INOUT_DESCRIPTOR(ITF_NUM_HID, 0, HID_ITF_PROTOCOL_NONE,
                              sizeof(desc_hid_report),
                              EPNUM_HID_OUT, EPNUM_HID_IN,
                              HID_EP_SIZE, HID_EP_INTERVAL)
};

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
    "Nintendo Co., Ltd.",          // 1: iManufacturer
    "Pro Controller",              // 2: iProduct
    "000000000001",                // 3: iSerialNumber (matches real captures)
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
 * Still needed elsewhere in your project (not this file):
 *
 *   uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id,
 *       hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen);
 *
 *   void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id,
 *       hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize);
 *
 * set_report_cb is where the incoming 64-byte OUT packets (report IDs
 * 0x80 / 0x01 / 0x10) land — that's the entry point for the handshake
 * state machine, subcommand dispatch, SPI-flash-read emulation, and
 * rumble handling described in the protocol reference. Your main loop
 * (or a timer) is responsible for periodically calling tud_hid_report()
 * with report ID 0x30 once handshake/streaming is enabled.
 */