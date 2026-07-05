// WiFi scanning on the Presto's RM2 (CYW43439) module.
//
// Uses pico_cyw43_arch_none: the full driver + firmware upload, but no lwIP
// TCP/IP stack — a scan happens entirely at the driver level, which keeps
// ~30KB of SRAM free for the display stack. The arch's background
// async_context services the chip from a low-priority IRQ, so scan results
// arrive via scan_cb in interrupt context; results are copied out under a
// brief IRQ-disable to keep them consistent.
#include "demo_hw.hpp"

#include "pico/cyw43_arch.h"
#include "hardware/sync.h"
#include <cstring>
#include <cstdio>

static bool s_ready = false;

static hw_wifi_net s_nets[24];
static volatile size_t s_count = 0;

static int scan_cb(void*, const cyw43_ev_scan_result_t* res) {
    if (!res || res->ssid_len == 0) return 0;

    char ssid[33];
    size_t len = res->ssid_len < 32 ? res->ssid_len : 32;
    memcpy(ssid, res->ssid, len);
    ssid[len] = 0;

    // Dedupe by SSID (APs broadcast on multiple BSSIDs); keep the strongest.
    for (size_t i = 0; i < s_count; i++) {
        if (strcmp(s_nets[i].ssid, ssid) == 0) {
            if (res->rssi > s_nets[i].rssi) {
                s_nets[i].rssi = res->rssi;
                s_nets[i].channel = res->channel;
            }
            return 0;
        }
    }
    if (s_count < count_of(s_nets)) {
        hw_wifi_net* n = &s_nets[s_count];
        strcpy(n->ssid, ssid);
        n->rssi = res->rssi;
        n->channel = res->channel;
        n->open = (res->auth_mode == 0);
        s_count = s_count + 1;
    }
    return 0;
}

bool hw_wifi_init() {
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_WORLDWIDE) != 0) {
        printf("wifi: cyw43_arch_init failed\n");
        return false;
    }
    cyw43_arch_enable_sta_mode();
    s_ready = true;
    return true;
}

bool hw_wifi_ready() { return s_ready; }

void hw_wifi_mac(char out[18]) {
    uint8_t mac[6] = {0};
    if (s_ready) cyw43_wifi_get_mac(&cyw43_state, CYW43_ITF_STA, mac);
    snprintf(out, 18, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

bool hw_wifi_scan_start() {
    if (!s_ready) return false;
    uint32_t save = save_and_disable_interrupts();
    s_count = 0;
    restore_interrupts(save);

    cyw43_wifi_scan_options_t opts = {0};
    return cyw43_wifi_scan(&cyw43_state, &opts, nullptr, scan_cb) == 0;
}

bool hw_wifi_scan_active() {
    return s_ready && cyw43_wifi_scan_active(&cyw43_state);
}

size_t hw_wifi_scan_results(hw_wifi_net* out, size_t max) {
    uint32_t save = save_and_disable_interrupts();
    size_t n = s_count < max ? s_count : max;
    memcpy(out, s_nets, n * sizeof(hw_wifi_net));
    restore_interrupts(save);

    // Sort by signal strength, strongest first (insertion sort, n <= 24).
    for (size_t i = 1; i < n; i++) {
        hw_wifi_net key = out[i];
        size_t j = i;
        while (j > 0 && out[j - 1].rssi < key.rssi) { out[j] = out[j - 1]; j--; }
        out[j] = key;
    }
    return n;
}
