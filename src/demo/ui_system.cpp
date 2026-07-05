// System tab — chip identity and live vitals: RP2350 die temperature (ADC),
// frame rate, uptime, LVGL heap usage, backlight control, and an on-demand
// microSD card probe.
#include "demo_ui.hpp"
#include "demo_hw.hpp"
#include "lvgl_port.hpp"

#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include "hardware/clocks.h"
#include "hardware/adc.h"
#include "hardware/psram.h"
#include <cstdio>

static lv_obj_t* temp_label;
static lv_obj_t* fps_label;
static lv_obj_t* uptime_label;
static lv_obj_t* mem_label;
static lv_obj_t* mem_bar;
static lv_obj_t* sd_label;

static float read_die_temp() {
    adc_select_input(ADC_TEMPERATURE_CHANNEL_NUM);
    float v = (float)adc_read() * 3.3f / 4096.0f;
    return 27.0f - (v - 0.706f) / 0.001721f;
}

static void vitals_tick(lv_timer_t*) {
    static uint32_t last_frames = 0;
    static uint32_t last_ms = 0;

    uint32_t now = to_ms_since_boot(get_absolute_time());
    uint32_t frames = lvgl_port_frame_count();
    if (last_ms != 0 && now > last_ms) {
        uint32_t fps10 = (frames - last_frames) * 10000 / (now - last_ms);
        lv_label_set_text_fmt(fps_label, "%lu.%lu fps",
                              (unsigned long)(fps10 / 10), (unsigned long)(fps10 % 10));
    }
    last_frames = frames;
    last_ms = now;

    int t10 = (int)(read_die_temp() * 10.0f);
    lv_label_set_text_fmt(temp_label, "%d.%d °C", t10 / 10, t10 % 10);

    uint32_t secs = now / 1000;
    lv_label_set_text_fmt(uptime_label, "%02lu:%02lu:%02lu",
                          (unsigned long)(secs / 3600),
                          (unsigned long)((secs / 60) % 60),
                          (unsigned long)(secs % 60));

    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    lv_label_set_text_fmt(mem_label, "LVGL heap  %lu / %lu KB",
                          (unsigned long)((mon.total_size - mon.free_size) / 1024),
                          (unsigned long)(mon.total_size / 1024));
    lv_bar_set_value(mem_bar, mon.used_pct, LV_ANIM_ON);
}

static lv_obj_t* info_row(lv_obj_t* card, const char* key, const char* value) {
    lv_obj_t* row = lv_obj_create(card);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* k = lv_label_create(row);
    lv_label_set_text(k, key);
    lv_obj_set_style_text_color(k, lv_color_hex(0x8a93a6), 0);

    lv_obj_t* v = lv_label_create(row);
    lv_label_set_text(v, value);
    return v;
}

static void sd_probe_cb(lv_event_t*) {
    lv_label_set_text(sd_label, "probing...");
    // Force the label to repaint before the (blocking) probe runs.
    lv_refr_now(nullptr);

    hw_sd_info info;
    hw_sdcard_probe(&info);
    if (!info.ok) {
        lv_label_set_text_fmt(sd_label, "no card / failed: %s", info.error);
        return;
    }
    uint32_t mb = (uint32_t)(((uint64_t)info.blocks * 512) / (1000 * 1000));
    lv_label_set_text_fmt(sd_label,
        "%s '%s'  %lu.%lu GB\n%s, mfr 0x%02x, boot sig %s",
        info.high_capacity ? "SDHC/XC" : "SD",
        info.product,
        (unsigned long)(mb / 1000), (unsigned long)((mb % 1000) / 100),
        info.v2 ? "v2.0+" : "v1.x",
        info.manufacturer_id,
        info.boot_sig ? "OK" : "absent");
}

void ui_system_create(lv_obj_t* tab) {
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(tab, S(10), 0);

    adc_init();
    adc_set_temp_sensor_enabled(true);

    // ── Static identity ──────────────────────────────────────────────
    lv_obj_t* id_card = demo_card(tab, "board");

    char clk[32];
    snprintf(clk, sizeof(clk), "%lu MHz", (unsigned long)(clock_get_hz(clk_sys) / 1000000));
    info_row(id_card, "RP2350B", clk);

    char board_id[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];
    pico_get_unique_board_id_string(board_id, sizeof(board_id));
    info_row(id_card, "unique id", board_id);

    char psram[32];
    snprintf(psram, sizeof(psram), "%u MB", (unsigned)(psram_get_size() / (1024 * 1024)));
    info_row(id_card, "PSRAM", psram_is_available() ? psram : "unavailable");

    char res[32];
    snprintf(res, sizeof(res), "%ux%u%s", DISPLAY_WIDTH, DISPLAY_HEIGHT,
             DISPLAY_WIDTH == 480 ? " native" : " doubled");
    info_row(id_card, "display", res);

    // ── Live vitals ──────────────────────────────────────────────────
    lv_obj_t* live_card = demo_card(tab, "live");
    temp_label   = info_row(live_card, "die temp", "-");
    fps_label    = info_row(live_card, "frame rate", "-");
    uptime_label = info_row(live_card, "uptime", "-");

    mem_label = lv_label_create(live_card);
    lv_label_set_text(mem_label, "LVGL heap");
    lv_obj_set_style_text_color(mem_label, lv_color_hex(0x8a93a6), 0);
    mem_bar = lv_bar_create(live_card);
    lv_obj_set_size(mem_bar, lv_pct(100), S(12));
    lv_bar_set_range(mem_bar, 0, 100);

    lv_timer_create(vitals_tick, 500, nullptr);

    // ── Backlight ────────────────────────────────────────────────────
    lv_obj_t* bl_card = demo_card(tab, "backlight (PWM, GPIO 45)");
    lv_obj_t* bl = lv_slider_create(bl_card);
    lv_obj_set_width(bl, lv_pct(96));
    lv_slider_set_range(bl, 10, 255);      // don't let the screen go black
    lv_slider_set_value(bl, 255, LV_ANIM_OFF);
    lv_obj_add_event_cb(bl, [](lv_event_t* e) {
        lvgl_port_set_backlight((uint8_t)lv_slider_get_value((lv_obj_t*)lv_event_get_target(e)));
    }, LV_EVENT_VALUE_CHANGED, nullptr);

    // ── microSD ──────────────────────────────────────────────────────
    lv_obj_t* sd_card = demo_card(tab, "microSD (SPI0)");
    lv_obj_t* btn = lv_button_create(sd_card);
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, LV_SYMBOL_SD_CARD "  Probe card");
    lv_obj_center(lbl);
    lv_obj_add_event_cb(btn, sd_probe_cb, LV_EVENT_CLICKED, nullptr);

    sd_label = lv_label_create(sd_card);
    lv_label_set_text(sd_label, "insert a card and probe (read-only)");
    lv_label_set_long_mode(sd_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(sd_label, lv_pct(100));
    lv_obj_set_style_text_color(sd_label, lv_color_hex(0x8a93a6), 0);
}
