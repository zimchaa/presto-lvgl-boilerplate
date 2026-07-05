// The Presto's 7 ambient WS2812 LEDs (GPIO 33), via the Pimoroni plasma
// driver. They sit on PIO2 so they can't interact with the ST7701 scanout
// (PIO1) or the CYW43 PIO-SPI (which claims a free SM on PIO0/1): GPIO 33
// needs the PIO's GPIO base moved to 16, and that setting is per-PIO.
#include "demo_hw.hpp"

#include "ws2812.hpp"

static const uint LED_PIN = 33;

static plasma::WS2812::RGB led_buf[HW_NUM_LEDS];
static plasma::WS2812* strip = nullptr;

void hw_leds_init() {
    static plasma::WS2812 ws(HW_NUM_LEDS, pio2, 0, LED_PIN,
                             plasma::WS2812::DEFAULT_SERIAL_FREQ, false,
                             plasma::WS2812::COLOR_ORDER::GRB, led_buf);
    strip = &ws;
    hw_leds_show();   // all off
}

void hw_leds_set(uint32_t i, uint8_t r, uint8_t g, uint8_t b) {
    if (i < HW_NUM_LEDS) led_buf[i].rgb(r, g, b);
}

void hw_leds_show() {
    if (strip) strip->update(true);
}
