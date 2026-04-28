#pragma once

#include <stddef.h>
#include <stdint.h>

#include "config.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SCALE_COMMAND_MAX_LEN 16U
#define SCALE_RESPONSE_MAX_LEN 128U

typedef struct scale_t *scale_handle_t;

esp_err_t scale_init(const config_scale_t *config, scale_handle_t *out_handle);
esp_err_t scale_delete(scale_handle_t handle);

esp_err_t scale_request(scale_handle_t handle, uint8_t *buffer, size_t buffer_size, size_t *out_len);
esp_err_t scale_read_unit(scale_handle_t handle, float *out_value);
esp_err_t scale_read_unit_stable(scale_handle_t handle, size_t sample_count, float *out_value);
esp_err_t scale_tare(scale_handle_t handle);

#ifdef __cplusplus
}
#endif
