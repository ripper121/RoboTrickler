#include "sd_card.h"

#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

static const char *TAG = "sd_card";

#define SD_CARD_SPI_HOST SPI2_HOST
#define SD_CARD_SPI_DMA_CH SPI_DMA_CH1

esp_err_t sd_card_mount(sdmmc_card_t **out_card)
{
    if (out_card == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files              = 5,
        .allocation_unit_size   = 16 * 1024,
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_CARD_SPI_HOST;
    host.max_freq_khz = PINCFG_SD_SPI_FREQ_KHZ;

    spi_bus_config_t bus_config = {
        .mosi_io_num = PINCFG_SD_MOSI_GPIO,
        .miso_io_num = PINCFG_SD_MISO_GPIO,
        .sclk_io_num = PINCFG_SD_SCK_GPIO,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = 4000,
    };

    esp_err_t ret = spi_bus_initialize(host.slot, &bus_config, SD_CARD_SPI_DMA_CH);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize SD SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }

    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.gpio_cs = PINCFG_SD_CS_GPIO;
    slot.host_id = host.slot;

    ESP_LOGI(TAG, "Mounting SD card at %s", SD_CARD_MOUNT_POINT);

    ret = esp_vfs_fat_sdspi_mount(SD_CARD_MOUNT_POINT, &host, &slot, &mount_cfg, out_card);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem");
        } else {
            ESP_LOGE(TAG, "Failed to initialise card: %s", esp_err_to_name(ret));
        }
        if (spi_bus_free(host.slot) != ESP_OK) {
            ESP_LOGW(TAG, "Failed to free SD SPI bus after mount error");
        }
        return ret;
    }

    ESP_LOGI(TAG, "SD card mounted");

    return ESP_OK;
}

esp_err_t sd_card_unmount(sdmmc_card_t *card)
{
    esp_err_t ret = esp_vfs_fat_sdcard_unmount(SD_CARD_MOUNT_POINT, card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SD card unmounted");
    esp_err_t bus_ret = spi_bus_free(SD_CARD_SPI_HOST);
    if (bus_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to free SD SPI bus: %s", esp_err_to_name(bus_ret));
    }
    return ESP_OK;
}

