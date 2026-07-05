// WiFi tab — scans for networks with the RM2 (CYW43439) module and lists
// them by signal strength. Scan-only: no credentials, no lwIP.
#include "demo_ui.hpp"
#include "demo_hw.hpp"

#include "pico.h"   // count_of
#include <cstdio>

static lv_obj_t* mac_label;
static lv_obj_t* status_label;
static lv_obj_t* scan_btn;
static lv_obj_t* spinner;
static lv_obj_t* list;
static lv_timer_t* poll_timer;

static void show_results() {
    static hw_wifi_net nets[24];
    size_t n = hw_wifi_scan_results(nets, count_of(nets));

    lv_obj_clean(list);
    lv_label_set_text_fmt(status_label, "%u network%s found",
                          (unsigned)n, n == 1 ? "" : "s");

    for (size_t i = 0; i < n; i++) {
        char line[64];
        snprintf(line, sizeof(line), "%s\n%d dBm   ch %u%s",
                 nets[i].ssid, nets[i].rssi, nets[i].channel,
                 nets[i].open ? "   open" : "");
        lv_obj_t* btn = lv_list_add_button(list, LV_SYMBOL_WIFI, line);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1e232b), 0);
        lv_obj_set_style_text_color(btn, lv_color_white(), 0);
    }
}

static void poll_scan(lv_timer_t* t) {
    if (hw_wifi_scan_active()) return;

    lv_timer_delete(t);
    poll_timer = nullptr;
    lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_state(scan_btn, LV_STATE_DISABLED);
    show_results();
}

static void start_scan(lv_event_t*) {
    if (!hw_wifi_ready() || poll_timer) return;
    if (!hw_wifi_scan_start()) {
        lv_label_set_text(status_label, "scan failed to start");
        return;
    }
    lv_label_set_text(status_label, "scanning...");
    lv_obj_remove_flag(spinner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_state(scan_btn, LV_STATE_DISABLED);
    poll_timer = lv_timer_create(poll_scan, 250, nullptr);
}

void ui_wifi_create(lv_obj_t* tab) {
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(tab, S(10), 0);

    lv_obj_t* card = demo_card(tab, "CYW43439 radio");

    // The radio is brought up after the first frame (see demo_main.cpp);
    // ui_wifi_set_ready() fills this in.
    mac_label = lv_label_create(card);
    lv_label_set_text(mac_label, "starting radio...");

    lv_obj_t* row = lv_obj_create(card);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, S(12), 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    scan_btn = lv_button_create(row);
    lv_obj_t* lbl = lv_label_create(scan_btn);
    lv_label_set_text(lbl, LV_SYMBOL_REFRESH "  Scan");
    lv_obj_center(lbl);
    lv_obj_add_event_cb(scan_btn, start_scan, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_state(scan_btn, LV_STATE_DISABLED);   // until the radio is up

    spinner = lv_spinner_create(row);
    lv_obj_set_size(spinner, S(32), S(32));
    lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);

    status_label = lv_label_create(row);
    lv_label_set_text(status_label, "-");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x8a93a6), 0);

    list = lv_list_create(tab);
    lv_obj_set_width(list, lv_pct(100));
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x1e232b), 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_radius(list, S(12), 0);
}

// Called from demo_main once the (deferred) radio bring-up has finished.
void ui_wifi_set_ready(bool ok) {
    if (ok) {
        char mac[18];
        hw_wifi_mac(mac);
        lv_label_set_text_fmt(mac_label, "MAC  %s", mac);
        lv_label_set_text(status_label, "ready");
        lv_obj_remove_state(scan_btn, LV_STATE_DISABLED);
    } else {
        lv_label_set_text(mac_label, "radio init failed");
        lv_obj_set_style_text_color(mac_label, lv_palette_main(LV_PALETTE_RED), 0);
    }
}
