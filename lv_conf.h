/**
 * lv_conf.h — LVGL configuration for the Pimoroni Presto boilerplate.
 *
 * Only deviations from LVGL defaults are set here; everything else falls
 * back to the defaults in lvgl/src/lv_conf_internal.h.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

/* Framebuffer is RGB565 (byte-swapped at flush time for the ST7701) */
#define LV_COLOR_DEPTH 16

/* LVGL heap for widgets/styles/draw tasks (RP2350 has 520K SRAM) */
#define LV_MEM_SIZE (48 * 1024U)

/* 240x240 logical resolution, hardware pixel-doubled to the 480x480 panel */
#define LV_DPI_DEF 87

/* Keep warnings visible over USB serial while developing */
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

/* A few font sizes to play with (default is montserrat_14) */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1

#endif /* LV_CONF_H */
