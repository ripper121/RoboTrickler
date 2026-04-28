#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Default path of the firmware binary on the SD card. */
#define FW_UPDATE_SD_DEFAULT_PATH  "/sdcard/firmware.bin"

/** Outcome of fw_update_sd_apply(). */
typedef enum {
    FW_UPDATE_SD_NOT_FOUND = 0, /**< No .bin file at the given path — nothing to do */
    FW_UPDATE_SD_DONE,          /**< Firmware flashed; call esp_restart() to activate */
} fw_update_sd_result_t;

/**
 * @brief Flash a firmware binary from the SD card into the next OTA partition.
 *
 * Opens bin_path (defaults to FW_UPDATE_SD_DEFAULT_PATH when NULL), streams it
 * into the next available OTA partition, validates the image, and marks that
 * partition as the next boot target.
 *
 * The caller is responsible for calling esp_restart() when ready.
 * To avoid re-flashing on the next boot, delete the .bin file after a
 * successful call (result == FW_UPDATE_SD_DONE).
 *
 * Prerequisites: SD card mounted, OTA partition table active.
 *
 * @param bin_path  Path to the .bin file on the SD card.
 *                  Pass NULL to use FW_UPDATE_SD_DEFAULT_PATH.
 * @param result    Receives the outcome.  May be NULL.
 * @return
 *   - ESP_OK              on success (inspect *result for flash status)
 *   - ESP_ERR_NO_MEM      on allocation failure
 *   - ESP_FAIL            if the file is empty, unreadable, or image is invalid
 *   - ESP_ERR_OTA_*       on OTA partition or validation errors
 */
esp_err_t fw_update_sd_apply(const char *bin_path, fw_update_sd_result_t *result);

#ifdef __cplusplus
}
#endif
