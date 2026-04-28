#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "display.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "fw_download.h"
#include "fw_update_sd.h"
#include "sd_card.h"
#include "ui.h"
#include "webserver.h"
#include "wifi.h"

static const char *TAG = "main";

#define MAIN_UI_POLL_PERIOD_MS  20U
#define MAIN_HEAP_LOG_PERIOD_MS 10000U

static void log_heap_status(void)
{
    const uint32_t total_heap = (uint32_t)heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    const uint32_t current_free_heap = esp_get_free_heap_size();
    const uint32_t minimum_free_heap = esp_get_minimum_free_heap_size();
    const uint32_t current_free_pct = total_heap > 0U ? ((current_free_heap * 100U) / total_heap) : 0U;
    const uint32_t minimum_free_pct = total_heap > 0U ? ((minimum_free_heap * 100U) / total_heap) : 0U;

    ESP_LOGI(TAG,
             "Heap free current=%" PRIu32 "/%" PRIu32 " (%" PRIu32 "%%), low=%" PRIu32 "/%" PRIu32 " (%" PRIu32 "%%)",
             current_free_heap,
             total_heap,
             current_free_pct,
             minimum_free_heap,
             total_heap,
             minimum_free_pct);
}

static void app_config_set_defaults(app_config_t *cfg)
{
    if (cfg == NULL) {
        return;
    }

    memset(cfg, 0, sizeof(*cfg));
    cfg->scale.baud = 9600;
    snprintf(cfg->scale.separator, sizeof(cfg->scale.separator), ".");
    snprintf(cfg->scale.terminator, sizeof(cfg->scale.terminator), "0D");
    snprintf(cfg->scale.request, sizeof(cfg->scale.request), "1B 70 0D 0A");
    snprintf(cfg->scale.tare, sizeof(cfg->scale.tare), "1B 74 0D 0A");
    cfg->scale.timeout_ms = 1000;
    cfg->scale.request_delay_ms = 0;
    cfg->scale.tare_auto = false;
    cfg->scale.tare_delay_ms = 0;
    cfg->wifi.ap = false;
    cfg->fw_update.check = false;
    snprintf(cfg->profile, sizeof(cfg->profile), "powder");
    snprintf(cfg->language, sizeof(cfg->language), "en");
    cfg->buzzer_mode = CONFIG_BUZZER_MODE_DONE;
}

static void apply_sd_firmware_update(sdmmc_card_t *card)
{
    fw_update_sd_result_t ota_result = FW_UPDATE_SD_NOT_FOUND;
    esp_err_t ret = fw_update_sd_apply(NULL, &ota_result);

    if (ret == ESP_OK && ota_result == FW_UPDATE_SD_DONE) {
        remove(FW_UPDATE_SD_DEFAULT_PATH);
        sd_card_unmount(card);
        ESP_LOGI(TAG, "Rebooting to activate firmware from SD card");
        esp_restart();
    }

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            if (remove(FW_UPDATE_SD_DEFAULT_PATH) == 0) {
                ESP_LOGW(TAG, "Deleted invalid firmware file: %s", FW_UPDATE_SD_DEFAULT_PATH);
            }
        }
        ESP_LOGE(TAG, "SD firmware update failed: %s", esp_err_to_name(ret));
    }
}

void app_main(void)
{
    sdmmc_card_t *card = NULL;
    app_config_t cfg = {0};
    fw_download_result_t fw_result = FW_DOWNLOAD_NOT_NEEDED;
    bool ui_ready = false;
    bool wifi_connected_sta = false;
    TickType_t last_heap_log_tick = xTaskGetTickCount();

    esp_err_t ret = display_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Display init failed: %s", esp_err_to_name(ret));
    }

    ret = sd_card_mount(&card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD card mount failed: %s", esp_err_to_name(ret));
    }

    if (card != NULL) {
        apply_sd_firmware_update(card);
    }

    ret = app_config_load(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Config load failed (%s), using defaults", esp_err_to_name(ret));
        app_config_set_defaults(&cfg);
        if (card != NULL) {
            esp_err_t save_ret = app_config_save(&cfg);
            if (save_ret != ESP_OK) {
                ESP_LOGW(TAG, "Default config save failed: %s", esp_err_to_name(save_ret));
            }
        }
    }

    ret = ui_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UI init failed: %s", esp_err_to_name(ret));
    } else {
        ui_ready = true;
        (void)ui_set_webserver_callbacks(webserver_start, webserver_stop);
    }

    if (cfg.wifi.ssid[0] != '\0') {
        ret = wifi_connect(&cfg);
        if (ui_ready) {
            (void)ui_set_wifi_connected(ret == ESP_OK);
        }

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "WiFi connect failed: %s", esp_err_to_name(ret));
        } else {
            wifi_connected_sta = !cfg.wifi.ap;
            ret = webserver_start();
            if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
                ESP_LOGE(TAG, "Webserver start failed: %s", esp_err_to_name(ret));
            }
        }
    } else {
        ESP_LOGW(TAG, "WiFi SSID not configured, skipping WiFi and webserver startup");
    }

    if (cfg.fw_update.check && wifi_connected_sta) {
        ret = fw_download_latest(&cfg, &fw_result);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Firmware download failed: %s", esp_err_to_name(ret));
        }
        if (fw_result == FW_DOWNLOAD_DONE) {
            ESP_LOGI(TAG, "New firmware downloaded to SD card, rebooting");
            sd_card_unmount(card);
            esp_restart();
        }
    }

    log_heap_status();

    while (true) {
        if (ui_ready) {
            ret = ui_poll();
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "UI poll failed: %s", esp_err_to_name(ret));
            }
        }

        const TickType_t now = xTaskGetTickCount();
        if ((now - last_heap_log_tick) >= pdMS_TO_TICKS(MAIN_HEAP_LOG_PERIOD_MS)) {
            log_heap_status();
            last_heap_log_tick = now;
        }

        vTaskDelay(pdMS_TO_TICKS(MAIN_UI_POLL_PERIOD_MS));
    }
}
