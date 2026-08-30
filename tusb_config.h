/*
 * tusb_config.h
 *
 * TinyUSB configuration for a Raspberry Pi Pico 2 W (RP2350) posing as a
 * Nintendo Switch Pro Controller. Pairs with the usb_descriptors.c from
 * the companion reference (VID 0x057E / PID 0x2009, single HID interface
 * with bidirectional 64-byte interrupt endpoints).
 *
 * RP2350 note: TinyUSB has no separate "OPT_MCU_RP2350" value. RP2350's
 * USB controller registers are compatible with RP2040's, so the SDK
 * reuses the same `rp2040` DCD driver for both chips -- it just doesn't
 * define PICO_RP2040 on RP2350 builds, which disables a couple of
 * RP2040-silicon-only errata workarounds inside that driver. You still
 * build/link everything the same way; CFG_TUSB_MCU is still
 * OPT_MCU_RP2040 on a Pico 2 / Pico 2 W.
 *
 * CFG_TUSB_MCU and CFG_TUSB_OS are normally injected for you by the
 * Pico SDK's CMake build (via the `tinyusb_device` / `tinyusb_board`
 * interface libraries) as -DCFG_TUSB_MCU=OPT_MCU_RP2040 and
 * -DCFG_TUSB_OS=OPT_OS_PICO, so you should NOT need to set them
 * yourself in a normal pico_sdk_init()-based CMakeLists.txt. The guards
 * below just make this header safe to use standalone too.
 */

#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// Board Specific Configuration
//--------------------------------------------------------------------+

// RHPort number used for device — Pico 2 W's native USB is port 0
#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT      0
#endif

// RP2350's native controller is full-speed (12 Mbps) only, same as RP2040
#ifndef BOARD_TUD_MAX_SPEED
#define BOARD_TUD_MAX_SPEED   OPT_MODE_FULL_SPEED
#endif

// Required if your main.c calls the plain, zero-argument tusb_init().
// That call form is a compatibility shim that only works through this
// legacy combined mode macro -- it does NOT derive from
// CFG_TUD_ENABLED/BOARD_TUD_RHPORT above. If you instead call
// tud_init(BOARD_TUD_RHPORT) explicitly in main.c, this define isn't
// needed and can be removed. RP2040/RP2350 only have one native USB
// roothub port (no OTG), so CFG_TUSB_RHPORT1_MODE is not applicable
// here and should NOT be defined.
#ifndef CFG_TUSB_RHPORT0_MODE
#define CFG_TUSB_RHPORT0_MODE  (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)
#endif

//--------------------------------------------------------------------+
// Common Configuration
//--------------------------------------------------------------------+

// Provided by pico-sdk's CMake build for both Pico 2 / Pico 2 W targets.
// Left as a hard error (matching upstream TinyUSB's own template) so a
// misconfigured build fails loudly instead of silently picking the
// wrong port driver.
#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined (pico-sdk sets this to OPT_MCU_RP2040 automatically when you link tinyusb_device)
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS            OPT_OS_PICO
#endif

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG         0
#endif

// Enable device stack only — this project doesn't need host mode
#define CFG_TUD_ENABLED        1
#define CFG_TUD_MAX_SPEED      BOARD_TUD_MAX_SPEED

// RP2040/RP2350 have no special DMA-accessible-SRAM restriction, so
// these are no-ops here, but kept for portability with the rest of the
// TinyUSB example ecosystem.
#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN     __attribute__((aligned(4)))
#endif

//--------------------------------------------------------------------+
// Device Configuration
//--------------------------------------------------------------------+

#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE  64
#endif

//------------- CLASS -------------//
// Only HID is enabled: the real Pro Controller exposes exactly one
// interface (HID, bNumInterfaces = 1). Turning on CDC/MSC/etc. here
// would add extra interfaces and change bNumInterfaces / wTotalLength
// away from what the real device (and the descriptor set you're
// matching) reports — leave these at 0 unless you deliberately want a
// composite device for debugging and don't mind it looking different
// from a genuine Pro Controller on the wire.
#define CFG_TUD_HID     1
#define CFG_TUD_CDC     0
#define CFG_TUD_MSC     0
#define CFG_TUD_MIDI    0
#define CFG_TUD_VENDOR  0

// Pro Controller reports are always exactly 64 bytes (in both
// directions), not the small 8/16-byte keyboard/mouse reports most HID
// examples default to — this MUST be 64, and must match the endpoint
// size you pass into TUD_HID_INOUT_DESCRIPTOR() in usb_descriptors.c.
#define CFG_TUD_HID_EP_BUFSIZE  64

#ifdef __cplusplus
}
#endif

#endif /* TUSB_CONFIG_H_ */

/*
 * ---------------------------------------------------------------------
 * Minimal CMakeLists.txt wiring for a Pico 2 W target (for reference):
 *
 *   cmake_minimum_required(VERSION 3.13)
 *   include(pico_sdk_import.cmake)
 *   project(pro_controller_emu C CXX ASM)
 *   set(PICO_BOARD pico2_w)          # or pass -DPICO_BOARD=pico2_w
 *   pico_sdk_init()
 *
 *   add_executable(pro_controller_emu
 *       main.c
 *       usb_descriptors.c
 *   )
 *   target_include_directories(pro_controller_emu PRIVATE ${CMAKE_CURRENT_LIST_DIR})
 *   target_link_libraries(pro_controller_emu PRIVATE
 *       pico_stdlib
 *       tinyusb_device
 *   )
 *   pico_add_extra_outputs(pro_controller_emu)
 *
 * Linking `tinyusb_device` is what triggers pico-sdk to inject
 * CFG_TUSB_MCU / CFG_TUSB_OS as compile definitions — you don't set
 * those in code. If you build without going through pico_sdk_init()
 * (e.g. a hand-rolled Makefile), you'll need to pass
 * -DCFG_TUSB_MCU=OPT_MCU_RP2040 yourself.
 * ---------------------------------------------------------------------
 */