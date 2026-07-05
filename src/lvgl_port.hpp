// LVGL <-> Presto glue: display flush + FT6236 touch input.
#pragma once

#include "lvgl.h"
#include "libraries/pico_graphics/pico_graphics.hpp"
#include "drivers/st7701/st7701.hpp"

// Initialise LVGL with the Presto display (already init()ed — see main.cpp
// for the core1 arrangement) and the FT6236 capacitive touch controller.
// front_gfx wraps the front buffer that presto->update() copies to the
// scanout back buffer.
void lvgl_port_init(pimoroni::ST7701* presto, pimoroni::PicoGraphics_PenRGB565* front_gfx);

// Latest touch state in 240x240 logical coordinates (for debugging/display).
bool lvgl_port_touch_state(uint16_t* x, uint16_t* y);
