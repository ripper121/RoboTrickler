#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "cJSON.h"
#include "esp_log.h"

#include "config.h"
#include "storage_utils.h"

static const char *TAG = "config";

/* Copy a cJSON string item into a fixed-size buffer. Silently truncates. */
static void copy_str(char *dst, size_t dst_size, const cJSON *item)
{
    if (cJSON_IsString(item) && item->valuestring) {
        snprintf(dst, dst_size, "%s", item->valuestring);
    } else {
        dst[0] = '\0';
    }
}

static config_buzzer_mode_t parse_buzzer_mode(const cJSON *item)
{
    if (!cJSON_IsString(item) || item->valuestring == NULL) {
        return CONFIG_BUZZER_MODE_OFF;
    }

    if (strcasecmp(item->valuestring, "done") == 0) {
        return CONFIG_BUZZER_MODE_DONE;
    }
    if (strcasecmp(item->valuestring, "button") == 0) {
        return CONFIG_BUZZER_MODE_BUTTON;
    }
    if (strcasecmp(item->valuestring, "both") == 0) {
        return CONFIG_BUZZER_MODE_BOTH;
    }
    if (strcasecmp(item->valuestring, "off") != 0 && item->valuestring[0] != '\0') {
        ESP_LOGW(TAG, "Unknown buzzerMode '%s', disabling buzzer events", item->valuestring);
    }

    return CONFIG_BUZZER_MODE_OFF;
}

static const char *buzzer_mode_to_string(config_buzzer_mode_t mode)
{
    switch (mode) {
    case CONFIG_BUZZER_MODE_DONE:
        return "done";
    case CONFIG_BUZZER_MODE_BUTTON:
        return "button";
    case CONFIG_BUZZER_MODE_BOTH:
        return "both";
    case CONFIG_BUZZER_MODE_OFF:
    default:
        return "off";
    }
}

esp_err_t app_config_load(app_config_t *cfg)
{
    cJSON *root = NULL;
    esp_err_t ret;

    if (!cfg) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(cfg, 0, sizeof(*cfg));

    ret = storage_utils_parse_json_file(CONFIG_PATH, TAG, &root);
    if (ret != ESP_OK) {
        return ret;
    }

    /* scale */
    const cJSON *scale = cJSON_GetObjectItemCaseSensitive(root, "scale");
    if (scale) {
        const cJSON *v;
        if ((v = cJSON_GetObjectItemCaseSensitive(scale, "baud")) && cJSON_IsNumber(v))
            cfg->scale.baud = (int)v->valuedouble;
        copy_str(cfg->scale.separator,  sizeof(cfg->scale.separator),  cJSON_GetObjectItemCaseSensitive(scale, "separator"));
        copy_str(cfg->scale.terminator, sizeof(cfg->scale.terminator), cJSON_GetObjectItemCaseSensitive(scale, "terminator"));
        copy_str(cfg->scale.request,    sizeof(cfg->scale.request),    cJSON_GetObjectItemCaseSensitive(scale, "request"));
        if ((v = cJSON_GetObjectItemCaseSensitive(scale, "request_delay_ms")) && cJSON_IsNumber(v))
            cfg->scale.request_delay_ms = (int)v->valuedouble;
        copy_str(cfg->scale.tare, sizeof(cfg->scale.tare), cJSON_GetObjectItemCaseSensitive(scale, "tare"));
        cfg->scale.tare_auto = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(scale, "tare_auto"));
        v = cJSON_GetObjectItemCaseSensitive(scale, "tare_delay_ms");
        if (cJSON_IsNumber(v))
            cfg->scale.tare_delay_ms = (int)v->valuedouble;
        if ((v = cJSON_GetObjectItemCaseSensitive(scale, "timeout_ms")) && cJSON_IsNumber(v))
            cfg->scale.timeout_ms = (int)v->valuedouble;
    }

    /* wifi */
    const cJSON *wifi = cJSON_GetObjectItemCaseSensitive(root, "wifi");
    if (wifi) {
        const cJSON *ap = cJSON_GetObjectItemCaseSensitive(wifi, "ap");
        if (ap != NULL) {
            cfg->wifi.ap = cJSON_IsTrue(ap);
        }
        copy_str(cfg->wifi.ssid,      sizeof(cfg->wifi.ssid),      cJSON_GetObjectItemCaseSensitive(wifi, "ssid"));
        copy_str(cfg->wifi.psk,       sizeof(cfg->wifi.psk),       cJSON_GetObjectItemCaseSensitive(wifi, "psk"));
        copy_str(cfg->wifi.static_ip, sizeof(cfg->wifi.static_ip), cJSON_GetObjectItemCaseSensitive(wifi, "staticIp"));
        copy_str(cfg->wifi.gateway,   sizeof(cfg->wifi.gateway),   cJSON_GetObjectItemCaseSensitive(wifi, "gateway"));
        copy_str(cfg->wifi.subnet,    sizeof(cfg->wifi.subnet),    cJSON_GetObjectItemCaseSensitive(wifi, "subnet"));
        copy_str(cfg->wifi.dns,       sizeof(cfg->wifi.dns),       cJSON_GetObjectItemCaseSensitive(wifi, "dns"));
    }

    /* fw_update */
    const cJSON *fw = cJSON_GetObjectItemCaseSensitive(root, "fw_update");
    if (fw) {
        const cJSON *v;
        if ((v = cJSON_GetObjectItemCaseSensitive(fw, "check")))
            cfg->fw_update.check = cJSON_IsTrue(v);
        copy_str(cfg->fw_update.url, sizeof(cfg->fw_update.url), cJSON_GetObjectItemCaseSensitive(fw, "url"));
    }

    /* top-level fields */
    copy_str(cfg->profile,     sizeof(cfg->profile),     cJSON_GetObjectItemCaseSensitive(root, "profile"));
    copy_str(cfg->language,    sizeof(cfg->language),    cJSON_GetObjectItemCaseSensitive(root, "language"));
    if (cfg->language[0] == '\0') {
        snprintf(cfg->language, sizeof(cfg->language), "en");
    }
    cfg->buzzer_mode = parse_buzzer_mode(cJSON_GetObjectItemCaseSensitive(root, "buzzerMode"));
    copy_str(cfg->key,         sizeof(cfg->key),         cJSON_GetObjectItemCaseSensitive(root, "key"));

    const cJSON *dbg = cJSON_GetObjectItemCaseSensitive(root, "debug");
    if (dbg) cfg->debug = cJSON_IsTrue(dbg);

    cJSON_Delete(root);

    ESP_LOGI(TAG, "Config loaded (profile=%s, debug=%d)", cfg->profile, cfg->debug);
    return ESP_OK;
}

esp_err_t app_config_save(const app_config_t *cfg)
{
    cJSON *root = NULL;
    cJSON *scale = NULL;
    cJSON *wifi = NULL;
    cJSON *fw = NULL;
    char *json = NULL;
    esp_err_t ret = ESP_OK;

    if (cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    root = cJSON_CreateObject();
    if (root == NULL) {
        return ESP_ERR_NO_MEM;
    }

    scale = cJSON_AddObjectToObject(root, "scale");
    wifi = cJSON_AddObjectToObject(root, "wifi");
    fw = cJSON_AddObjectToObject(root, "fw_update");
    if (scale == NULL || wifi == NULL || fw == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    if (!cJSON_AddNumberToObject(scale, "baud", cfg->scale.baud) ||
        !cJSON_AddStringToObject(scale, "separator", cfg->scale.separator) ||
        !cJSON_AddStringToObject(scale, "terminator", cfg->scale.terminator) ||
        !cJSON_AddStringToObject(scale, "request", cfg->scale.request) ||
        !cJSON_AddNumberToObject(scale, "request_delay_ms", cfg->scale.request_delay_ms) ||
        !cJSON_AddStringToObject(scale, "tare", cfg->scale.tare) ||
        !cJSON_AddBoolToObject(scale, "tare_auto", cfg->scale.tare_auto) ||
        !cJSON_AddNumberToObject(scale, "tare_delay_ms", cfg->scale.tare_delay_ms) ||
        !cJSON_AddNumberToObject(scale, "timeout_ms", cfg->scale.timeout_ms) ||
        !cJSON_AddBoolToObject(wifi, "ap", cfg->wifi.ap) ||
        !cJSON_AddStringToObject(wifi, "ssid", cfg->wifi.ssid) ||
        !cJSON_AddStringToObject(wifi, "psk", cfg->wifi.psk) ||
        !cJSON_AddStringToObject(wifi, "staticIp", cfg->wifi.static_ip) ||
        !cJSON_AddStringToObject(wifi, "gateway", cfg->wifi.gateway) ||
        !cJSON_AddStringToObject(wifi, "subnet", cfg->wifi.subnet) ||
        !cJSON_AddStringToObject(wifi, "dns", cfg->wifi.dns) ||
        !cJSON_AddBoolToObject(fw, "check", cfg->fw_update.check) ||
        !cJSON_AddStringToObject(fw, "url", cfg->fw_update.url) ||
        !cJSON_AddStringToObject(root, "profile", cfg->profile) ||
        !cJSON_AddStringToObject(root, "language", cfg->language[0] != '\0' ? cfg->language : "en") ||
        !cJSON_AddStringToObject(root, "buzzerMode", buzzer_mode_to_string(cfg->buzzer_mode)) ||
        !cJSON_AddStringToObject(root, "key", cfg->key) ||
        !cJSON_AddBoolToObject(root, "debug", cfg->debug)) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    json = cJSON_Print(root);
    if (json == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    ret = storage_utils_write_text_file(CONFIG_PATH, json, TAG);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Config saved to %s", CONFIG_PATH);
    }

cleanup:
    if (json != NULL) {
        cJSON_free(json);
    }
    cJSON_Delete(root);
    return ret;
}
