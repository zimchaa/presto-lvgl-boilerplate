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

#if PRESTO_FULL_RES

/* Native 480x480: the scanout buffer eats nearly all of SRAM, so LVGL's
 * heap moves to PSRAM (pool provided by lvgl_port.cpp). PSRAM is 8MB, so
 * size it generously — the kitchen-sink demo builds a six-tab UI. */
#define LV_MEM_SIZE (192 * 1024U)
#define LV_MEM_POOL_INCLUDE "lvgl_psram_pool.h"
#define LV_MEM_POOL_ALLOC lvgl_psram_pool

#define LV_DPI_DEF 174

#else

/* LVGL heap for widgets/styles/draw tasks (RP2350 has 520K SRAM).
 * 64K leaves room for the kitchen-sink demo's six-tab UI. */
#define LV_MEM_SIZE (64 * 1024U)

/* 240x240 logical resolution, hardware pixel-doubled to the 480x480 panel */
#define LV_DPI_DEF 87

#endif /* PRESTO_FULL_RES */

/* Keep warnings visible over USB serial while developing */
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

/* A few font sizes to play with (default is montserrat_14) */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1

#endif /* LV_CONF_H */
