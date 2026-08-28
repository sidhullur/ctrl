#ifndef _TUSB_CONFIG_H
#define _TUSB_CONFIG_H

#ifdef __cplusplus
 extern "C" {
#endif

// Raspberry Pi Pico 2 Setup
#ifndef CFG_TUSB_MCU
#define CFG_TUSB_MCU                OPT_MCU_RP2350
#endif

#ifndef CFG_TUSB_MCU
#define CFG_TUSB_OS                 OPT_OS_NONE
#endif

#define CFG_TUSB_DEBUG              0 // no debug output
#define CFG_TUD_ENABLED             1 // rpi should operate in device mode
#define CFG_TUD_ENDPOINT0_SIZE      64 // 64 byte max size on default channel

// actual usb port should run as a device, at full speed (best latency)
// - device -> this microcontroller is plugging into another computer
// - host -> this is the computer that the other end is plugging into
#define CFG_TUSB_RHPORT0_MODE       (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)

// configuring the USB class this should act as - HID
#define CFG_TUD_HID                 1
#define CFG_TUD_HID_BUFSIZE         64
#define CFG_TUD_HID_EP_BUFSIZE      64 // buffers sent are 64 bytes in size

// Disable all other standard classes - this is only an HID
#define CFG_TUD_CDC                 0
#define CFG_TUD_MSC                 0
#define CFG_TUD_MIDI                0
#define CFG_TUD_VENDOR              0

#ifdef __cplusplus
 }
#endif

#endif