#pragma once

// ============================================================
// Minimal Arduino API compatibility layer for ESP-IDF
// ============================================================

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string>
#include <cstdio>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_attr.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "driver/gpio.h"

// ---------- Logic levels ----------
#ifndef HIGH
#define HIGH 1
#endif
#ifndef LOW
#define LOW  0
#endif

// ---------- Primitive types ----------
typedef uint8_t  byte;

// ---------- Timing ----------
static inline uint32_t millis(void) {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}
static inline uint32_t micros(void) {
    return (uint32_t)(esp_timer_get_time());
}
static inline void delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}
static inline void delayMicroseconds(uint32_t us) {
    // Busy-wait for short delays
    uint32_t start = micros();
    while ((micros() - start) < us);
}

// ---------- Yielding ----------
static inline void yield(void) {
    taskYIELD();
}

// ---------- GPIO ----------
static inline void pinMode(gpio_num_t pin, gpio_mode_t mode) {
    gpio_set_direction(pin, mode);
}
static inline void digitalWrite(gpio_num_t pin, int val) {
    gpio_set_level(pin, val);
}
static inline int digitalRead(gpio_num_t pin) {
    return gpio_get_level(pin);
}

// Convenience: allow int pin (same as gpio_num_t cast)
static inline void pinMode(int pin, gpio_mode_t mode) {
    gpio_set_direction((gpio_num_t)pin, mode);
}
static inline void digitalWrite(int pin, int val) {
    gpio_set_level((gpio_num_t)pin, val);
}
static inline int digitalRead(int pin) {
    return gpio_get_level((gpio_num_t)pin);
}

// ---------- ESP system helpers ----------
static inline void esp_restart_wrapper(void) { esp_restart(); }

#define ESP_FREE_HEAP()      heap_caps_get_free_size(MALLOC_CAP_DEFAULT)
#define ESP_MIN_FREE_HEAP()  heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT)
#define ESP_HEAP_SIZE()      heap_caps_get_total_size(MALLOC_CAP_DEFAULT)
#define ESP_MAX_ALLOC_HEAP() heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT)

// ---------- std::string helper utilities ----------
// These provide the Arduino String-style operations that the codebase uses.

#include <sstream>
#include <iomanip>

static inline std::string str_float(float val, int decimals) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(decimals) << val;
    return oss.str();
}

static inline float str_to_float(const std::string &s) {
    return strtof(s.c_str(), nullptr);
}
static inline int str_to_int(const std::string &s) {
    return (int)strtol(s.c_str(), nullptr, 10);
}
static inline bool str_ends_with(const std::string &s, const char *suffix) {
    size_t sl = strlen(suffix);
    if (s.size() < sl) return false;
    return s.compare(s.size() - sl, sl, suffix) == 0;
}
static inline bool str_starts_with(const std::string &s, const char *prefix) {
    return s.rfind(prefix, 0) == 0;
}
static inline std::string str_replace(std::string s, const std::string &from, const std::string &to) {
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

// Bit manipulation (used in shift register)
#ifndef bitSet
#define bitSet(value, bit)   ((value) |=  (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitRead(value, bit)  (((value) >> (bit)) & 0x01)
#endif

// MSBFIRST (used in shiftOut emulation)
#ifndef MSBFIRST
#define MSBFIRST 1
#define LSBFIRST 0
#endif

// OUTPUT / INPUT mode aliases
#ifndef OUTPUT
#define OUTPUT GPIO_MODE_OUTPUT
#endif
#ifndef INPUT
#define INPUT  GPIO_MODE_INPUT
#endif
