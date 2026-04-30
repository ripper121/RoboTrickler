#pragma once

#include <stddef.h>
#include "esp_err.h"

esp_err_t rt_ota_from_sd(void);
esp_err_t rt_ota_upload_begin(size_t image_size);
esp_err_t rt_ota_upload_write(const void *data, size_t len);
esp_err_t rt_ota_upload_finish(void);
const char *rt_ota_last_error(void);
