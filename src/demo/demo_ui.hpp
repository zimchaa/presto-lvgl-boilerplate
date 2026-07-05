// Shared bits for the demo UI tabs.
//
// The UI is designed at 480x480; S() scales coordinates down for the
// 240x240 (pixel-doubled) build so both modes get a sensible layout.
#pragma once

#include "lvgl.h"
#include "display_config.hpp"

inline constexpr int32_t S(int32_t px_at_480) {
    return px_at_480 * DISPLAY_WIDTH / 480;
}

#if PRESTO_FULL_RES
#define DEMO_FONT_DEF   (&lv_font_montserrat_20)
#define DEMO_FONT_BIG   (&lv_font_montserrat_28)
#define DEMO_FONT_SMALL (&lv_font_montserrat_14)
#else
#define DEMO_FONT_DEF   (&lv_font_montserrat_14)
#define DEMO_FONT_BIG   (&lv_font_montserrat_20)
#define DEMO_FONT_SMALL (&lv_font_montserrat_14)
#endif

// A titled "card": full-width rounded container laid out as a flex column.
inline lv_obj_t* demo_card(lv_obj_t* parent, const char* title) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1e232b), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, S(12), 0);
    lv_obj_set_style_pad_all(card, S(12), 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, S(8), 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    if (title) {
        lv_obj_t* t = lv_label_create(card);
        lv_label_set_text(t, title);
        lv_obj_set_style_text_color(t, lv_color_hex(0x8a93a6), 0);
        lv_obj_set_style_text_font(t, DEMO_FONT_SMALL, 0);
    }
    return card;
}

// Each tab module builds its content into the (already scrollable) tab page.
void ui_home_create(lv_obj_t* tab);
void ui_leds_create(lv_obj_t* tab);
void ui_sound_create(lv_obj_t* tab);
void ui_sound_silence();      // kill continuous tones (tab switched away)
void ui_paint_create(lv_obj_t* tab);
void ui_wifi_create(lv_obj_t* tab);
void ui_system_create(lv_obj_t* tab);
