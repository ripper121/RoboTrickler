#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t shift595_init(void);
esp_err_t shift595_set_bit(uint8_t bit, bool level);
esp_err_t shift595_pulse_bit(uint8_t bit, uint32_t high_us, uint32_t low_us);
esp_err_t shift595_write(uint32_t state);
uint32_t shift595_get_state(void);

#ifdef __cplusplus
}
#endif
