#pragma once
// Minimal Arduino.h stub for QuickPID compatibility under ESP-IDF.
#include <stdint.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static inline uint32_t micros(void) {
    return (uint32_t)esp_timer_get_time();
}

static inline uint32_t millis(void) {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static inline void delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

template <typename T>
static inline constexpr T constrain(T value, T low, T high) {
    return (value < low) ? low : ((value > high) ? high : value);
}
