#include "shift_register.h"
#include "pindef.h"
#include "driver/gpio.h"

// Shadow register — tracks the current bit state of all 8 output pins
static uint8_t sr_shadow = 0;

// ============================================================
// Internal: clock one byte out MSB-first (shiftOut equivalent)
// ============================================================
static void shift_out_byte(uint8_t val) {
    for (int i = 7; i >= 0; i--) {
        gpio_set_level(SR_DATA_PIN, (val >> i) & 0x01);
        gpio_set_level(SR_CLK_PIN, 1);
        gpio_set_level(SR_CLK_PIN, 0);
    }
}

// ============================================================
// Push the shadow register to the hardware
// ============================================================
void sr_update(void) {
    shift_out_byte(sr_shadow);
    // Latch: rising edge then falling edge
    gpio_set_level(SR_LATCH_PIN, 1);
    gpio_set_level(SR_LATCH_PIN, 0);
}

// ============================================================
// Public API
// ============================================================
void sr_init(void) {
    gpio_config_t io = {};
    io.mode = GPIO_MODE_OUTPUT;
    io.pin_bit_mask = (1ULL << SR_DATA_PIN) |
                      (1ULL << SR_CLK_PIN)  |
                      (1ULL << SR_LATCH_PIN);
    gpio_config(&io);

    gpio_set_level(SR_CLK_PIN,   0);
    gpio_set_level(SR_DATA_PIN,  0);
    gpio_set_level(SR_LATCH_PIN, 0);

    sr_shadow = 0;
    sr_update();
}

void sr_set(uint8_t bit_index, uint8_t value) {
    if (value) {
        sr_shadow |=  (1u << bit_index);
    } else {
        sr_shadow &= ~(1u << bit_index);
    }
    sr_update();
}

void sr_set_all_low(void) {
    sr_shadow = 0;
    sr_update();
}

void sr_set_all_high(void) {
    sr_shadow = 0xFF;
    sr_update();
}
