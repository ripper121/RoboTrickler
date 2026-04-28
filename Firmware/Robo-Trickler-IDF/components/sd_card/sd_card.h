#pragma once

#include "esp_err.h"
#include "pinConfig.h"
#include "sdmmc_cmd.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SD card mount point */
#define SD_CARD_MOUNT_POINT     "/sdcard"

/**
 * @brief Mount the SD card via SDSPI and register a FAT filesystem.
 *
 * Initialises the SPI host and slot, mounts the FAT filesystem at
 * SD_CARD_MOUNT_POINT, and returns a handle to the card on success.
 *
 * @param[out] out_card  Pointer that receives the sdmmc_card_t handle.
 *                       Must not be NULL.
 * @return
 *   - ESP_OK            on success
 *   - ESP_ERR_INVALID_ARG  if out_card is NULL
 *   - other esp_err_t   on SDMMC or VFS error
 */
esp_err_t sd_card_mount(sdmmc_card_t **out_card);

/**
 * @brief Unmount the SD card and release the SDMMC peripheral.
 *
 * @param card  Card handle returned by sd_card_mount().
 * @return
 *   - ESP_OK on success
 *   - other esp_err_t on error
 */
esp_err_t sd_card_unmount(sdmmc_card_t *card);

#ifdef __cplusplus
}
#endif
