/*
 * tusb_config.h
 *
 * TinyUSB configuration for a Raspberry Pi Pico 2 W (RP2350) enumerating
 * as a HORI Pokken Tournament DX Pro Pad (VID 0x0F0D / PID 0x00C1) so the
 * Nintendo Switch accepts it as a licensed wired controller. Single HID
 * interface, bidirectional 64-byte interrupt endpoints.
 *
 * RP2350 note: TinyUSB has no separate "OPT_MCU_RP2350" value. RP2350's
 * USB controller registers are compatible with RP2040's, so the SDK
 * reuses the same `rp2040` DCD driver for both chips. CFG_TUSB_MCU is
 * still OPT_MCU_RP2040 on a Pico 2 / Pico 2 W, and pico-sdk injects it
 * (and CFG_TUSB_OS) for you when you link `tinyusb_device`.
 */

#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// Board Specific Configuration
//--------------------------------------------------------------------+

// Pico 2 W's native USB is roothub port 0
#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT      0
#endif

// RP2350's native controller is full-speed (12 Mbps) only, same as RP2040
#ifndef BOARD_TUD_MAX_SPEED
#define BOARD_TUD_MAX_SPEED   OPT_MODE_FULL_SPEED
#endif

// Required for the plain zero-argument tusb_init() call form used in
// ctrl.c. Harmless if you switch to tud_init(BOARD_TUD_RHPORT) instead.
#ifndef CFG_TUSB_RHPORT0_MODE
#define CFG_TUSB_RHPORT0_MODE  (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)
#endif

//--------------------------------------------------------------------+
// Common Configuration
//--------------------------------------------------------------------+

// pico-sdk sets this to OPT_MCU_RP2040 automatically when tinyusb_device
// is linked. Left as a hard error so a misconfigured build fails loudly.
#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined (pico-sdk sets it to OPT_MCU_RP2040 when you link tinyusb_device)
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS           OPT_OS_PICO
#endif

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG        0
#endif

// Device stack only -- no host mode needed
#define CFG_TUD_ENABLED       1
#define CFG_TUD_MAX_SPEED     BOARD_TUD_MAX_SPEED

#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN    __attribute__((aligned(4)))
#endif

//--------------------------------------------------------------------+
// Device Configuration
//--------------------------------------------------------------------+

#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE  64
#endif

//------------- CLASS -------------//
// Only HID: the HORIPAD exposes exactly one interface (bNumInterfaces = 1).
#define CFG_TUD_HID     1
#define CFG_TUD_CDC     0
#define CFG_TUD_MSC     0
#define CFG_TUD_MIDI    0
#define CFG_TUD_VENDOR  0

// Must be >= the largest HID report. The input report is 8 bytes; 64 is
// kept to match the real device's endpoint size and leave headroom for
// the ignored rumble OUT report. Must match the endpoint size passed to
// TUD_HID_INOUT_DESCRIPTOR() in usb_descriptors.c.
#define CFG_TUD_HID_EP_BUFSIZE  64

#ifdef __cplusplus
}
#endif

#endif /* TUSB_CONFIG_H_ */
