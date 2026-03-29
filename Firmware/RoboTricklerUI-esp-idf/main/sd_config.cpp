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

// ArduinoJson
#include <ArduinoJson.h>

static const char *TAG = "sd_config";

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

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);

    if (err) {
        ESP_LOGE(TAG, "deserializeJson failed on %s: %s", rel_path, err.c_str());
        return false;
    }

    if (std::string(rel_path).find("pid") != std::string::npos) {
        cfg.pidThreshold        = doc["threshold"]      | 0.10f;
        cfg.pidStepMin          = doc["stepMin"]        | 5;
        cfg.pidStepMax          = doc["stepMax"]        | 36000;
        cfg.pidSpeed            = doc["speed"]          | 200;
        cfg.pidConMeasurements  = doc["conMeasurements"]| 2;
        cfg.pidAggMeasurements  = doc["aggMeasurements"]| 25;
        cfg.pidTolerance        = doc["tolerance"]      | 0.0f;
        cfg.pidAlarmThreshold   = doc["alarmThreshold"] | 1.0f;
        cfg.pidConsNum          = doc["consNum"]        | 1;
        cfg.pidConsKp           = doc["consKp"]         | 1.0f;
        cfg.pidConsKi           = doc["consKi"]         | 0.05f;
        cfg.pidConsKd           = doc["consKd"]         | 0.25f;
        cfg.pidAggNum           = doc["aggNum"]         | 1;
        cfg.pidAggKp            = doc["aggKp"]          | 4.0f;
        cfg.pidAggKi            = doc["aggKi"]          | 0.2f;
        cfg.pidAggKd            = doc["aggKd"]          | 1.0f;
        cfg.pidOscillate        = doc["oscillate"]      | false;
        cfg.pidReverse          = doc["reverse"]        | false;
        cfg.pidAcceleration     = doc["acceleration"]   | false;
        PID_AKTIVE = true;
    } else {
        for (JsonPair item : doc.as<JsonObject>()) {
            int key = (int)(strtol(item.key().c_str(), nullptr, 10) - 1);
            if (key == 0) {
                cfg.profile_stepsPerUnit   = item.value()["stepsPerUnit"]   | 0.0;
                cfg.profile_tolerance      = item.value()["tolerance"]      | 0.0f;
                cfg.profile_alarmThreshold = item.value()["alarmThreshold"] | 1.0f;
            }
            cfg.profile_num[key]         = item.value()["number"]       | 1;
            cfg.profile_weight[key]      = item.value()["weight"];
            cfg.profile_steps[key]       = item.value()["steps"];
            cfg.profile_speed[key]       = item.value()["speed"];
            cfg.profile_measurements[key]= item.value()["measurements"];
            cfg.profile_oscillate[key]   = item.value()["oscillate"]    | false;
            cfg.profile_reverse[key]     = item.value()["reverse"]      | false;
            cfg.profile_acceleration[key]= item.value()["acceleration"] | false;
            cfg.profile_count = key + 1;
        }
        PID_AKTIVE = false;
    }

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

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);

    if (err) {
        ESP_LOGE(TAG, "Config deserializeJson failed: %s", err.c_str());
        return false;
    }

    strlcpy(cfg.wifi_ssid,       doc["wifi"]["ssid"]       | "", sizeof(cfg.wifi_ssid));
    strlcpy(cfg.wifi_psk,        doc["wifi"]["psk"]        | "", sizeof(cfg.wifi_psk));
    strlcpy(cfg.IPStatic,        doc["wifi"]["IPStatic"]   | "", sizeof(cfg.IPStatic));
    strlcpy(cfg.IPGateway,       doc["wifi"]["IPGateway"]  | "", sizeof(cfg.IPGateway));
    strlcpy(cfg.IPSubnet,        doc["wifi"]["IPSubnet"]   | "", sizeof(cfg.IPSubnet));
    strlcpy(cfg.IPDns,           doc["wifi"]["IPDNS"]      | "", sizeof(cfg.IPDns));
    strlcpy(cfg.scale_protocol,  doc["scale"]["protocol"]  | "GG", sizeof(cfg.scale_protocol));
    strlcpy(cfg.scale_customCode,doc["scale"]["customCode"]| "", sizeof(cfg.scale_customCode));
    cfg.scale_baud        = doc["scale"]["baud"]            | 9600;
    strlcpy(cfg.profile,         doc["profile"]            | "calibrate", sizeof(cfg.profile));
    cfg.log_measurements  = doc["log_Measurements"]         | 20;
    cfg.targetWeight      = doc["weight"]                   | 1.0f;
    cfg.microsteps        = doc["microsteps"]               | 1;
    strlcpy(cfg.beeper,          doc["beeper"]             | "done", sizeof(cfg.beeper));
    cfg.debugLog          = doc["debug_log"]                | false;
    cfg.fwCheck           = doc["fw_check"]                 | true;

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

    JsonDocument doc;
    doc["wifi"]["ssid"]      = cfg.wifi_ssid;
    doc["wifi"]["psk"]       = cfg.wifi_psk;
    doc["wifi"]["IPStatic"]  = cfg.IPStatic;
    doc["wifi"]["IPGateway"] = cfg.IPGateway;
    doc["wifi"]["IPSubnet"]  = cfg.IPSubnet;
    doc["wifi"]["IPDNS"]     = cfg.IPDns;
    doc["scale"]["protocol"] = cfg.scale_protocol;
    doc["scale"]["customCode"]= cfg.scale_customCode;
    doc["scale"]["baud"]     = cfg.scale_baud;
    doc["profile"]           = cfg.profile;
    doc["weight"]            = serialized(str_float(cfg.targetWeight, 3).c_str());
    doc["log_Measurements"]  = cfg.log_measurements;
    doc["microsteps"]        = cfg.microsteps;
    doc["beeper"]            = cfg.beeper;
    doc["debug_log"]         = cfg.debugLog;
    doc["fw_check"]          = cfg.fwCheck;

    std::string json;
    serializeJsonPretty(doc, json);
    fputs(json.c_str(), f);
    fclose(f);
}
