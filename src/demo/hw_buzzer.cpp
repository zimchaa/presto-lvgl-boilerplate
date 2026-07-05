// Piezo buzzer on GPIO 43, driven with hardware PWM.
#include "demo_hw.hpp"

#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"

static const uint BUZZER_PIN = 43;
static uint slice;

void hw_buzzer_init() {
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(BUZZER_PIN);
    pwm_set_enabled(slice, false);
}

void hw_buzzer_tone(float freq_hz, float duty) {
    if (freq_hz < 20.0f) {
        pwm_set_enabled(slice, false);
        gpio_put(BUZZER_PIN, 0);          // don't leave DC across the piezo
        return;
    }
    if (duty < 0.0f) duty = 0.0f;
    if (duty > 1.0f) duty = 1.0f;

    // Pick the smallest clock divider that fits the period in the 16-bit
    // counter, for maximum frequency resolution.
    float clk = (float)clock_get_hz(clk_sys);
    float div = clk / (freq_hz * 65536.0f);
    if (div < 1.0f) div = 1.0f;
    uint32_t wrap = (uint32_t)(clk / (div * freq_hz)) - 1;

    pwm_set_clkdiv(slice, div);
    pwm_set_wrap(slice, (uint16_t)wrap);
    pwm_set_gpio_level(BUZZER_PIN, (uint16_t)((float)wrap * duty));
    pwm_set_enabled(slice, true);
}
