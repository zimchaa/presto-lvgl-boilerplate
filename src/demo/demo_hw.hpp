// Hardware helpers for the kitchen-sink demo. Each module wraps one bit of
// Presto hardware behind a couple of functions so the UI code stays clean.
#pragma once

#include <cstdint>
#include <cstddef>

// ── Ambient LEDs — 7x WS2812 behind the display (GPIO 33, PIO2) ─────
inline constexpr uint32_t HW_NUM_LEDS = 7;

void hw_leds_init();
void hw_leds_set(uint32_t i, uint8_t r, uint8_t g, uint8_t b);
void hw_leds_show();                       // push the buffer out via DMA

// ── Piezo buzzer (PWM on GPIO 43) ────────────────────────────────────
void hw_buzzer_init();
// freq_hz <= 0 (or < ~20Hz) silences. duty 0..1 (piezo volume, roughly).
void hw_buzzer_tone(float freq_hz, float duty = 0.5f);

// ── microSD slot (SPI0: SCK 34, MOSI 35, MISO 36, CS 39) ─────────────
struct hw_sd_info {
    bool     ok;                // full probe succeeded
    bool     high_capacity;     // SDHC/SDXC (block addressed)
    bool     v2;                // SD spec 2.0+ (answered CMD8)
    uint32_t blocks;            // 512-byte blocks
    uint8_t  manufacturer_id;   // from CID
    char     product[6];        // 5-char product name from CID + NUL
    bool     boot_sig;          // sector 0 ends in 0x55AA (MBR/FAT boot sig)
    const char* error;          // failing stage when !ok
};
// Blocking probe, ~50ms with a card, ~250ms timeout without. Safe to call
// repeatedly (re-initialises the card each time). Read-only: never writes.
void hw_sdcard_probe(hw_sd_info* out);

// ── WiFi — RM2 (CYW43439) module, scan only (no lwIP) ────────────────
struct hw_wifi_net {
    char    ssid[33];
    int16_t rssi;               // dBm
    uint8_t channel;
    bool    open;               // no auth
};

bool   hw_wifi_init();          // ~300ms; false if the radio doesn't respond
bool   hw_wifi_ready();
void   hw_wifi_mac(char out[18]);          // "xx:xx:xx:xx:xx:xx"
bool   hw_wifi_scan_start();
bool   hw_wifi_scan_active();
// Copy out up to `max` results (deduped by SSID, strongest RSSI kept).
size_t hw_wifi_scan_results(hw_wifi_net* out, size_t max);
