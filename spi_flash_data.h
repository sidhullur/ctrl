/*
 * spi_flash_data.h
 *
 * Emulated SPI flash contents for a Pro Controller USB HID emulator.
 * Serves subcommand 0x10 (SPI flash read) requests -- see the protocol
 * reference for the full request/response envelope.
 */

#ifndef SPI_FLASH_DATA_H_
#define SPI_FLASH_DATA_H_

#include <stdint.h>
#include <string.h>

static const uint8_t mac_addr[] = {0x7C, 0xBB, 0x8A, 0x08, 0x12, 0x11};

//--------------------------------------------------------------------+
// 0x6000-0x600F -- Serial number (16 bytes)
// First byte >= 0x80 means "no serial reported". 
//--------------------------------------------------------------------+
static const uint8_t spi_serial[16] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

// 0x6012 -- device type. 0x01 = Joy-Con(L), 0x02 = Joy-Con(R), 0x03 = Pro
#define SPI_DEVICE_TYPE  0x03

// 0x601B -- color-info-present flag. 0x01 = use SPI colors below
#define SPI_COLOR_FLAG   0x01

static const uint8_t spi_device_type[1] = { SPI_DEVICE_TYPE };
static const uint8_t spi_color_flag[1]  = { SPI_COLOR_FLAG };

//--------------------------------------------------------------------+
// 0x6020-0x6037 -- 6-axis (IMU) factory calibration (24 bytes)
// Layout: 12x int16LE = [AccOriginXYZ][AccSensitivityXYZ]
//                        [GyroOriginXYZ][GyroSensitivityXYZ]
//--------------------------------------------------------------------+
static const uint8_t spi_6axis_cal[24] = {
    0x76, 0x00, 0xA6, 0xFE, 0xEA, 0x02, // Acc origin XYZ  (118, -348, 746)
    0x00, 0x40, 0x00, 0x40, 0x00, 0x40, // Acc sensitivity XYZ (16384 x3)
    0x0E, 0x00, 0xFC, 0xFF, 0xE0, 0xFF, // Gyro origin XYZ (14, -4, -32)
    0x3B, 0x34, 0x3B, 0x34, 0x3B, 0x34, // Gyro sensitivity XYZ (13371 x3)
};

//--------------------------------------------------------------------+
// 0x603D-0x605B -- Stick factory calibration + body/button/grip color
// Contains real controller body colors (Dark Grey / White / Black)
//--------------------------------------------------------------------+
static const uint8_t spi_stick_cal_and_colors[31] = {
    0xBA, 0xF5, 0x62, 0x6F, 0xC8, 0x77, 0xED, 0x95, 0x5B, // left stick cal
    0x16, 0xD8, 0x7D, 0xF2, 0xB5, 0x5F, 0x86, 0x65, 0x5E, // right stick cal
    0xFF,                                                 // gap/unknown
    0x32, 0x32, 0x32,                                     // Pro Body color (Dark Grey)
    0xFF, 0xFF, 0xFF,                                     // Pro Button color (White)
    0x0A, 0x0A, 0x0A,                                     // Left grip color (Black)
    0x0A, 0x0A, 0x0A,                                     // Right grip color (Black)
};

//--------------------------------------------------------------------+
// 0x6080-0x6097 -- 6-axis horizontal offsets (6B) + stick device
// parameters 1 / dead-zone & range ratio (18B) = 24 bytes total.
// REAL CAPTURE: Replaced synthensized values with genuine ALPS 
// stick curve parameters to prevent 2162-0002 OS crash.
//--------------------------------------------------------------------+
static const uint8_t spi_stick_params_1[24] = {
    // 6-axis horizontal offsets
    0x50, 0xFD, 0x00, 0x00, 0xC6, 0x0F, 
    // Stick A Dead-zone / range / curve parameters
    0x0F, 0x30, 0x61, 0x96, 0x30, 0xF3, 
    0xD4, 0x14, 0x54, 0x41, 0x15, 0x54, 
    0xC7, 0x79, 0x9C, 0x33, 0x36, 0x63,
};

// 0x6098-0x60A9 -- stick device parameters 2 (18 bytes).
// Mirrored exact copy of stick params 1 per genuine hardware behavior.
static const uint8_t spi_stick_params_2[18] = {
    0x0F, 0x30, 0x61, 0x96, 0x30, 0xF3,
    0xD4, 0x14, 0x54, 0x41, 0x15, 0x54,
    0xC7, 0x79, 0x9C, 0x33, 0x36, 0x63,
};

//--------------------------------------------------------------------+
// 0x8010-0x803F -- User stick + 6-axis calibration (48 bytes).
// All 0xFF = "no user calibration written, fall back to factory"
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
// 0xFF for anything outside the mapped regions.
//--------------------------------------------------------------------+
static inline void spi_flash_read(uint32_t addr, uint8_t size, uint8_t *out)
{
    // The genuine Winbond memory behaves as unprogrammed space (0xFF) outside mapped memory.
    memset(out, 0xFF, size);

    struct {
        uint32_t base_addr;
        uint32_t len;
        const uint8_t* data_ptr;
    } regions[] = {
        { 0x6000, sizeof(spi_serial),                spi_serial },
        { 0x6012, sizeof(spi_device_type),           spi_device_type },
        { 0x601B, sizeof(spi_color_flag),            spi_color_flag },
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
            return; 
        }
    }
}

#endif /* SPI_FLASH_DATA_H_ */