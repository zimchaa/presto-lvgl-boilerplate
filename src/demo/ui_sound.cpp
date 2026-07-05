// Sound tab — the piezo buzzer (PWM on GPIO 43).
// A one-octave piano (tone while a key is held), a manual tone slider, and
// a "siren" that drives the buzzer frequency from an LVGL animation.
#include "demo_ui.hpp"
#include "demo_hw.hpp"

#include "pico.h"   // count_of

static const struct { const char* name; uint16_t freq; } NOTES[] = {
    {"C", 523}, {"D", 587}, {"E", 659}, {"F", 698},
    {"G", 784}, {"A", 880}, {"B", 988}, {"C", 1047},
};

static lv_obj_t* tone_switch;
static lv_obj_t* tone_slider;
static lv_obj_t* siren_btn;

static void stop_continuous_sounds() {
    lv_obj_remove_state(tone_switch, LV_STATE_CHECKED);
    lv_obj_remove_state(siren_btn, LV_STATE_CHECKED);
    lv_anim_delete(siren_btn, nullptr);
    hw_buzzer_tone(0);
}

static void build_piano(lv_obj_t* parent) {
    lv_obj_t* card = demo_card(parent, "piano — hold a key");

    lv_obj_t* row = lv_obj_create(card);
    lv_obj_set_size(row, lv_pct(100), S(120));
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_column(row, S(4), 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    for (size_t i = 0; i < count_of(NOTES); i++) {
        lv_obj_t* key = lv_button_create(row);
        lv_obj_set_height(key, lv_pct(100));
        lv_obj_set_flex_grow(key, 1);
        lv_obj_set_style_bg_color(key, lv_color_hex(0xe8e8e8), 0);
        lv_obj_set_style_bg_color(key, lv_palette_main(LV_PALETTE_CYAN), LV_STATE_PRESSED);
        lv_obj_set_style_radius(key, S(6), 0);

        lv_obj_t* lbl = lv_label_create(key);
        lv_label_set_text(lbl, NOTES[i].name);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x303030), 0);
        lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, 0);

        lv_obj_add_event_cb(key, [](lv_event_t* e) {
            stop_continuous_sounds();
            hw_buzzer_tone((float)(uintptr_t)lv_event_get_user_data(e));
        }, LV_EVENT_PRESSED, (void*)(uintptr_t)NOTES[i].freq);
        lv_obj_add_event_cb(key, [](lv_event_t*) {
            hw_buzzer_tone(0);
        }, LV_EVENT_RELEASED, nullptr);
    }
}

static void build_tone(lv_obj_t* parent) {
    lv_obj_t* card = demo_card(parent, "tone generator");

    lv_obj_t* row = lv_obj_create(card);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* freq_label = lv_label_create(row);
    lv_label_set_text(freq_label, "440 Hz");

    tone_switch = lv_switch_create(row);
    lv_obj_add_event_cb(tone_switch, [](lv_event_t* e) {
        lv_obj_t* sw = (lv_obj_t*)lv_event_get_target(e);
        if (lv_obj_has_state(sw, LV_STATE_CHECKED)) {
            lv_obj_remove_state(siren_btn, LV_STATE_CHECKED);
            lv_anim_delete(siren_btn, nullptr);
            hw_buzzer_tone((float)lv_slider_get_value(tone_slider));
        } else {
            hw_buzzer_tone(0);
        }
    }, LV_EVENT_VALUE_CHANGED, nullptr);

    tone_slider = lv_slider_create(card);
    lv_obj_set_width(tone_slider, lv_pct(96));
    lv_slider_set_range(tone_slider, 100, 2000);
    lv_slider_set_value(tone_slider, 440, LV_ANIM_OFF);
    lv_obj_add_event_cb(tone_slider, [](lv_event_t* e) {
        int32_t f = lv_slider_get_value((lv_obj_t*)lv_event_get_target(e));
        lv_label_set_text_fmt((lv_obj_t*)lv_event_get_user_data(e), "%ld Hz", (long)f);
        if (lv_obj_has_state(tone_switch, LV_STATE_CHECKED)) hw_buzzer_tone((float)f);
    }, LV_EVENT_VALUE_CHANGED, freq_label);
}

static void build_siren(lv_obj_t* parent) {
    lv_obj_t* card = demo_card(parent, "lv_anim driving hardware");

    siren_btn = lv_button_create(card);
    lv_obj_add_flag(siren_btn, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_width(siren_btn, lv_pct(100));
    lv_obj_t* lbl = lv_label_create(siren_btn);
    lv_label_set_text(lbl, LV_SYMBOL_BELL "  Siren");
    lv_obj_center(lbl);

    lv_obj_add_event_cb(siren_btn, [](lv_event_t* e) {
        lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
        if (lv_obj_has_state(btn, LV_STATE_CHECKED)) {
            lv_obj_remove_state(tone_switch, LV_STATE_CHECKED);
            // The animation's "variable" is the buzzer itself: each animated
            // value becomes a PWM frequency.
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, btn);
            lv_anim_set_exec_cb(&a, [](void*, int32_t v) { hw_buzzer_tone((float)v, 0.3f); });
            lv_anim_set_values(&a, 400, 1200);
            lv_anim_set_duration(&a, 700);
            lv_anim_set_playback_duration(&a, 700);
            lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
            lv_anim_start(&a);
        } else {
            lv_anim_delete(btn, nullptr);
            hw_buzzer_tone(0);
        }
    }, LV_EVENT_VALUE_CHANGED, nullptr);
}

void ui_sound_create(lv_obj_t* tab) {
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(tab, S(10), 0);

    build_piano(tab);
    build_tone(tab);
    build_siren(tab);
}

// Called by demo_main when the user switches tabs.
void ui_sound_silence() {
    if (tone_switch) stop_continuous_sounds();
}
