// Home tab — a tour of the LVGL toolkit itself: an animated round gauge
// (lv_scale + needle driven by lv_anim), a live scrolling chart, and a row
// of the everyday input widgets.
#include "demo_ui.hpp"

#include <cmath>
#include <cstdlib>

// ── Gauge ────────────────────────────────────────────────────────────

static lv_obj_t* gauge_scale;
static lv_obj_t* gauge_needle;
static lv_obj_t* gauge_value_label;

static void gauge_anim_cb(void*, int32_t v) {
    lv_scale_set_line_needle_value(gauge_scale, gauge_needle, S(70), v);
    lv_label_set_text_fmt(gauge_value_label, "%ld", (long)v);
}

static void build_gauge(lv_obj_t* parent) {
    lv_obj_t* card = demo_card(parent, "lv_scale + lv_anim");
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    gauge_scale = lv_scale_create(card);
    lv_obj_set_size(gauge_scale, S(180), S(180));
    lv_scale_set_mode(gauge_scale, LV_SCALE_MODE_ROUND_INNER);
    lv_scale_set_range(gauge_scale, 0, 100);
    lv_scale_set_total_tick_count(gauge_scale, 21);
    lv_scale_set_major_tick_every(gauge_scale, 5);
    lv_scale_set_angle_range(gauge_scale, 270);
    lv_scale_set_rotation(gauge_scale, 135);
    lv_obj_set_style_length(gauge_scale, S(8), LV_PART_ITEMS);
    lv_obj_set_style_length(gauge_scale, S(14), LV_PART_INDICATOR);
    lv_obj_set_style_text_font(gauge_scale, DEMO_FONT_SMALL, LV_PART_INDICATOR);

    gauge_needle = lv_line_create(gauge_scale);
    lv_obj_set_style_line_width(gauge_needle, S(5), 0);
    lv_obj_set_style_line_rounded(gauge_needle, true, 0);
    lv_obj_set_style_line_color(gauge_needle, lv_palette_main(LV_PALETTE_RED), 0);

    gauge_value_label = lv_label_create(gauge_scale);
    lv_obj_set_style_text_font(gauge_value_label, DEMO_FONT_BIG, 0);
    lv_obj_align(gauge_value_label, LV_ALIGN_BOTTOM_MID, 0, -S(8));

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, gauge_anim_cb);
    lv_anim_set_values(&a, 0, 100);
    lv_anim_set_duration(&a, 2400);
    lv_anim_set_playback_duration(&a, 2400);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

// ── Live chart ───────────────────────────────────────────────────────

static lv_chart_series_t* ser_sine;
static lv_chart_series_t* ser_walk;

static void chart_tick(lv_timer_t* t) {
    lv_obj_t* chart = (lv_obj_t*)lv_timer_get_user_data(t);

    static float phase = 0.0f;
    static int32_t walk = 50;
    phase += 0.25f;
    walk += (rand() % 15) - 7;
    if (walk < 5) walk = 5;
    if (walk > 95) walk = 95;

    lv_chart_set_next_value(chart, ser_sine, (int32_t)(50.0f + 40.0f * sinf(phase)));
    lv_chart_set_next_value(chart, ser_walk, walk);
}

static void build_chart(lv_obj_t* parent) {
    lv_obj_t* card = demo_card(parent, "lv_chart (live)");

    lv_obj_t* chart = lv_chart_create(card);
    lv_obj_set_size(chart, lv_pct(100), S(140));
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, 48);
    lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_div_line_count(chart, 5, 8);
    lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR);   // no point dots

    ser_sine = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_CYAN),
                                   LV_CHART_AXIS_PRIMARY_Y);
    ser_walk = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_ORANGE),
                                   LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_value(chart, ser_sine, 50);
    lv_chart_set_all_value(chart, ser_walk, 50);

    lv_timer_create(chart_tick, 120, chart);
}

// ── Everyday input widgets ───────────────────────────────────────────

static void build_controls(lv_obj_t* parent) {
    lv_obj_t* card = demo_card(parent, "widgets");

    lv_obj_t* row = lv_obj_create(card);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* sw = lv_switch_create(row);
    lv_obj_add_state(sw, LV_STATE_CHECKED);

    lv_obj_t* cb = lv_checkbox_create(row);
    lv_checkbox_set_text(cb, "check");

    lv_obj_t* dd = lv_dropdown_create(row);
    lv_dropdown_set_options(dd, "Alpha\nBeta\nGamma\nDelta");
    lv_obj_set_width(dd, S(140));

    lv_obj_t* slider = lv_slider_create(card);
    lv_obj_set_width(slider, lv_pct(96));
    lv_slider_set_value(slider, 60, LV_ANIM_OFF);
    lv_obj_set_style_margin_top(slider, S(8), 0);

    lv_obj_t* roller = lv_roller_create(card);
    lv_roller_set_options(roller,
        "January\nFebruary\nMarch\nApril\nMay\nJune\nJuly", LV_ROLLER_MODE_INFINITE);
    lv_roller_set_visible_row_count(roller, 3);
    lv_obj_set_width(roller, lv_pct(100));
}

// ── Tab ──────────────────────────────────────────────────────────────

void ui_home_create(lv_obj_t* tab) {
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(tab, S(10), 0);

    lv_obj_t* title = lv_label_create(tab);
    lv_label_set_text(title, "Presto kitchen sink");
    lv_obj_set_style_text_font(title, DEMO_FONT_BIG, 0);

    lv_obj_t* sub = lv_label_create(tab);
    lv_label_set_text_fmt(sub, "LVGL v%d.%d.%d",
                          LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
    lv_obj_set_style_text_color(sub, lv_color_hex(0x8a93a6), 0);
    lv_obj_set_style_text_font(sub, DEMO_FONT_SMALL, 0);

    build_gauge(tab);
    build_chart(tab);
    build_controls(tab);
}
