#include <stdio.h>
#include <inttypes.h>
#include "fw_update_sd.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_ota_ops.h"

static const char *TAG = "fw_update_sd";

#define CHUNK_SIZE  4096

esp_err_t fw_update_sd_apply(const char *bin_path, fw_update_sd_result_t *result)
{
    if (result) {
        *result = FW_UPDATE_SD_NOT_FOUND;
    }

    const char *path = bin_path ? bin_path : FW_UPDATE_SD_DEFAULT_PATH;

    /* Open the firmware binary */
    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGW(TAG, "No firmware file at %s", path);
        return ESP_OK;  /* Not an error — nothing to apply */
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0) {
        ESP_LOGE(TAG, "Firmware file at %s is empty", path);
        fclose(f);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Applying %s (%ld bytes)", path, file_size);

    /* Resolve the OTA partition to write into */
    const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
    if (!part) {
        ESP_LOGE(TAG, "No OTA partition available — check partition table");
        fclose(f);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Target partition: '%s' @ 0x%08" PRIx32, part->label, part->address);

    /* Begin OTA session */
    esp_ota_handle_t ota_handle = 0;
    esp_err_t err = esp_ota_begin(part, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin: %s", esp_err_to_name(err));
        fclose(f);
        return err;
    }

    /* Stream file → OTA partition in chunks */
    char *buf = malloc(CHUNK_SIZE);
    if (!buf) {
        esp_ota_abort(ota_handle);
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    long total = 0;
    size_t rd;
    while ((rd = fread(buf, 1, CHUNK_SIZE, f)) > 0) {
        err = esp_ota_write(ota_handle, buf, rd);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write at offset %ld: %s", total, esp_err_to_name(err));
            free(buf);
            esp_ota_abort(ota_handle);
            fclose(f);
            return err;
        }
        total += (long)rd;
    }

    free(buf);
    fclose(f);

    if (total != file_size) {
        ESP_LOGE(TAG, "Read only %ld of %ld bytes", total, file_size);
        esp_ota_abort(ota_handle);
        return ESP_FAIL;
    }

    /* Validate image and finalise */
    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end: %s", esp_err_to_name(err));
        return err;
    }

    /* Mark new partition as next boot target */
    err = esp_ota_set_boot_partition(part);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Flash complete (%ld bytes) — delete %s then call esp_restart()", total, path);

    if (result) {
        *result = FW_UPDATE_SD_DONE;
    }
    return ESP_OK;
}
