#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Outcome of a firmware download check. */
typedef enum {
    FW_DOWNLOAD_NOT_NEEDED = 0, /**< Remote build_epoch is not newer than running firmware */
    FW_DOWNLOAD_DONE,           /**< Newer firmware saved to /sdcard/firmware.bin */
} fw_download_result_t;

/** Metadata returned by a firmware update probe. */
typedef struct {
    bool update_available;       /**< True when remote_build_epoch is newer than local_build_epoch */
    uint32_t local_build_epoch;  /**< Build epoch compiled into the running firmware */
    uint32_t remote_build_epoch; /**< Build epoch from firmware.json */
    char file[64];               /**< Firmware file from firmware.json, or firmware.bin */
} fw_download_info_t;

/**
 * @brief Check whether a newer firmware is available without downloading it.
 *
 * Fetches {cfg->fw_update.url}/firmware.json and compares its build_epoch
 * against the running firmware.
 *
 * Prerequisites: WiFi connected.
 *
 * @param cfg       Loaded application configuration.  Must not be NULL and
 *                  cfg->fw_update.url must not be empty.
 * @param out_info  Receives remote/local firmware metadata.  Must not be NULL.
 * @return
 *   - ESP_OK              on success
 *   - ESP_ERR_INVALID_ARG if cfg/out_info is NULL or url is empty
 *   - ESP_ERR_NO_MEM      on allocation failure
 *   - ESP_FAIL            on HTTP or JSON-parse error
 */
esp_err_t fw_download_probe(const app_config_t *cfg, fw_download_info_t *out_info);

/**
 * @brief Download the newest firmware if firmware.json advertises one.
 *
 * Prerequisites: WiFi connected, SD card mounted.
 *
 * @param cfg     Loaded application configuration.  Must not be NULL and
 *                cfg->fw_update.url must not be empty.
 * @param result  Receives the outcome.  May be NULL.
 */
esp_err_t fw_download_latest(const app_config_t *cfg, fw_download_result_t *result);

#ifdef __cplusplus
}
#endif
