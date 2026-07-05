// Presto kitchen-sink demo — exercises every bit of hardware on the board
// behind a tabbed LVGL interface:
//
//   tab            hardware                          LVGL features
//   ─────────────  ────────────────────────────────  ─────────────────────────
//   Home           display + touch                   scale/needle, lv_anim,
//                                                    live chart, input widgets
//   LEDs           7x WS2812 (GPIO 33, PIO2 + DMA)   lv_led, roller, sliders
//   Sound          piezo buzzer (PWM, GPIO 43)       button events, lv_anim
//                                                    driving hardware
//   Paint          FT6236 touch, PSRAM canvas        lv_canvas + draw layers
//   WiFi           RM2/CYW43439 (PIO-SPI)            lv_list, async UI updates
//   System         die temp (ADC), microSD (SPI0),   bars, timers, cards
//                  backlight PWM, PSRAM, unique ID
//
// The boot/core-split arrangement is identical to the plain demo (main.cpp):
// core 1 owns the ST7701 scanout, core 0 runs LVGL and everything else.
#include "libraries/pico_graphics/pico_graphics.hpp"
#include "drivers/st7701/st7701.hpp"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "hardware/psram.h"
#include "pico/platform/sections.h"
#include <cstdio>

#include "lvgl.h"
#include "lvgl_port.hpp"
#include "display_config.hpp"
#include "demo_hw.hpp"
#include "demo_ui.hpp"

using namespace pimoroni;

static const uint FRAME_WIDTH  = DISPLAY_WIDTH;
static const uint FRAME_HEIGHT = DISPLAY_HEIGHT;

static const uint BACKLIGHT = 45;
static const uint LCD_CLK = 26;
static const uint LCD_CS  = 28;
static const uint LCD_DAT = 27;
static const uint LCD_DC  = -1;

// Scanout buffer must stay in SRAM (see main.cpp for the full story).
static uint16_t back_buffer[FRAME_WIDTH * FRAME_HEIGHT];
#if PRESTO_FULL_RES
static uint16_t __uninitialized_psram("framebuffer") front_buffer[FRAME_WIDTH * FRAME_HEIGHT];
#else
static uint16_t front_buffer[FRAME_WIDTH * FRAME_HEIGHT];
#endif

static ST7701* g_presto = nullptr;

static void core1_entry() {
    g_presto->init();
    multicore_fifo_push_blocking(1);
    while (true) {
        tight_loop_contents();
    }
}

static void build_ui() {
    lv_display_t* disp = lv_display_get_default();
    lv_theme_t* theme = lv_theme_default_init(
        disp, lv_palette_main(LV_PALETTE_CYAN), lv_palette_main(LV_PALETTE_PINK),
        true /* dark */, DEMO_FONT_DEF);
    lv_display_set_theme(disp, theme);

    lv_obj_t* tv = lv_tabview_create(lv_screen_active());
    lv_tabview_set_tab_bar_position(tv, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(tv, S(52));

    // Swiping between tabs is disabled: the Paint tab needs drag gestures
    // for brush strokes. The tab buttons still switch pages.
    lv_obj_remove_flag(lv_tabview_get_content(tv), LV_OBJ_FLAG_SCROLLABLE);

    ui_home_create  (lv_tabview_add_tab(tv, LV_SYMBOL_HOME));
    ui_leds_create  (lv_tabview_add_tab(tv, LV_SYMBOL_TINT));
    ui_sound_create (lv_tabview_add_tab(tv, LV_SYMBOL_AUDIO));
    ui_paint_create (lv_tabview_add_tab(tv, LV_SYMBOL_EDIT));
    ui_wifi_create  (lv_tabview_add_tab(tv, LV_SYMBOL_WIFI));
    ui_system_create(lv_tabview_add_tab(tv, LV_SYMBOL_SETTINGS));

    lv_obj_add_event_cb(tv, [](lv_event_t*) {
        ui_sound_silence();               // don't leave a tone running
        hw_buzzer_tone(1200, 0.2f);       // click blip on tab switch
        lv_timer_create([](lv_timer_t* t) {
            hw_buzzer_tone(0);
            lv_timer_delete(t);
        }, 30, nullptr);
    }, LV_EVENT_VALUE_CHANGED, nullptr);
}

int main() {
    stdio_init_all();

    printf("\n=== Presto kitchen-sink demo ===\n");
    printf("sys_clk = %lu Hz, psram = %u bytes\n",
           (unsigned long)clock_get_hz(clk_sys), (unsigned)psram_get_size());

    gpio_init(LCD_CS);
    gpio_set_dir(LCD_CS, GPIO_OUT);
    gpio_put(LCD_CS, 1);

    static ST7701 presto(FRAME_WIDTH, FRAME_HEIGHT, ROTATE_0,
                         SPIPins{spi1, LCD_CS, LCD_CLK, LCD_DAT, PIN_UNUSED, LCD_DC, BACKLIGHT},
                         back_buffer);
    static PicoGraphics_PenRGB565 gfx(FRAME_WIDTH, FRAME_HEIGHT, front_buffer);
    g_presto = &presto;

    multicore_launch_core1(core1_entry);
    multicore_fifo_pop_blocking();
    presto.set_backlight(255);

    hw_leds_init();
    hw_buzzer_init();
    bool wifi_ok = hw_wifi_init();        // ~300ms: uploads CYW43 firmware
    printf("wifi: %s\n", wifi_ok ? "up" : "FAILED");

    lvgl_port_init(&presto, &gfx);
    build_ui();
    printf("LVGL up\n");

    while (true) {
        uint32_t wait_ms = lv_timer_handler();
        sleep_ms(wait_ms > 10 ? 10 : wait_ms);
    }
}
