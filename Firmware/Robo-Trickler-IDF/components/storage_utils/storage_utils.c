#include "storage_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

static const char *TAG = "storage_utils";

static const char *storage_utils_log_tag(const char *log_tag)
{
    return log_tag != NULL ? log_tag : TAG;
}

esp_err_t storage_utils_read_text_file(const char *path, const char *log_tag, char **out_buf)
{
    FILE *f = NULL;
    char *buf = NULL;
    long len = 0;
    size_t bytes_read = 0;
    const char *tag = storage_utils_log_tag(log_tag);

    if (path == NULL || out_buf == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_buf = NULL;

    f = fopen(path, "r");
    if (f == NULL) {
        ESP_LOGE(tag, "Cannot open %s", path);
        return ESP_ERR_NOT_FOUND;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        ESP_LOGE(tag, "Failed to seek %s", path);
        fclose(f);
        return ESP_FAIL;
    }

    len = ftell(f);
    if (len < 0) {
        ESP_LOGE(tag, "Failed to get size of %s", path);
        fclose(f);
        return ESP_FAIL;
    }

    rewind(f);

    buf = malloc((size_t)len + 1U);
    if (buf == NULL) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    bytes_read = fread(buf, 1, (size_t)len, f);
    fclose(f);

    if (bytes_read != (size_t)len) {
        ESP_LOGE(tag, "Short read on %s", path);
        free(buf);
        return ESP_FAIL;
    }

    buf[len] = '\0';
    *out_buf = buf;
    return ESP_OK;
}

esp_err_t storage_utils_write_text_file(const char *path, const char *data, const char *log_tag)
{
    FILE *f = NULL;
    size_t data_len = 0;
    size_t bytes_written = 0;
    const char *tag = storage_utils_log_tag(log_tag);

    if (path == NULL || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    f = fopen(path, "w");
    if (f == NULL) {
        ESP_LOGE(tag, "Cannot open %s for write", path);
        return ESP_FAIL;
    }

    data_len = strlen(data);
    bytes_written = fwrite(data, 1, data_len, f);
    if (bytes_written != data_len) {
        ESP_LOGE(tag, "Short write on %s", path);
        fclose(f);
        return ESP_FAIL;
    }

    if (fclose(f) != 0) {
        ESP_LOGE(tag, "Failed to close %s", path);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t storage_utils_parse_json_file(const char *path, const char *log_tag, cJSON **out_root)
{
    char *buf = NULL;
    cJSON *root = NULL;
    const char *tag = storage_utils_log_tag(log_tag);
    esp_err_t ret;

    if (path == NULL || out_root == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_root = NULL;

    ret = storage_utils_read_text_file(path, tag, &buf);
    if (ret != ESP_OK) {
        return ret;
    }

    root = cJSON_Parse(buf);
    free(buf);

    if (root == NULL) {
        ESP_LOGE(tag,
                 "JSON parse error in %s near: %s",
                 path,
                 cJSON_GetErrorPtr() != NULL ? cJSON_GetErrorPtr() : "?");
        return ESP_FAIL;
    }

    *out_root = root;
    return ESP_OK;
}
