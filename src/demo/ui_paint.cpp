// Paint tab — finger painting on an lv_canvas.
// Strokes are rendered with the LVGL draw engine (lv_canvas_init_layer +
// lv_draw_line with round caps), and the canvas pixel buffer lives in PSRAM
// — at full res it wouldn't fit anywhere else, and it exercises the QMI
// memory under CPU read-modify-write load.
#include "demo_ui.hpp"

#include "pico.h"   // count_of
#include "pico/platform/sections.h"

// Sized to fill the tab below the toolbar (design space is 480x480:
// ~56px tab bar, ~90px toolbar, padding).
static constexpr int32_t CANVAS_W = S(432);
static constexpr int32_t CANVAS_H = S(280);

static uint16_t __uninitialized_psram("paint_canvas") canvas_buf[CANVAS_W * CANVAS_H];

static lv_obj_t*  canvas;
static lv_color_t brush_color;
static int32_t    brush_size = S(10);
static bool       stroke_active = false;
static lv_point_t last_pt;

static const uint32_t PALETTE[] = {
    0x000000, 0xe53935, 0xfb8c00, 0xfdd835,
    0x43a047, 0x1e88e5, 0x8e24aa, 0xffffff,   // white = eraser
};

static void canvas_clear() {
    lv_canvas_fill_bg(canvas, lv_color_white(), LV_OPA_COVER);
}

static void draw_segment(lv_point_t a, lv_point_t b) {
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = brush_color;
    dsc.width = brush_size;
    dsc.round_start = 1;
    dsc.round_end = 1;
    dsc.p1.x = a.x; dsc.p1.y = a.y;
    dsc.p2.x = b.x; dsc.p2.y = b.y;
    lv_draw_line(&layer, &dsc);

    lv_canvas_finish_layer(canvas, &layer);
}

static void canvas_pressing_cb(lv_event_t* e) {
    lv_indev_t* indev = lv_event_get_indev(e);
    if (!indev) return;

    lv_point_t screen;
    lv_indev_get_point(indev, &screen);

    lv_area_t coords;
    lv_obj_get_coords(canvas, &coords);
    lv_point_t p = { screen.x - coords.x1, screen.y - coords.y1 };
    if (p.x < 0 || p.y < 0 || p.x >= CANVAS_W || p.y >= CANVAS_H) return;

    draw_segment(stroke_active ? last_pt : p, p);
    stroke_active = true;
    last_pt = p;
}

void ui_paint_create(lv_obj_t* tab) {
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(tab, S(8), 0);
    lv_obj_set_style_pad_all(tab, S(10), 0);
    // No scrolling here — drags are brush strokes, not scroll gestures.
    lv_obj_remove_flag(tab, LV_OBJ_FLAG_SCROLLABLE);

    brush_color = lv_color_black();

    canvas = lv_canvas_create(tab);
    lv_canvas_set_buffer(canvas, canvas_buf, CANVAS_W, CANVAS_H,
                         LV_COLOR_FORMAT_RGB565);
    lv_obj_set_style_radius(canvas, S(6), 0);
    lv_obj_add_flag(canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(canvas, LV_OBJ_FLAG_PRESS_LOCK);
    canvas_clear();

    lv_obj_add_event_cb(canvas, canvas_pressing_cb, LV_EVENT_PRESSING, nullptr);
    lv_obj_add_event_cb(canvas, [](lv_event_t*) { stroke_active = false; },
                        LV_EVENT_RELEASED, nullptr);

    // Toolbar: colour swatches, brush size, clear.
    lv_obj_t* bar = lv_obj_create(tab);
    lv_obj_set_size(bar, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_style_pad_column(bar, S(6), 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    for (size_t i = 0; i < count_of(PALETTE); i++) {
        lv_obj_t* sw = lv_button_create(bar);
        lv_obj_set_size(sw, S(38), S(38));
        lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(sw, lv_color_hex(PALETTE[i]), 0);
        lv_obj_set_style_border_width(sw, S(2), 0);
        lv_obj_set_style_border_color(sw, lv_color_hex(0x555555), 0);
        lv_obj_add_event_cb(sw, [](lv_event_t* e) {
            brush_color = lv_color_hex((uint32_t)(uintptr_t)lv_event_get_user_data(e));
        }, LV_EVENT_CLICKED, (void*)(uintptr_t)PALETTE[i]);
    }

    lv_obj_t* bar2 = lv_obj_create(tab);
    lv_obj_set_size(bar2, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(bar2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bar2, 0, 0);
    lv_obj_set_style_pad_all(bar2, 0, 0);
    lv_obj_set_style_pad_column(bar2, S(10), 0);
    lv_obj_set_flex_flow(bar2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar2, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(bar2, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* size_slider = lv_slider_create(bar2);
    lv_obj_set_flex_grow(size_slider, 1);
    lv_slider_set_range(size_slider, S(3), S(28));
    lv_slider_set_value(size_slider, brush_size, LV_ANIM_OFF);
    lv_obj_add_event_cb(size_slider, [](lv_event_t* e) {
        brush_size = lv_slider_get_value((lv_obj_t*)lv_event_get_target(e));
    }, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t* clear_btn = lv_button_create(bar2);
    lv_obj_t* lbl = lv_label_create(clear_btn);
    lv_label_set_text(lbl, LV_SYMBOL_TRASH);
    lv_obj_center(lbl);
    lv_obj_add_event_cb(clear_btn, [](lv_event_t*) { canvas_clear(); },
                        LV_EVENT_CLICKED, nullptr);
}
