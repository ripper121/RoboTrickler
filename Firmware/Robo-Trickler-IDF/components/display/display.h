#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t display_init(void);
bool display_lock(uint32_t timeout_ms);
void display_unlock(void);

#ifdef __cplusplus
}
#endif
