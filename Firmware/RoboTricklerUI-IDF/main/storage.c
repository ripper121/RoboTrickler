#include "storage.h"

#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>
#include "board.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "cJSON.h"

static sdmmc_card_t *s_card;
static char s_last_error[160];

static void set_error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(s_last_error, sizeof(s_last_error), fmt, args);
    va_end(args);
}

const char *rt_storage_last_error(void)
{
    return s_last_error;
}

const char *rt_storage_mount_point(void)
{
    return RT_SD_MOUNT_POINT;
}

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        set_error("open failed: %s", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);
    if (len < 0 || len > 64 * 1024) {
        fclose(f);
        set_error("invalid file size: %s", path);
        return NULL;
    }
    char *data = calloc(1, len + 1);
    if (!data) {
        fclose(f);
        set_error("out of memory reading: %s", path);
        return NULL;
    }
    if (fread(data, 1, len, f) != (size_t)len) {
        free(data);
        fclose(f);
        set_error("short read: %s", path);
        return NULL;
    }
    fclose(f);
    return data;
}

static const cJSON *obj(const cJSON *parent, const char *key)
{
    if (!parent) {
        return NULL;
    }
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(parent, key);
    return cJSON_IsObject(item) ? item : NULL;
}

static const char *json_str(const cJSON *parent, const char *key, const char *fallback)
{
    if (!parent) {
        return fallback;
    }
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(parent, key);
    return cJSON_IsString(item) && item->valuestring ? item->valuestring : fallback;
}

static int json_int(const cJSON *parent, const char *key, int fallback)
{
    if (!parent) {
        return fallback;
    }
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(parent, key);
    return cJSON_IsNumber(item) ? item->valueint : fallback;
}

static double json_double(const cJSON *parent, const char *key, double fallback)
{
    if (!parent) {
        return fallback;
    }
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(parent, key);
    return cJSON_IsNumber(item) ? item->valuedouble : fallback;
}

static bool json_bool(const cJSON *parent, const char *key, bool fallback)
{
    if (!parent) {
        return fallback;
    }
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(parent, key);
    return cJSON_IsBool(item) ? cJSON_IsTrue(item) : fallback;
}

static esp_err_t write_json_file(const char *path, cJSON *doc)
{
    char *printed = cJSON_Print(doc);
    if (!printed) {
        set_error("json print failed: %s", path);
        return ESP_ERR_NO_MEM;
    }
    FILE *f = fopen(path, "wb");
    if (!f) {
        set_error("open for write failed: %s", path);
        free(printed);
        return ESP_FAIL;
    }
    size_t len = strlen(printed);
    bool ok = fwrite(printed, 1, len, f) == len;
    fclose(f);
    free(printed);
    if (!ok) {
        set_error("write failed: %s", path);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t rt_storage_mount(void)
{
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = RT_SD_SPI_FREQ_KHZ;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = RT_SD_SPI_MOSI,
        .miso_io_num = RT_SD_SPI_MISO,
        .sclk_io_num = RT_SD_SPI_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    esp_err_t ret = spi_bus_initialize(host.slot, &bus_cfg, RT_SD_DMA_CH);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        set_error("sd spi bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = RT_SD_SPI_CS;
    slot_config.host_id = host.slot;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 8,
        .allocation_unit_size = 16 * 1024,
    };
    ret = esp_vfs_fat_sdspi_mount(RT_SD_MOUNT_POINT, &host, &slot_config, &mount_config, &s_card);
    if (ret != ESP_OK) {
        set_error("sd mount failed: %s", esp_err_to_name(ret));
        return ret;
    }
    sdmmc_card_print_info(stdout, s_card);
    return ESP_OK;
}

esp_err_t rt_storage_save_config(const char *path, const rt_config_t *config)
{
    char full_path[96];
    snprintf(full_path, sizeof(full_path), "%s%s", RT_SD_MOUNT_POINT, path);
    cJSON *doc = cJSON_CreateObject();
    cJSON *wifi = cJSON_AddObjectToObject(doc, "wifi");
    cJSON_AddStringToObject(wifi, "ssid", config->wifi_ssid);
    cJSON_AddStringToObject(wifi, "psk", config->wifi_psk);
    cJSON_AddStringToObject(wifi, "IPStatic", config->ip_static);
    cJSON_AddStringToObject(wifi, "IPGateway", config->ip_gateway);
    cJSON_AddStringToObject(wifi, "IPSubnet", config->ip_subnet);
    cJSON_AddStringToObject(wifi, "IPDNS", config->ip_dns);
    cJSON *scale = cJSON_AddObjectToObject(doc, "scale");
    cJSON_AddStringToObject(scale, "protocol", config->scale_protocol);
    cJSON_AddStringToObject(scale, "customCode", config->scale_custom_code);
    cJSON_AddNumberToObject(scale, "baud", config->scale_baud);
    cJSON_AddStringToObject(doc, "profile", config->profile);
    cJSON_AddStringToObject(doc, "beeper", config->beeper);
    cJSON_AddStringToObject(doc, "language", config->language);
    cJSON_AddBoolToObject(doc, "fw_check", config->fw_check);
    esp_err_t ret = write_json_file(full_path, doc);
    cJSON_Delete(doc);
    return ret;
}

esp_err_t rt_storage_load_config(const char *path, rt_config_t *config)
{
    char full_path[96];
    snprintf(full_path, sizeof(full_path), "%s%s", RT_SD_MOUNT_POINT, path);
    char *data = read_file(full_path);
    if (!data) {
        return ESP_FAIL;
    }
    cJSON *doc = cJSON_Parse(data);
    free(data);
    if (!cJSON_IsObject(doc)) {
        cJSON_Delete(doc);
        set_error("config json parse failed: %s", full_path);
        return ESP_FAIL;
    }

    const cJSON *wifi = obj(doc, "wifi");
    const cJSON *scale = obj(doc, "scale");
    strlcpy(config->wifi_ssid, json_str(wifi, "ssid", ""), sizeof(config->wifi_ssid));
    strlcpy(config->wifi_psk, json_str(wifi, "psk", ""), sizeof(config->wifi_psk));
    strlcpy(config->ip_static, json_str(wifi, "IPStatic", ""), sizeof(config->ip_static));
    strlcpy(config->ip_gateway, json_str(wifi, "IPGateway", ""), sizeof(config->ip_gateway));
    strlcpy(config->ip_subnet, json_str(wifi, "IPSubnet", ""), sizeof(config->ip_subnet));
    strlcpy(config->ip_dns, json_str(wifi, "IPDNS", ""), sizeof(config->ip_dns));
    strlcpy(config->scale_protocol, json_str(scale, "protocol", "GG"), sizeof(config->scale_protocol));
    strlcpy(config->scale_custom_code, json_str(scale, "customCode", ""), sizeof(config->scale_custom_code));
    config->scale_baud = json_int(scale, "baud", 9600);
    strlcpy(config->profile, json_str(doc, "profile", "calibrate"), sizeof(config->profile));
    strlcpy(config->beeper, json_str(doc, "beeper", "done"), sizeof(config->beeper));
    strlcpy(config->language, json_str(doc, "language", "en"), sizeof(config->language));
    config->fw_check = json_bool(doc, "fw_check", true);
    cJSON_Delete(doc);
    return ESP_OK;
}

static uint8_t actuator_number(const char *name)
{
    if (!name || strcmp(name, "stepper1") == 0) {
        return 1;
    }
    return strcmp(name, "stepper2") == 0 ? 2 : 0;
}

static bool load_profile_entry(const cJSON *entry, rt_config_t *config)
{
    if (!cJSON_IsObject(entry) || config->profile_count >= RT_MAX_PROFILE_STEPS) {
        return false;
    }
    uint8_t num = cJSON_HasObjectItem(entry, "number") ? (uint8_t)json_int(entry, "number", 1) : actuator_number(json_str(entry, "actuator", "stepper1"));
    long steps = json_int(entry, "steps", 0);
    int speed = json_int(entry, "speed", 200);
    int measurements = json_int(entry, "measurements", 5);
    float diff_weight = (float)json_double(entry, "diffWeight", json_double(entry, "weight", 0.0));
    if (num < 1 || num > 2 || steps <= 0 || speed <= 0 || measurements < 0 || diff_weight < 0.0f) {
        return false;
    }
    int i = config->profile_count++;
    config->profile_num[i] = num;
    config->profile_steps[i] = steps;
    config->profile_speed[i] = speed;
    config->profile_measurements[i] = measurements;
    config->profile_weight[i] = diff_weight;
    config->profile_reverse[i] = json_bool(entry, "reverse", false);
    return true;
}

esp_err_t rt_storage_load_profile(const char *profile_name, rt_config_t *config)
{
    char path[128];
    snprintf(path, sizeof(path), "%s/profiles/%s.txt", RT_SD_MOUNT_POINT, profile_name);
    char *data = read_file(path);
    if (!data) {
        return ESP_FAIL;
    }
    cJSON *doc = cJSON_Parse(data);
    free(data);
    if (!cJSON_IsObject(doc)) {
        cJSON_Delete(doc);
        set_error("profile json parse failed: %s", path);
        return ESP_FAIL;
    }

    config->profile_tolerance = 0.0f;
    config->profile_alarm_threshold = 0.0f;
    config->profile_weight_gap = 1.0f;
    config->profile_general_measurements = 20;
    config->profile_count = 0;
    memset(config->profile_stepper_enabled, 0, sizeof(config->profile_stepper_enabled));
    memset(config->profile_stepper_units_per_throw, 0, sizeof(config->profile_stepper_units_per_throw));
    memset(config->profile_stepper_units_per_throw_speed, 0, sizeof(config->profile_stepper_units_per_throw_speed));

    const cJSON *general = obj(doc, "general");
    if (general) {
        config->target_weight = (float)json_double(general, "targetWeight", config->target_weight);
        config->profile_tolerance = (float)json_double(general, "tolerance", 0.0);
        config->profile_alarm_threshold = (float)json_double(general, "alarmThreshold", 0.0);
        config->profile_weight_gap = (float)json_double(general, "weightGap", 1.0);
        config->profile_general_measurements = json_int(general, "measurements", 20);
    }

    const cJSON *actuator = obj(doc, "actuator");
    for (int stepper = 1; actuator && stepper <= 2; stepper++) {
        const cJSON *s = obj(actuator, stepper == 1 ? "stepper1" : "stepper2");
        if (s) {
            config->profile_stepper_enabled[stepper] = json_bool(s, "enabled", true);
            config->profile_stepper_units_per_throw[stepper] = json_double(s, "unitsPerThrow", 0.0);
            config->profile_stepper_units_per_throw_speed[stepper] = json_int(s, "unitsPerThrowSpeed", 0);
        }
    }

    const cJSON *map = cJSON_GetObjectItemCaseSensitive(doc, "rs232TrickleMap");
    if (cJSON_IsArray(map)) {
        const cJSON *entry = NULL;
        cJSON_ArrayForEach(entry, map) {
            if (!load_profile_entry(entry, config)) {
                cJSON_Delete(doc);
                set_error("invalid profile entry: %s", path);
                return ESP_FAIL;
            }
        }
    } else if (!load_profile_entry(doc, config)) {
        cJSON_Delete(doc);
        set_error("profile has no usable entries: %s", path);
        return ESP_FAIL;
    }
    cJSON_Delete(doc);
    return config->profile_count > 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t rt_storage_save_profile_target(const char *profile_name, float target_weight)
{
    char path[128];
    char tmp[136];
    snprintf(path, sizeof(path), "%s/profiles/%s.txt", RT_SD_MOUNT_POINT, profile_name);
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    char *data = read_file(path);
    if (!data) {
        return ESP_FAIL;
    }
    cJSON *doc = cJSON_Parse(data);
    free(data);
    if (!cJSON_IsObject(doc)) {
        cJSON_Delete(doc);
        set_error("profile json parse failed: %s", path);
        return ESP_FAIL;
    }
    cJSON *general = cJSON_GetObjectItemCaseSensitive(doc, "general");
    if (!cJSON_IsObject(general)) {
        general = cJSON_AddObjectToObject(doc, "general");
    }
    if (cJSON_HasObjectItem(general, "targetWeight")) {
        cJSON_ReplaceItemInObject(general, "targetWeight", cJSON_CreateNumber(target_weight));
    } else {
        cJSON_AddNumberToObject(general, "targetWeight", target_weight);
    }
    unlink(tmp);
    esp_err_t ret = write_json_file(tmp, doc);
    cJSON_Delete(doc);
    if (ret != ESP_OK) {
        unlink(tmp);
        return ret;
    }
    unlink(path);
    if (rename(tmp, path) != 0) {
        set_error("replace profile failed: %s", path);
        unlink(tmp);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static bool profile_name_exists(char names[RT_MAX_PROFILE_NAMES][32], uint8_t count, const char *name)
{
    for (uint8_t i = 0; i < count; i++) {
        if (strcmp(names[i], name) == 0) {
            return true;
        }
    }
    return false;
}

esp_err_t rt_storage_get_profile_list(char names[RT_MAX_PROFILE_NAMES][32], uint8_t *count)
{
    *count = 0;
    char dir_path[96];
    snprintf(dir_path, sizeof(dir_path), "%s/profiles", RT_SD_MOUNT_POINT);
    DIR *dir = opendir(dir_path);
    if (!dir) {
        set_error("open profiles dir failed: %s", dir_path);
        return ESP_FAIL;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && *count < RT_MAX_PROFILE_NAMES) {
        const char *name = entry->d_name;
        size_t len = strlen(name);
        if (len <= 4 || strcmp(name + len - 4, ".txt") != 0 || strstr(name, ".cor")) {
            continue;
        }
        char profile[32];
        snprintf(profile, sizeof(profile), "%.*s", (int)(len - 4), name);
        if (!profile_name_exists(names, *count, profile)) {
            strlcpy(names[*count], profile, 32);
            (*count)++;
        }
    }
    closedir(dir);
    return ESP_OK;
}

esp_err_t rt_storage_create_profile_from_calibration(const rt_config_t *config, float weight, char *profile_name, size_t profile_name_size)
{
    if (weight <= 0.0f) {
        set_error("invalid calibration weight");
        return ESP_ERR_INVALID_ARG;
    }
    char dir_path[96];
    snprintf(dir_path, sizeof(dir_path), "%s/profiles", RT_SD_MOUNT_POINT);
    mkdir(dir_path, 0775);

    char path[128];
    profile_name[0] = '\0';
    for (int i = 0; i <= 999; i++) {
        snprintf(profile_name, profile_name_size, "powder_%03d", i);
        snprintf(path, sizeof(path), "%s/%s.txt", dir_path, profile_name);
        struct stat st;
        if (stat(path, &st) != 0) {
            break;
        }
        profile_name[0] = '\0';
    }
    if (profile_name[0] == '\0') {
        set_error("no free profile name");
        return ESP_FAIL;
    }

    const float diff_weights[5] = {1.929f, 0.965f, 0.482f, 0.241f, 0.0f};
    const int measurements[5] = {2, 2, 5, 10, 15};
    const float units_per_throw = weight / 100.0f;
    int speed = config->profile_speed[0] > 0 ? config->profile_speed[0] : 200;

    cJSON *doc = cJSON_CreateObject();
    cJSON *general = cJSON_AddObjectToObject(doc, "general");
    cJSON_AddNumberToObject(general, "targetWeight", config->target_weight);
    cJSON_AddNumberToObject(general, "tolerance", 0.0);
    cJSON_AddNumberToObject(general, "alarmThreshold", 1.0);
    cJSON_AddNumberToObject(general, "weightGap", 1.0);
    cJSON_AddNumberToObject(general, "measurements", config->profile_general_measurements > 0 ? config->profile_general_measurements : 20);
    cJSON *actuator = cJSON_AddObjectToObject(doc, "actuator");
    cJSON *stepper1 = cJSON_AddObjectToObject(actuator, "stepper1");
    cJSON_AddBoolToObject(stepper1, "enabled", true);
    cJSON_AddNumberToObject(stepper1, "unitsPerThrow", units_per_throw);
    cJSON_AddNumberToObject(stepper1, "unitsPerThrowSpeed", speed);
    cJSON *stepper2 = cJSON_AddObjectToObject(actuator, "stepper2");
    cJSON_AddBoolToObject(stepper2, "enabled", config->profile_stepper_enabled[2]);
    cJSON_AddNumberToObject(stepper2, "unitsPerThrow", config->profile_stepper_units_per_throw[2] > 0.0 ? config->profile_stepper_units_per_throw[2] : 10.0);
    cJSON_AddNumberToObject(stepper2, "unitsPerThrowSpeed", config->profile_stepper_units_per_throw_speed[2] > 0 ? config->profile_stepper_units_per_throw_speed[2] : 200);
    cJSON *map = cJSON_AddArrayToObject(doc, "rs232TrickleMap");
    for (int i = 0; i < 5; i++) {
        long steps = lroundf(((diff_weights[i] * 200.0f) / units_per_throw) * 0.65f);
        if (steps < 5) {
            steps = 5;
        }
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "diffWeight", diff_weights[i]);
        cJSON_AddStringToObject(item, "actuator", "stepper1");
        cJSON_AddNumberToObject(item, "steps", steps);
        cJSON_AddNumberToObject(item, "speed", speed);
        cJSON_AddBoolToObject(item, "reverse", false);
        cJSON_AddNumberToObject(item, "measurements", measurements[i]);
        cJSON_AddItemToArray(map, item);
    }
    esp_err_t ret = write_json_file(path, doc);
    cJSON_Delete(doc);
    return ret;
}
