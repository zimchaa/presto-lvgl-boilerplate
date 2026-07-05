// LED tab — drives the 7 ambient WS2812 LEDs behind the display.
// A row of lv_led widgets mirrors the physical strip; a roller picks an
// effect, RGB sliders set the colour for the solid mode, and a brightness
// slider scales everything.
#include "demo_ui.hpp"
#include "demo_hw.hpp"

#include <cmath>

enum led_mode : uint32_t { MODE_OFF, MODE_SOLID, MODE_RAINBOW, MODE_CHASE, MODE_BREATHE };

static uint32_t s_mode = MODE_RAINBOW;
static uint8_t  s_r = 0, s_g = 180, s_b = 255;
static uint8_t  s_brightness = 128;

static lv_obj_t* mirror[HW_NUM_LEDS];
static lv_obj_t* swatch;

// Effect engine: computes 7 RGB values each tick, pushes them to the strip
// and to the on-screen mirror widgets.
static void led_tick(lv_timer_t*) {
    uint32_t t = lv_tick_get();

    for (uint32_t i = 0; i < HW_NUM_LEDS; i++) {
        lv_color_t c = lv_color_black();
        switch (s_mode) {
            case MODE_OFF:
                break;
            case MODE_SOLID:
                c = lv_color_make(s_r, s_g, s_b);
                break;
            case MODE_RAINBOW: {
                uint16_t hue = (t / 10 + i * (360 / HW_NUM_LEDS)) % 360;
                c = lv_color_hsv_to_rgb(hue, 100, 100);
                break;
            }
            case MODE_CHASE: {
                // A bright dot sweeping back and forth with a decaying tail.
                float pos = (float)((t / 2) % 2400) / 200.0f;       // 0..12
                if (pos > 6.0f) pos = 12.0f - pos;                   // bounce
                float d = fabsf((float)i - pos);
                uint8_t v = d < 2.0f ? (uint8_t)(255.0f * (1.0f - d / 2.0f)) : 0;
                c = lv_color_make((uint16_t)s_r * v / 255,
                                  (uint16_t)s_g * v / 255,
                                  (uint16_t)s_b * v / 255);
                break;
            }
            case MODE_BREATHE: {
                float phase = (float)(t % 3000) / 3000.0f * 2.0f * (float)M_PI;
                float v = 0.5f - 0.5f * cosf(phase);
                c = lv_color_make((uint8_t)(s_r * v), (uint8_t)(s_g * v), (uint8_t)(s_b * v));
                break;
            }
        }

        hw_leds_set(i, (uint16_t)c.red   * s_brightness / 255,
                       (uint16_t)c.green * s_brightness / 255,
                       (uint16_t)c.blue  * s_brightness / 255);

        bool lit = c.red | c.green | c.blue;
        lv_led_set_color(mirror[i], lit ? c : lv_color_hex(0x202020));
        lv_led_set_brightness(mirror[i], lit ? LV_LED_BRIGHT_MAX : LV_LED_BRIGHT_MIN);
    }
    hw_leds_show();
}

static void update_swatch() {
    lv_obj_set_style_bg_color(swatch, lv_color_make(s_r, s_g, s_b), 0);
}

static lv_obj_t* rgb_slider(lv_obj_t* parent, lv_palette_t pal, uint8_t init,
                            void (*cb)(lv_event_t*)) {
    lv_obj_t* sl = lv_slider_create(parent);
    lv_obj_set_width(sl, lv_pct(96));
    lv_slider_set_range(sl, 0, 255);
    lv_slider_set_value(sl, init, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(sl, lv_palette_main(pal), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sl, lv_palette_main(pal), LV_PART_KNOB);
    lv_obj_add_event_cb(sl, cb, LV_EVENT_VALUE_CHANGED, nullptr);
    return sl;
}

void ui_leds_create(lv_obj_t* tab) {
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(tab, S(10), 0);

    // Mirror of the physical strip.
    lv_obj_t* strip_card = demo_card(tab, "7x WS2812 (GPIO 33)");
    lv_obj_t* row = lv_obj_create(strip_card);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, S(4), 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    for (uint32_t i = 0; i < HW_NUM_LEDS; i++) {
        mirror[i] = lv_led_create(row);
        lv_obj_set_size(mirror[i], S(36), S(36));
        lv_led_off(mirror[i]);
    }

    // Effect selection.
    lv_obj_t* mode_card = demo_card(tab, "effect");
    lv_obj_t* roller = lv_roller_create(mode_card);
    lv_roller_set_options(roller, "Off\nSolid\nRainbow\nChase\nBreathe",
                          LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(roller, 3);
    lv_obj_set_width(roller, lv_pct(100));
    lv_roller_set_selected(roller, s_mode, LV_ANIM_OFF);
    lv_obj_add_event_cb(roller, [](lv_event_t* e) {
        s_mode = lv_roller_get_selected((lv_obj_t*)lv_event_get_target(e));
    }, LV_EVENT_VALUE_CHANGED, nullptr);

    // Colour + brightness.
    lv_obj_t* col_card = demo_card(tab, "colour (solid / chase / breathe)");

    swatch = lv_obj_create(col_card);
    lv_obj_set_size(swatch, lv_pct(96), S(28));
    lv_obj_set_style_radius(swatch, S(8), 0);
    lv_obj_set_style_border_width(swatch, 0, 0);
    lv_obj_remove_flag(swatch, LV_OBJ_FLAG_SCROLLABLE);
    update_swatch();

    rgb_slider(col_card, LV_PALETTE_RED, s_r, [](lv_event_t* e) {
        s_r = (uint8_t)lv_slider_get_value((lv_obj_t*)lv_event_get_target(e));
        update_swatch();
    });
    rgb_slider(col_card, LV_PALETTE_GREEN, s_g, [](lv_event_t* e) {
        s_g = (uint8_t)lv_slider_get_value((lv_obj_t*)lv_event_get_target(e));
        update_swatch();
    });
    rgb_slider(col_card, LV_PALETTE_BLUE, s_b, [](lv_event_t* e) {
        s_b = (uint8_t)lv_slider_get_value((lv_obj_t*)lv_event_get_target(e));
        update_swatch();
    });

    lv_obj_t* bri_card = demo_card(tab, "brightness");
    lv_obj_t* bri = lv_slider_create(bri_card);
    lv_obj_set_width(bri, lv_pct(96));
    lv_slider_set_range(bri, 0, 255);
    lv_slider_set_value(bri, s_brightness, LV_ANIM_OFF);
    lv_obj_add_event_cb(bri, [](lv_event_t* e) {
        s_brightness = (uint8_t)lv_slider_get_value((lv_obj_t*)lv_event_get_target(e));
    }, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_timer_create(led_tick, 33, nullptr);
}
