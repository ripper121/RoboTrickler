#include "config.h"
#include "arduino_compat.h"
#include "pindef.h"

#include <string>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <dirent.h>
#include <errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "esp_log.h"

#include "cJSON.h"

static const char *TAG = "sd_config";

// ============================================================
// cJSON helpers — typed access with default values
// ============================================================
static float cj_float(const cJSON *obj, const char *key, float def) {
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsNumber(it) ? (float)it->valuedouble : def;
}

static int cj_int(const cJSON *obj, const char *key, int def) {
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsNumber(it) ? it->valueint : def;
}

static double cj_double(const cJSON *obj, const char *key, double def) {
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsNumber(it) ? it->valuedouble : def;
}

static bool cj_bool(const cJSON *obj, const char *key, bool def) {
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsTrue(it))  return true;
    if (cJSON_IsFalse(it)) return false;
    return def;
}

static void cj_str(const cJSON *obj, const char *key,
                   char *dst, size_t dst_sz, const char *def) {
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    strlcpy(dst,
            (cJSON_IsString(it) && it->valuestring) ? it->valuestring : def,
            dst_sz);
}

// ============================================================
// SD path helper
// ============================================================
static std::string sd_path(const char *rel) {
    if (rel && rel[0] == '/') return std::string(SD_MOUNT_POINT) + rel;
    return std::string(SD_MOUNT_POINT) + "/" + (rel ? rel : "");
}

static bool read_file_to_string(const std::string &path, std::string &out) {
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) return false;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }

    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return false;
    }

    rewind(f);
    out.resize((size_t)size);
    size_t read = size > 0 ? fread(out.data(), 1, out.size(), f) : 0;
    fclose(f);

    if (read != out.size()) return false;
    return true;
}

// ============================================================
// sd_mount — mount the SD card via SPI
// ============================================================
bool sd_mount() {
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;   // HSPI

    spi_bus_config_t bus = {};
    bus.mosi_io_num   = SD_MOSI;
    bus.miso_io_num   = SD_MISO;
    bus.sclk_io_num   = SD_SCK;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = 4096;

    esp_err_t r = spi_bus_initialize((spi_host_device_t)host.slot, &bus, SPI_DMA_CH_AUTO);
    if (r != ESP_OK && r != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(r));
        return false;
    }

    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.gpio_cs   = SD_SS;
    slot.host_id   = (spi_host_device_t)host.slot;

    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {};
    mount_cfg.format_if_mount_failed = false;
    mount_cfg.max_files              = 10;
    mount_cfg.allocation_unit_size  = 16 * 1024;

    sdmmc_card_t *card = NULL;
    r = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot, &mount_cfg, &card);
    if (r != ESP_OK) {
        ESP_LOGE(TAG, "SD mount failed: %s", esp_err_to_name(r));
        return false;
    }

    ESP_LOGI(TAG, "SD mounted. Name:%s Speed:%u MHz",
             card->cid.name,
             (unsigned)(card->max_freq_khz / 1000));
    return true;
}

// ============================================================
// printFile — print file to debug log
// ============================================================
void printFile(const char *rel_path) {
    std::string full = sd_path(rel_path);
    FILE *f = fopen(full.c_str(), "r");
    if (!f) { ESP_LOGD(TAG, "printFile: cannot open %s", full.c_str()); return; }
    char line[128];
    while (fgets(line, sizeof(line), f)) ESP_LOGD(TAG, "%s", line);
    fclose(f);
}

// ============================================================
// readProfile — load a profile JSON file into config
// ============================================================
bool readProfile(const char *rel_path, Config &cfg) {
    std::string full = sd_path(rel_path);
    ESP_LOGD(TAG, "readProfile: %s", full.c_str());

    // Delay matches original Arduino version
    vTaskDelay(pdMS_TO_TICKS(500));

    if (access(full.c_str(), F_OK) != 0) {
        ESP_LOGW(TAG, "Profile not found: %s", full.c_str());
        return false;
    }

    printFile(rel_path);

    std::string json;
    if (!read_file_to_string(full, json)) return false;

    cJSON *doc = cJSON_Parse(json.c_str());
    if (!doc) {
        ESP_LOGE(TAG, "cJSON_Parse failed on %s", rel_path);
        return false;
    }

    if (std::string(rel_path).find("pid") != std::string::npos) {
        cfg.pidThreshold        = cj_float (doc, "threshold",       0.10f);
        cfg.pidStepMin          = cj_int   (doc, "stepMin",         5);
        cfg.pidStepMax          = cj_int   (doc, "stepMax",         36000);
        cfg.pidSpeed            = cj_int   (doc, "speed",           200);
        cfg.pidConMeasurements  = cj_int   (doc, "conMeasurements", 2);
        cfg.pidAggMeasurements  = cj_int   (doc, "aggMeasurements", 25);
        cfg.pidTolerance        = cj_float (doc, "tolerance",       0.0f);
        cfg.pidAlarmThreshold   = cj_float (doc, "alarmThreshold",  1.0f);
        cfg.pidConsNum          = (uint8_t)cj_int(doc, "consNum",   1);
        cfg.pidConsKp           = cj_float (doc, "consKp",          1.0f);
        cfg.pidConsKi           = cj_float (doc, "consKi",          0.05f);
        cfg.pidConsKd           = cj_float (doc, "consKd",          0.25f);
        cfg.pidAggNum           = (uint8_t)cj_int(doc, "aggNum",    1);
        cfg.pidAggKp            = cj_float (doc, "aggKp",           4.0f);
        cfg.pidAggKi            = cj_float (doc, "aggKi",           0.2f);
        cfg.pidAggKd            = cj_float (doc, "aggKd",           1.0f);
        cfg.pidOscillate        = cj_bool  (doc, "oscillate",       false);
        cfg.pidReverse          = cj_bool  (doc, "reverse",         false);
        cfg.pidAcceleration     = cj_bool  (doc, "acceleration",    false);
        PID_AKTIVE = true;
    } else {
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, doc) {
            int key = (int)(strtol(item->string, nullptr, 10) - 1);
            if (key == 0) {
                cfg.profile_stepsPerUnit   = cj_double(item, "stepsPerUnit",   0.0);
                cfg.profile_tolerance      = cj_float (item, "tolerance",      0.0f);
                cfg.profile_alarmThreshold = cj_float (item, "alarmThreshold", 1.0f);
            }
            cfg.profile_num[key]          = cj_int  (item, "number",       1);
            cfg.profile_weight[key]       = cj_float(item, "weight",       0.0f);
            cfg.profile_steps[key]        = cj_int  (item, "steps",        0);
            cfg.profile_speed[key]        = cj_int  (item, "speed",        0);
            cfg.profile_measurements[key] = cj_int  (item, "measurements", 0);
            cfg.profile_oscillate[key]    = cj_bool (item, "oscillate",    false);
            cfg.profile_reverse[key]      = cj_bool (item, "reverse",      false);
            cfg.profile_acceleration[key] = cj_bool (item, "acceleration", false);
            cfg.profile_count = key + 1;
        }
        PID_AKTIVE = false;
    }

    cJSON_Delete(doc);
    return true;
}

// ============================================================
// loadConfiguration — read /config.txt into Config struct
// ============================================================
bool loadConfiguration(const char *rel_path, Config &cfg) {
    printFile(rel_path);

    std::string full = sd_path(rel_path);
    std::string json;
    if (!read_file_to_string(full, json)) {
        ESP_LOGE(TAG, "Cannot open %s", full.c_str());
        return false;
    }

    cJSON *doc = cJSON_Parse(json.c_str());
    if (!doc) {
        ESP_LOGE(TAG, "Config cJSON_Parse failed");
        return false;
    }

    const cJSON *wifi  = cJSON_GetObjectItemCaseSensitive(doc, "wifi");
    const cJSON *scale = cJSON_GetObjectItemCaseSensitive(doc, "scale");

    cj_str(wifi,  "ssid",       cfg.wifi_ssid,        sizeof(cfg.wifi_ssid),        "");
    cj_str(wifi,  "psk",        cfg.wifi_psk,         sizeof(cfg.wifi_psk),         "");
    cj_str(wifi,  "IPStatic",   cfg.IPStatic,         sizeof(cfg.IPStatic),         "");
    cj_str(wifi,  "IPGateway",  cfg.IPGateway,        sizeof(cfg.IPGateway),        "");
    cj_str(wifi,  "IPSubnet",   cfg.IPSubnet,         sizeof(cfg.IPSubnet),         "");
    cj_str(wifi,  "IPDNS",      cfg.IPDns,            sizeof(cfg.IPDns),            "");
    cj_str(scale, "protocol",   cfg.scale_protocol,   sizeof(cfg.scale_protocol),   "GG");
    cj_str(scale, "customCode", cfg.scale_customCode, sizeof(cfg.scale_customCode), "");
    cfg.scale_baud       = cj_int  (scale, "baud",             9600);
    cj_str(doc,   "profile",    cfg.profile,          sizeof(cfg.profile),          "calibrate");
    cfg.log_measurements = cj_int  (doc,   "log_Measurements", 20);
    cfg.targetWeight     = cj_float(doc,   "weight",           1.0f);
    cfg.microsteps       = cj_int  (doc,   "microsteps",       1);
    cj_str(doc,   "beeper",     cfg.beeper,           sizeof(cfg.beeper),           "done");
    cfg.debugLog         = cj_bool (doc,   "debug_log",        false);
    cfg.fwCheck          = cj_bool (doc,   "fw_check",         true);

    cJSON_Delete(doc);
    return true;
}

// ============================================================
// getProfileList — scan SD root for .txt profile files
// ============================================================

extern void updateDisplayLog(const std::string &msg, bool noLog);

void getProfileList() {
    DIR *dir = opendir(SD_MOUNT_POINT);
    if (!dir) { ESP_LOGE(TAG, "Failed to open SD root"); return; }

    updateDisplayLog("Search Profiles...", false);
    uint8_t cnt = 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && cnt < 32) {
        if (entry->d_type == DT_DIR) continue;

        std::string fname(entry->d_name);
        if (fname.find(".txt")    != std::string::npos &&
            fname.find("config.txt") == std::string::npos &&
            fname.find(".cor")    == std::string::npos) {
            // Strip .txt extension
            fname = fname.substr(0, fname.size() - 4);
            profileListBuff[cnt++] = fname;
        }
    }
    closedir(dir);
    profileListCount = cnt;

    for (int i = 0; i < cnt; i++) ESP_LOGD(TAG, "Profile: %s", profileListBuff[i].c_str());
}

// ============================================================
// saveConfiguration — write Config struct to /config.txt
// ============================================================
void saveConfiguration(const char *rel_path, const Config &cfg) {
    std::string full = sd_path(rel_path);
    // Remove existing file
    unlink(full.c_str());

    FILE *f = fopen(full.c_str(), "w");
    if (!f) { ESP_LOGE(TAG, "Failed to create %s", full.c_str()); return; }

    cJSON *doc   = cJSON_CreateObject();
    cJSON *wifi  = cJSON_CreateObject();
    cJSON *scale = cJSON_CreateObject();

    cJSON_AddStringToObject(wifi,  "ssid",        cfg.wifi_ssid);
    cJSON_AddStringToObject(wifi,  "psk",         cfg.wifi_psk);
    cJSON_AddStringToObject(wifi,  "IPStatic",    cfg.IPStatic);
    cJSON_AddStringToObject(wifi,  "IPGateway",   cfg.IPGateway);
    cJSON_AddStringToObject(wifi,  "IPSubnet",    cfg.IPSubnet);
    cJSON_AddStringToObject(wifi,  "IPDNS",       cfg.IPDns);
    cJSON_AddItemToObject(doc, "wifi", wifi);

    cJSON_AddStringToObject(scale, "protocol",    cfg.scale_protocol);
    cJSON_AddStringToObject(scale, "customCode",  cfg.scale_customCode);
    cJSON_AddNumberToObject(scale, "baud",        cfg.scale_baud);
    cJSON_AddItemToObject(doc, "scale", scale);

    cJSON_AddStringToObject(doc, "profile",         cfg.profile);
    cJSON_AddItemToObject   (doc, "weight",
        cJSON_CreateRaw(str_float(cfg.targetWeight, 3).c_str()));
    cJSON_AddNumberToObject(doc, "log_Measurements", cfg.log_measurements);
    cJSON_AddNumberToObject(doc, "microsteps",       cfg.microsteps);
    cJSON_AddStringToObject(doc, "beeper",           cfg.beeper);
    cJSON_AddBoolToObject  (doc, "debug_log",        cfg.debugLog);
    cJSON_AddBoolToObject  (doc, "fw_check",         cfg.fwCheck);

    char *json = cJSON_Print(doc);
    cJSON_Delete(doc);

    if (json) {
        fputs(json, f);
        free(json);
    }
    fclose(f);
}
