// Pimoroni Presto + LVGL boilerplate
//
// Core split (mirrors the shipping MicroPython firmware):
//   core 1 — owns the ST7701 display. init() runs there, so the PIO scanout
//            interrupts (timing feed + end-of-line/frame) are serviced on
//            core 1, and nothing the application does can starve the panel.
//   core 0 — LVGL and your application.
//
// By default the panel runs at 240x240 logical resolution; the driver
// pixel-doubles in hardware to the physical 480x480, which is what lets the
// whole display stack (two 112.5KB framebuffers + LVGL) fit in on-chip SRAM.
// Building with -DPRESTO_FULL_RES=ON drives the panel at native 480x480:
// the scanout buffer (450KB) fills most of SRAM and the LVGL front buffer
// and heap move out to the 8MB QMI PSRAM (see display_config.hpp).
//
// Demo UI: a counter button, a spinner (so you can see the frame rate), and
// a live touch-coordinate readout.

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

using namespace pimoroni;

static const uint FRAME_WIDTH  = DISPLAY_WIDTH;
static const uint FRAME_HEIGHT = DISPLAY_HEIGHT;

static const uint BACKLIGHT = 45;
static const uint LCD_CLK = 26;
static const uint LCD_CS  = 28;
static const uint LCD_DAT = 27;
static const uint LCD_DC  = -1;

// The scanout buffer must stay in SRAM: the core1 PIO/DMA scanout cannot
// tolerate QMI (flash/PSRAM) bus latency without underrunning the panel.
static uint16_t back_buffer[FRAME_WIDTH * FRAME_HEIGHT];
#if PRESTO_FULL_RES
// At 480x480 the RGB565 front buffer (450KB) no longer fits in SRAM beside
// the scanout buffer, so it lives in PSRAM. Only core 0 touches it: LVGL
// flushes stripes in, presto->update() beam-races the copy out.
static uint16_t __uninitialized_psram("framebuffer") front_buffer[FRAME_WIDTH * FRAME_HEIGHT];
#else
static uint16_t front_buffer[FRAME_WIDTH * FRAME_HEIGHT];   // LVGL composits here
#endif

static ST7701* g_presto = nullptr;

// PSRAM (8MB QMI, brought up in runtime init) is memory-mapped; a variable
// placed there exercises the whole path: linker region, boot init, XIP writes.
static uint32_t __uninitialized_psram("probe") psram_probe[64];

static void psram_smoke_test() {
    printf("psram: %u bytes %s\n", (unsigned)psram_get_size(),
           psram_is_available() ? "available" : "NOT AVAILABLE");
    for (uint32_t i = 0; i < count_of(psram_probe); i++) psram_probe[i] = i * 0x01010101u;
    for (uint32_t i = 0; i < count_of(psram_probe); i++) {
        if (psram_probe[i] != i * 0x01010101u) {
            printf("psram: readback FAILED at %lu\n", (unsigned long)i);
            return;
        }
    }
    printf("psram: readback OK (%p)\n", (void*)psram_probe);
}

static void core1_entry() {
    g_presto->init();                    // display ISRs now live on core 1
    multicore_fifo_push_blocking(1);
    while (true) {
        tight_loop_contents();           // ISRs preempt this idle loop
    }
}

// ── Demo UI ──────────────────────────────────────────────────────────

static lv_obj_t* touch_label;

static void build_ui() {
    lv_obj_t* scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x14161a), 0);

    lv_obj_t* title = lv_label_create(scr);
    lv_label_set_text(title, "Presto + LVGL");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_t* spinner = lv_spinner_create(scr);
    lv_obj_set_size(spinner, 36, 36);
    lv_obj_align(spinner, LV_ALIGN_TOP_RIGHT, -8, 8);

    lv_obj_t* btn = lv_button_create(scr);
    lv_obj_set_size(btn, 150, 64);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "TAP ME");
    lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_20, 0);
    lv_obj_center(btn_label);

    lv_obj_add_event_cb(btn, [](lv_event_t* e) {
        static int count = 0;
        lv_obj_t* label = lv_obj_get_child((lv_obj_t*)lv_event_get_target(e), 0);
        lv_label_set_text_fmt(label, "%d taps", ++count);
    }, LV_EVENT_CLICKED, nullptr);

    touch_label = lv_label_create(scr);
    lv_label_set_text(touch_label, "touch the screen");
    lv_obj_set_style_text_color(touch_label, lv_color_hex(0x8a93a6), 0);
    lv_obj_align(touch_label, LV_ALIGN_BOTTOM_MID, 0, -12);

    lv_timer_create([](lv_timer_t*) {
        uint16_t x, y;
        if (lvgl_port_touch_state(&x, &y)) {
            lv_label_set_text_fmt(touch_label, "touch: %u, %u", x, y);
        }
    }, 100, nullptr);
}

int main() {
    // No set_sys_clock_khz(): the `presto` board header applies its tuned
    // 200MHz PLL at boot, which the ST7701 display timing depends on.
    stdio_init_all();

    printf("\n=== Presto + LVGL boilerplate ===\n");
    printf("sys_clk = %lu Hz\n", (unsigned long)clock_get_hz(clk_sys));
    psram_smoke_test();

    gpio_init(LCD_CS);
    gpio_set_dir(LCD_CS, GPIO_OUT);
    gpio_put(LCD_CS, 1);

    static ST7701 presto(FRAME_WIDTH, FRAME_HEIGHT, ROTATE_0,
                         SPIPins{spi1, LCD_CS, LCD_CLK, LCD_DAT, PIN_UNUSED, LCD_DC, BACKLIGHT},
                         back_buffer);
    static PicoGraphics_PenRGB565 gfx(FRAME_WIDTH, FRAME_HEIGHT, front_buffer);
    g_presto = &presto;

    multicore_launch_core1(core1_entry);
    multicore_fifo_pop_blocking();       // wait for display init on core 1
    presto.set_backlight(255);

    lvgl_port_init(&presto, &gfx);
    build_ui();
    printf("LVGL up\n");

    while (true) {
        uint32_t wait_ms = lv_timer_handler();
        sleep_ms(wait_ms > 10 ? 10 : wait_ms);
    }
}
