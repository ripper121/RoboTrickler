#pragma once

#include "cJSON.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t storage_utils_read_text_file(const char *path, const char *log_tag, char **out_buf);
esp_err_t storage_utils_write_text_file(const char *path, const char *data, const char *log_tag);
esp_err_t storage_utils_parse_json_file(const char *path, const char *log_tag, cJSON **out_root);

#ifdef __cplusplus
}
#endif
