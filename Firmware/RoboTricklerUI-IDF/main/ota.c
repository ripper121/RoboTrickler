#include "ota.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "board.h"
#include "esp_check.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "storage.h"

#define RT_OTA_SD_BUF_SIZE 1024

static char s_last_error[128];
static esp_ota_handle_t s_handle;
static const esp_partition_t *s_partition;

static void set_error(const char *text, esp_err_t err)
{
    snprintf(s_last_error, sizeof(s_last_error), "%s: %s", text, esp_err_to_name(err));
}

const char *rt_ota_last_error(void)
{
    return s_last_error;
}

esp_err_t rt_ota_upload_begin(size_t image_size)
{
    s_partition = esp_ota_get_next_update_partition(NULL);
    if (!s_partition) {
        strlcpy(s_last_error, "no OTA partition", sizeof(s_last_error));
        return ESP_FAIL;
    }
    esp_err_t ret = esp_ota_begin(s_partition, image_size > 0 ? image_size : OTA_SIZE_UNKNOWN, &s_handle);
    if (ret != ESP_OK) {
        set_error("ota begin failed", ret);
    }
    return ret;
}

esp_err_t rt_ota_upload_write(const void *data, size_t len)
{
    if (len == 0) {
        return ESP_OK;
    }
    esp_err_t ret = esp_ota_write(s_handle, data, len);
    if (ret != ESP_OK) {
        set_error("ota write failed", ret);
    }
    return ret;
}

esp_err_t rt_ota_upload_finish(void)
{
    esp_err_t ret = esp_ota_end(s_handle);
    if (ret != ESP_OK) {
        set_error("ota end failed", ret);
        return ret;
    }
    ret = esp_ota_set_boot_partition(s_partition);
    if (ret != ESP_OK) {
        set_error("ota set boot failed", ret);
    }
    return ret;
}

esp_err_t rt_ota_from_sd(void)
{
    char path[96];
    snprintf(path, sizeof(path), "%s/update.bin", RT_SD_MOUNT_POINT);
    FILE *f = fopen(path, "rb");
    if (!f) {
        return ESP_ERR_NOT_FOUND;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    if (size <= 0) {
        fclose(f);
        strlcpy(s_last_error, "empty update.bin", sizeof(s_last_error));
        return ESP_FAIL;
    }
    esp_err_t ret = rt_ota_upload_begin((size_t)size);
    if (ret != ESP_OK) {
        fclose(f);
        return ret;
    }
    uint8_t *buf = malloc(RT_OTA_SD_BUF_SIZE);
    if (!buf) {
        esp_ota_abort(s_handle);
        fclose(f);
        strlcpy(s_last_error, "ota buffer alloc failed", sizeof(s_last_error));
        return ESP_ERR_NO_MEM;
    }
    while (!feof(f)) {
        size_t n = fread(buf, 1, RT_OTA_SD_BUF_SIZE, f);
        if (n > 0) {
            ret = rt_ota_upload_write(buf, n);
            if (ret != ESP_OK) {
                esp_ota_abort(s_handle);
                free(buf);
                fclose(f);
                return ret;
            }
        }
    }
    if (ferror(f)) {
        esp_ota_abort(s_handle);
        free(buf);
        fclose(f);
        strlcpy(s_last_error, "ota read failed", sizeof(s_last_error));
        return ESP_FAIL;
    }
    free(buf);
    fclose(f);
    ret = rt_ota_upload_finish();
    if (ret == ESP_OK) {
        unlink(path);
        esp_restart();
    }
    return ret;
}
