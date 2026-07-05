// Raw SD-card probe over SPI0 (SCK 34, MOSI 35, MISO 36, CS 39 — the
// Presto's microSD slot). No filesystem: this runs the SPI-mode init
// handshake (CMD0/CMD8/ACMD41/CMD58), reads the CSD and CID registers for
// capacity and identity, then reads sector 0 at 12.5MHz to prove the data
// path. Strictly read-only.
#include "demo_hw.hpp"

#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <cstring>

static const uint SD_SCK  = 34;
static const uint SD_MOSI = 35;
static const uint SD_MISO = 36;
static const uint SD_CS   = 39;

static void cs(bool select) { gpio_put(SD_CS, !select); }

static uint8_t xfer(uint8_t tx) {
    uint8_t rx;
    spi_write_read_blocking(spi0, &tx, &rx, 1);
    return rx;
}

// Send a command frame and wait for the R1 response (MSB clear).
static uint8_t sd_cmd(uint8_t cmd, uint32_t arg, uint8_t crc) {
    uint8_t frame[6] = {
        (uint8_t)(0x40 | cmd),
        (uint8_t)(arg >> 24), (uint8_t)(arg >> 16),
        (uint8_t)(arg >> 8),  (uint8_t)arg,
        crc,
    };
    spi_write_blocking(spi0, frame, sizeof(frame));
    for (int i = 0; i < 10; i++) {
        uint8_t r = xfer(0xFF);
        if (!(r & 0x80)) return r;
    }
    return 0xFF;   // no response
}

// Wait for a data start token (0xFE) then read `len` bytes + 2 CRC bytes.
static bool sd_read_block(uint8_t* dst, size_t len) {
    absolute_time_t deadline = make_timeout_time_ms(200);
    uint8_t t;
    do {
        t = xfer(0xFF);
        if (t == 0xFE) break;
        if (absolute_time_diff_us(get_absolute_time(), deadline) < 0) return false;
    } while (true);
    spi_read_blocking(spi0, 0xFF, dst, len);
    xfer(0xFF); xfer(0xFF);   // discard CRC
    return true;
}

void hw_sdcard_probe(hw_sd_info* out) {
    memset(out, 0, sizeof(*out));

    spi_init(spi0, 400 * 1000);           // init must run at 100-400kHz
    gpio_set_function(SD_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(SD_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(SD_MISO, GPIO_FUNC_SPI);
    gpio_pull_up(SD_MISO);
    gpio_init(SD_CS);
    gpio_set_dir(SD_CS, GPIO_OUT);
    cs(false);

    // >74 clocks with CS high puts the card in SPI mode.
    for (int i = 0; i < 12; i++) xfer(0xFF);

    // CMD0: software reset, expect "idle" (0x01).
    cs(true);
    uint8_t r1 = 0xFF;
    for (int i = 0; i < 8 && r1 != 0x01; i++) r1 = sd_cmd(0, 0, 0x95);
    if (r1 != 0x01) { out->error = "no card (CMD0)"; goto done; }

    // CMD8: voltage check — answered by v2 cards, illegal on v1.
    r1 = sd_cmd(8, 0x1AA, 0x87);
    if (r1 == 0x01) {
        uint8_t r7[4];
        spi_read_blocking(spi0, 0xFF, r7, sizeof(r7));
        if (r7[2] != 0x01 || r7[3] != 0xAA) { out->error = "CMD8 echo bad"; goto done; }
        out->v2 = true;
    }

    // ACMD41 (with HCS for v2 cards) until the card leaves idle.
    {
        absolute_time_t deadline = make_timeout_time_ms(1000);
        do {
            sd_cmd(55, 0, 0xFF);
            r1 = sd_cmd(41, out->v2 ? (1u << 30) : 0, 0xFF);
            if (r1 == 0x00) break;
            if (absolute_time_diff_us(get_absolute_time(), deadline) < 0) {
                out->error = "init timeout (ACMD41)";
                goto done;
            }
        } while (true);
    }

    // CMD58: read OCR, bit 30 (CCS) = block-addressed SDHC/SDXC.
    if (sd_cmd(58, 0, 0xFF) == 0x00) {
        uint8_t ocr[4];
        spi_read_blocking(spi0, 0xFF, ocr, sizeof(ocr));
        out->high_capacity = (ocr[0] & 0x40) != 0;
    }

    // CMD9: CSD register -> capacity.
    {
        uint8_t csd[16];
        if (sd_cmd(9, 0, 0xFF) != 0x00 || !sd_read_block(csd, sizeof(csd))) {
            out->error = "CSD read failed";
            goto done;
        }
        if ((csd[0] >> 6) == 1) {          // CSD v2 (SDHC/SDXC)
            uint32_t c_size = ((uint32_t)(csd[7] & 0x3F) << 16) |
                              ((uint32_t)csd[8] << 8) | csd[9];
            out->blocks = (c_size + 1) * 1024;   // (C_SIZE+1) * 512KB / 512B
        } else {                            // CSD v1 (SDSC)
            uint32_t c_size = ((uint32_t)(csd[6] & 0x03) << 10) |
                              ((uint32_t)csd[7] << 2) | (csd[8] >> 6);
            uint32_t mult = 1u << ((((csd[9] & 0x03) << 1) | (csd[10] >> 7)) + 2);
            uint32_t bl_len = 1u << (csd[5] & 0x0F);
            out->blocks = (c_size + 1) * mult * (bl_len / 512);
        }
    }

    // CMD10: CID register -> manufacturer + product name.
    {
        uint8_t cid[16];
        if (sd_cmd(10, 0, 0xFF) == 0x00 && sd_read_block(cid, sizeof(cid))) {
            out->manufacturer_id = cid[0];
            memcpy(out->product, &cid[3], 5);
            out->product[5] = 0;
        }
    }

    // Data path check: read sector 0 at full speed, look for the 0x55AA
    // boot signature an MBR/FAT-formatted card ends its first sector with.
    {
        spi_set_baudrate(spi0, 12'500'000);
        static uint8_t sector[512];
        // Address 0 is sector 0 in both byte and block addressing modes.
        if (sd_cmd(17, 0, 0xFF) != 0x00 || !sd_read_block(sector, sizeof(sector))) {
            out->error = "sector 0 read failed";
            goto done;
        }
        out->boot_sig = (sector[510] == 0x55 && sector[511] == 0xAA);
    }

    out->ok = true;

done:
    cs(false);
    xfer(0xFF);          // 8 clocks with CS high releases the bus
    spi_deinit(spi0);
}
