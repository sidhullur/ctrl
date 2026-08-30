/*
 * spi_flash_data.h
 *
 * Emulated SPI flash contents for a Pro Controller USB HID emulator.
 * Serves subcommand 0x10 (SPI flash read) requests -- see the protocol
 * reference for the full request/response envelope.
 *
 * PROVENANCE (read this before trusting a byte blindly):
 *
 *   [REAL CAPTURE]   0x603D-0x6055 (stick calibration + colors) --
 *       byte-for-byte from a genuine controller, captured over a live
 *       Bluetooth session by the nxbt project (github.com/Brikwerk/nxbt,
 *       docs/Analog Stick Input.md). Decodes to sane real-world values
 *       (center ~2000-2159, min ~333-693, max ~3381-3676 on a 12-bit
 *       range) -- this is what a real stick's calibration looks like,
 *       not a synthetic guess.
 *
 *   [REAL SAMPLE, ADAPTED]  0x6020-0x6037 (6-axis/IMU calibration) --
 *       same 24-byte format, values taken from a published sample dump
 *       in dekuNukem's spi_flash_notes.md. That specific sample was
 *       documented at the *user*-calibration offset (0x8026-0x803F),
 *       not literally captured at 0x6020, but it uses the identical
 *       format and the values themselves look like real shipped
 *       defaults (accel sensitivity = 0x4000/16384 x3, a clean
 *       power-of-two; gyro sensitivity = 0x343B/13371 x3, repeated
 *       identically across axes) rather than arbitrary numbers -- i.e.
 *       exactly the pattern you'd expect from a factory constant table,
 *       not from someone's live per-unit calibration run.
 *
 *   [DOCUMENTED BEHAVIOR, NOT ARBITRARY]  Serial number region and user
 *       calibration regions (0x8010-0x803F) set to all 0xFF -- this is
 *       the real, documented "nothing here" sentinel (first serial byte
 *       >=0x80 means "no serial"; 0xFF-filled user-cal means "no user
 *       calibration, fall back to factory"), not a guess.
 *
 *   [SYNTHESIZED PLACEHOLDER -- lower confidence]  0x6080-0x60A9 (6-axis
 *       horizontal offsets + stick device parameters/dead-zone/range).
 *       I don't have a verified real capture for this specific region,
 *       so these are reasonable round placeholder values, not hardware
 *       data. Per the protocol reference this region is "Recommended"
 *       not "Required" -- the console tolerates a generic dead-zone
 *       table fine; revisit only if in-game stick feel seems off.
 */

#ifndef SPI_FLASH_DATA_H_
#define SPI_FLASH_DATA_H_

#include <stdint.h>
#include <string.h>

static const uint8_t mac_addr[] = {0x7C, 0xBB, 0x8A, 0x08, 0x12, 0x11};

//--------------------------------------------------------------------+
// 0x6000-0x600F -- Serial number (16 bytes)
// First byte >= 0x80 means "no serial reported". All-0xFF is the
// documented, real-world-observed way many units report this.
//--------------------------------------------------------------------+
static const uint8_t spi_serial[16] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

// 0x6012 -- device type. 0x01 = Joy-Con(L), 0x02 = Joy-Con(R), 0x03 = Pro
#define SPI_DEVICE_TYPE  0x03

// 0x601B -- color-info-present flag. 0x01 = use SPI colors below,
// 0x00 = console uses default gray/white instead.
#define SPI_COLOR_FLAG   0x01

//--------------------------------------------------------------------+
// 0x6020-0x6037 -- 6-axis (IMU) factory calibration (24 bytes)
// Layout: 12x int16LE = [AccOriginXYZ][AccSensitivityXYZ]
//                        [GyroOriginXYZ][GyroSensitivityXYZ]
// [REAL SAMPLE, ADAPTED] -- see provenance note above.
//--------------------------------------------------------------------+
static const uint8_t spi_6axis_cal[24] = {
    0x76, 0x00, 0xA6, 0xFE, 0xEA, 0x02, // Acc origin XYZ  (118, -348, 746)
    0x00, 0x40, 0x00, 0x40, 0x00, 0x40, // Acc sensitivity XYZ (16384 x3)
    0x0E, 0x00, 0xFC, 0xFF, 0xE0, 0xFF, // Gyro origin XYZ (14, -4, -32)
    0x3B, 0x34, 0x3B, 0x34, 0x3B, 0x34, // Gyro sensitivity XYZ (13371 x3)
};

//--------------------------------------------------------------------+
// 0x603D-0x6055 -- Stick factory calibration + body/button color
// (25 bytes total). [REAL CAPTURE] -- see provenance note above.
//
// Byte layout:
//   0x603D-0x6045 (9)  Left stick calibration
//   0x6046-0x604E (9)  Right stick calibration
//   0x604F        (1)  Unknown/gap (observed as 0xFF)
//   0x6050-0x6052 (3)  Body color (RGB) -- 0x828282, a dark gray
//   0x6053-0x6055 (3)  Button color (RGB) -- 0x0F0F0F, near-black
//
// Decodes to (verified against the source's own worked example):
//   Left  stick: center (2159, 1916), X range 693-3676, Y range 333-3381
//   Right stick: center (2070, 2013), X range  548-3484, Y range 482-3523
//--------------------------------------------------------------------+
static const uint8_t spi_stick_cal_and_colors[25] = {
    0xBA, 0xF5, 0x62, 0x6F, 0xC8, 0x77, 0xED, 0x95, 0x5B, // left stick cal
    0x16, 0xD8, 0x7D, 0xF2, 0xB5, 0x5F, 0x86, 0x65, 0x5E, // right stick cal
    0xFF,                                                 // gap/unknown
    0x82, 0x82, 0x82,                                     // body color
    0x0F, 0x0F, 0x0F,                                     // button color
};

//--------------------------------------------------------------------+
// 0x6080-0x6097 -- 6-axis horizontal offsets (6B) + stick device
// parameters 1 / dead-zone & range ratio (18B) = 24 bytes total.
// [SYNTHESIZED PLACEHOLDER] -- see provenance note above.
//--------------------------------------------------------------------+
// NOTE: unlike the 0x8010+ *user* calibration region, this factory
// region has no "magic marker" convention -- it's just raw dead-zone/
// range data, always considered present. All-zero deadzone is valid
// (per the doc, it just means a very small/no dead-zone, closer to
// d-pad-like snappy behavior) and won't cause the console to reject
// anything; it's a safe, honest placeholder rather than fabricated
// structure I can't verify.
static const uint8_t spi_stick_params_1[24] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 6-axis horiz. offsets (neutral)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // dead-zone/range, stick A (placeholder, all-zero)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // dead-zone/range, stick B (placeholder, all-zero)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// 0x6098-0x60A9 -- stick device parameters 2 (18 bytes). Real hardware
// normally mirrors params 1 exactly, even on Pro Controller, per the
// documented behavior -- so this is a direct copy, not an independent
// placeholder.
static const uint8_t spi_stick_params_2[18] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

//--------------------------------------------------------------------+
// 0x8010-0x803F -- User stick + 6-axis calibration (48 bytes).
// All 0xFF = "no user calibration written, fall back to factory
// values above." This is the real documented sentinel, not a guess --
// simplest and safest choice unless you specifically want to emulate
// a controller that's been through Switch's calibration menu.
//--------------------------------------------------------------------+
static const uint8_t spi_user_cal[48] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

//--------------------------------------------------------------------+
// Dispatcher -- call this from your subcommand 0x10 handler.
// Fills `out` with `size` bytes starting at `addr`. Falls back to
// zeros for anything outside the ranges defined above (safe default
// for addresses the real init sequence doesn't actually request).
//--------------------------------------------------------------------+
static inline void spi_flash_read(uint32_t addr, uint8_t size, uint8_t *out)
{
    memset(out, 0x00, size);

    struct {
        uint32_t base_addr;
        uint32_t len; 
        const uint8_t* data_ptr;
    } regions[] = {
        { 0x6000, sizeof(spi_serial),                spi_serial },
        { 0x6020, sizeof(spi_6axis_cal),             spi_6axis_cal },
        { 0x603D, sizeof(spi_stick_cal_and_colors),  spi_stick_cal_and_colors },
        { 0x6080, sizeof(spi_stick_params_1),        spi_stick_params_1 },
        { 0x6098, sizeof(spi_stick_params_2),        spi_stick_params_2 },
        { 0x8010, sizeof(spi_user_cal),              spi_user_cal },
    };

    for (size_t r = 0; r < sizeof(regions) / sizeof(regions[0]); r++) {
        uint32_t rbase = regions[r].base_addr, rlen = regions[r].len;
        if (addr >= rbase && addr < rbase + rlen) {
            uint32_t offset    = addr - rbase;
            uint32_t available = rlen - offset;
            uint32_t copy_len  = (size < available) ? size : available;
            memcpy(out, (regions[r].data_ptr) + offset, copy_len);
            return; // requests don't straddle two of these regions in practice
        }
    }
}

#endif /* SPI_FLASH_DATA_H_ */
