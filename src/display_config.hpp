// Display mode configuration, shared by main.cpp and lvgl_port.cpp.
//
// Default: 240x240 logical resolution; the ST7701 driver pixel-doubles to
// the physical 480x480 panel, and the whole display stack lives in SRAM.
//
// Build with PRESTO_FULL_RES=1 (cmake -DPRESTO_FULL_RES=ON) for native
// 480x480: sharper text and images, at the cost of 4x the pixels to render
// and a front buffer that has to live in PSRAM (a 450KB RGB565 buffer no
// longer fits in SRAM beside the 450KB scanout buffer).
#pragma once

#include <cstdint>

#if PRESTO_FULL_RES

inline constexpr uint16_t DISPLAY_WIDTH  = 480;
inline constexpr uint16_t DISPLAY_HEIGHT = 480;
// SRAM is nearly exhausted by the 450KB scanout buffer, so the LVGL stripe
// buffers shrink: 480x24 px x 2 bytes = 22.5KB each.
inline constexpr uint32_t LVGL_STRIPE_LINES = 24;

#else

inline constexpr uint16_t DISPLAY_WIDTH  = 240;
inline constexpr uint16_t DISPLAY_HEIGHT = 240;
// 240x60 px x 2 bytes = 28.8KB each.
inline constexpr uint32_t LVGL_STRIPE_LINES = 60;

#endif
