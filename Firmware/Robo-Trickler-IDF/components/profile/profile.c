#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "cJSON.h"
#include "esp_check.h"
#include "esp_log.h"

#include "profile.h"
#include "storage_utils.h"

static const char *TAG = "profile";

#define PROFILE_ACTUATOR_NAME_STEPPER1 "stepper1"
#define PROFILE_ACTUATOR_NAME_STEPPER2 "stepper2"

#define PROFILE_DEFAULT_TARGET_WEIGHT    40.000
#define PROFILE_DEFAULT_TOLERANCE        0.000
#define PROFILE_DEFAULT_TRICKLE_GAP      1.000
#define PROFILE_DEFAULT_ALARM_THRESHOLD  1.000
#define PROFILE_DEFAULT_STEPPER_SPEED    200
#define PROFILE_DEFAULT_UNITS_PER_THROW  1.000
#define PROFILE_DEFAULT_RS232_STEPS      10
#define PROFILE_DEFAULT_RS232_COUNT      5U

static esp_err_t profile_build_path(const char *profile_name, char *path, size_t path_size)
{
    int path_len = 0;

    ESP_RETURN_ON_FALSE(profile_name != NULL && profile_name[0] != '\0',
                        ESP_ERR_INVALID_ARG, TAG, "profile_name is invalid");
    ESP_RETURN_ON_FALSE(path != NULL && path_size > 0U,
                        ESP_ERR_INVALID_ARG, TAG, "path buffer is invalid");
    ESP_RETURN_ON_FALSE(strchr(profile_name, '/') == NULL && strchr(profile_name, '\\') == NULL,
                        ESP_ERR_INVALID_ARG, TAG, "Invalid profile name: %s", profile_name);

    path_len = snprintf(path, path_size, "%s/%s.json", PROFILE_DIR, profile_name);
    ESP_RETURN_ON_FALSE(path_len >= 0 && path_len < (int)path_size,
                        ESP_ERR_INVALID_ARG, TAG, "Profile path too long for %s", profile_name);
    return ESP_OK;
}

static void profile_clear_file_state(app_profile_t *profile)
{
    if (profile == NULL) {
        return;
    }

    profile->file_size_bytes = 0;
    profile->file_mtime = 0;
    profile->file_state_valid = false;
}

static esp_err_t profile_update_file_state(app_profile_t *profile, const char *path)
{
    struct stat st = {0};

    ESP_RETURN_ON_FALSE(profile != NULL, ESP_ERR_INVALID_ARG, TAG, "profile is null");
    ESP_RETURN_ON_FALSE(path != NULL && path[0] != '\0', ESP_ERR_INVALID_ARG, TAG, "path is invalid");

    if (stat(path, &st) != 0) {
        profile_clear_file_state(profile);
        ESP_LOGW(TAG, "stat failed for %s: errno=%d", path, errno);
        return (errno == ENOENT) ? ESP_ERR_NOT_FOUND : ESP_FAIL;
    }

    profile->file_size_bytes = (int64_t)st.st_size;
    profile->file_mtime = (int64_t)st.st_mtime;
    profile->file_state_valid = true;
    return ESP_OK;
}
static cJSON_bool profile_add_fixed3_number(cJSON *object, const char *name, double value)
{
    char formatted_value[24];
    cJSON *raw_number = NULL;
    int len = 0;

    if (object == NULL || name == NULL) {
        return 0;
    }

    len = snprintf(formatted_value, sizeof(formatted_value), "%.3f", value);
    if (len <= 0 || len >= (int)sizeof(formatted_value)) {
        return 0;
    }

    raw_number = cJSON_CreateRaw(formatted_value);
    if (raw_number == NULL) {
        return 0;
    }

    if (!cJSON_AddItemToObject(object, name, raw_number)) {
        cJSON_Delete(raw_number);
        return 0;
    }

    return 1;
}

static void profile_quarantine_invalid_file(const char *path)
{
    char err_path[PROFILE_LOAD_PATH_MAX_LEN];
    int path_len = 0;

    if (path == NULL || path[0] == '\0') {
        return;
    }

    path_len = snprintf(err_path, sizeof(err_path), "%s", path);
    if (path_len <= 0 || path_len >= (int)sizeof(err_path)) {
        ESP_LOGW(TAG, "Cannot quarantine invalid profile, path too long: %s", path);
        return;
    }

    if (path_len >= 5 && strcmp(&err_path[path_len - 5], ".json") == 0) {
        snprintf(&err_path[path_len - 5], sizeof(err_path) - (size_t)(path_len - 5), ".err");
    } else {
        path_len = snprintf(err_path, sizeof(err_path), "%s.err", path);
        if (path_len <= 0 || path_len >= (int)sizeof(err_path)) {
            ESP_LOGW(TAG, "Cannot quarantine invalid profile, err path too long: %s", path);
            return;
        }
    }

    if (rename(path, err_path) != 0) {
        ESP_LOGW(TAG,
                 "Failed to quarantine invalid profile %s -> %s: errno=%d",
                 path,
                 err_path,
                 errno);
        return;
    }

    ESP_LOGW(TAG, "Renamed invalid profile %s -> %s", path, err_path);
}

static bool read_int(int *out_value, const cJSON *item)
{
    if (out_value == NULL || !cJSON_IsNumber(item)) {
        return false;
    }

    *out_value = item->valueint;
    return true;
}

static bool read_double(double *out_value, const cJSON *item)
{
    if (out_value == NULL || !cJSON_IsNumber(item)) {
        return false;
    }

    *out_value = item->valuedouble;
    return true;
}

static bool read_bool(bool *out_value, const cJSON *item)
{
    if (out_value == NULL || !cJSON_IsBool(item)) {
        return false;
    }

    *out_value = cJSON_IsTrue(item);
    return true;
}

const char *profile_actuator_name(actuator_id_t actuator_id)
{
    switch (actuator_id) {
    case ACTUATOR_ID_STEPPER1:
        return PROFILE_ACTUATOR_NAME_STEPPER1;
    case ACTUATOR_ID_STEPPER2:
        return PROFILE_ACTUATOR_NAME_STEPPER2;
    default:
        return "unknown";
    }
}

esp_err_t profile_actuator_id_from_name(const char *name, actuator_id_t *out_id)
{
    if (out_id == NULL || name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (strcmp(name, PROFILE_ACTUATOR_NAME_STEPPER1) == 0) {
        *out_id = ACTUATOR_ID_STEPPER1;
        return ESP_OK;
    }
    if (strcmp(name, PROFILE_ACTUATOR_NAME_STEPPER2) == 0) {
        *out_id = ACTUATOR_ID_STEPPER2;
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Unsupported actuator '%s'", name);
    return ESP_ERR_NOT_SUPPORTED;
}

uint32_t profile_trickle_measurements(const profile_trickle_step_t *step)
{
    if (step == NULL || step->measurements <= 0) {
        return 1U;
    }

    return (uint32_t)step->measurements;
}

const profile_stepper_actuator_t *profile_get_stepper_actuator_config(const app_profile_t *profile,
                                                                      actuator_id_t actuator_id)
{
    if (profile == NULL) {
        return NULL;
    }

    switch (actuator_id) {
    case ACTUATOR_ID_STEPPER2:
        return &profile->actuator.stepper2;
    case ACTUATOR_ID_STEPPER1:
        return &profile->actuator.stepper1;
    default:
        return NULL;
    }
}

static bool profile_read_actuator_id(actuator_id_t *out_id, const cJSON *item)
{
    if (out_id == NULL || !cJSON_IsString(item) || item->valuestring == NULL) {
        return false;
    }

    if (profile_actuator_id_from_name(item->valuestring, out_id) == ESP_OK) {
        return true;
    }

    return false;
}

static void profile_init_default_rs232_trickle(app_profile_t *profile)
{
    static const double diff_weights[PROFILE_DEFAULT_RS232_COUNT] = { 1.929, 0.965, 0.482, 0.241, 0.000 };
    static const int diff_measurements[PROFILE_DEFAULT_RS232_COUNT] = { 2, 2, 10, 10, 15 };

    if (profile == NULL) {
        return;
    }

    profile->rs232_trickle_count = PROFILE_DEFAULT_RS232_COUNT;
    for (size_t i = 0; i < PROFILE_DEFAULT_RS232_COUNT; ++i) {
        profile_trickle_step_t *step = &profile->rs232_trickle[i];

        memset(step, 0, sizeof(*step));
        step->has_actuator = true;
        step->actuator = ACTUATOR_ID_STEPPER1;
        step->diff_weight = diff_weights[i];
        step->steps = PROFILE_DEFAULT_RS232_STEPS;
        step->speed = PROFILE_DEFAULT_STEPPER_SPEED;
        step->reverse = false;
        step->measurements = diff_measurements[i];
        
    }
}

static void profile_parse_stepper_actuator(profile_stepper_actuator_t *actuator, const cJSON *root)
{
    if (actuator == NULL || !cJSON_IsObject(root)) {
        return;
    }

    read_bool(&actuator->enabled, cJSON_GetObjectItemCaseSensitive(root, "enabled"));
    read_double(&actuator->units_per_throw, cJSON_GetObjectItemCaseSensitive(root, "unitsPerThrow"));
    read_int(&actuator->units_per_throw_speed, cJSON_GetObjectItemCaseSensitive(root, "unitsPerThrowSpeed"));
}

static void profile_parse_rs232_trickle(app_profile_t *profile, const cJSON *root)
{
    size_t index = 0;
    const cJSON *item = NULL;

    if (profile == NULL || !cJSON_IsArray(root)) {
        return;
    }

    cJSON_ArrayForEach(item, root) {
        profile_trickle_step_t *step = NULL;

        if (index >= PROFILE_MAX_TRICKLE_STEPS) {
            ESP_LOGW(TAG, "Truncating rs232TrickleMap to %u entries", PROFILE_MAX_TRICKLE_STEPS);
            break;
        }

        if (!cJSON_IsObject(item)) {
            continue;
        }

        step = &profile->rs232_trickle[index];
        read_double(&step->diff_weight, cJSON_GetObjectItemCaseSensitive(item, "diffWeight"));
        step->has_actuator = profile_read_actuator_id(&step->actuator,
                                                      cJSON_GetObjectItemCaseSensitive(item, "actuator"));
        read_int(&step->steps, cJSON_GetObjectItemCaseSensitive(item, "steps"));
        step->has_time = read_double(&step->time, cJSON_GetObjectItemCaseSensitive(item, "time"));
        read_int(&step->speed, cJSON_GetObjectItemCaseSensitive(item, "speed"));
        read_bool(&step->reverse, cJSON_GetObjectItemCaseSensitive(item, "reverse"));
        read_int(&step->measurements, cJSON_GetObjectItemCaseSensitive(item, "measurements"));
        index++;
    }

    profile->rs232_trickle_count = index;
}

esp_err_t profile_load(const char *profile_name, app_profile_t *profile)
{
    char path[PROFILE_LOAD_PATH_MAX_LEN];
    char requested_name[PROFILE_NAME_MAX_LEN];
    cJSON *root = NULL;
    esp_err_t ret = ESP_OK;
    const cJSON *section = NULL;

    if (profile_name == NULL || profile == NULL || profile_name[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_FALSE(strlen(profile_name) < sizeof(requested_name),
                        ESP_ERR_INVALID_ARG, TAG, "profile name too long: %s", profile_name);
    snprintf(requested_name, sizeof(requested_name), "%s", profile_name);
    ESP_RETURN_ON_ERROR(profile_build_path(requested_name, path, sizeof(path)), TAG, "profile path build failed");

    memset(profile, 0, sizeof(*profile));
    snprintf(profile->name, sizeof(profile->name), "%s", requested_name);

    ret = storage_utils_parse_json_file(path, TAG, &root);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            profile_quarantine_invalid_file(path);
        }
        return ret;
    }

    section = cJSON_GetObjectItemCaseSensitive(root, "general");
    if (cJSON_IsObject(section)) {
        read_double(&profile->general.target_weight, cJSON_GetObjectItemCaseSensitive(section, "targetWeight"));
        read_double(&profile->general.tolerance, cJSON_GetObjectItemCaseSensitive(section, "tolerance"));
        read_double(&profile->general.trickle_gap, cJSON_GetObjectItemCaseSensitive(section, "weightGap"));
        read_double(&profile->general.alarm_threshold, cJSON_GetObjectItemCaseSensitive(section, "alarmThreshold"));
    }

    section = cJSON_GetObjectItemCaseSensitive(root, "actuator");
    if (cJSON_IsObject(section)) {
        profile_parse_stepper_actuator(&profile->actuator.stepper1,
                                       cJSON_GetObjectItemCaseSensitive(section, PROFILE_ACTUATOR_NAME_STEPPER1));
        profile_parse_stepper_actuator(&profile->actuator.stepper2,
                                       cJSON_GetObjectItemCaseSensitive(section, PROFILE_ACTUATOR_NAME_STEPPER2));
    }

    profile_parse_rs232_trickle(profile, cJSON_GetObjectItemCaseSensitive(root, "rs232TrickleMap"));

    cJSON_Delete(root);

    if (ret != ESP_OK) {
        profile_quarantine_invalid_file(path);
        return ret;
    }

    ESP_LOGI(TAG,
             "Profile loaded (%s, trickle_steps=%u)",
             profile->name,
             (unsigned)profile->rs232_trickle_count);
    (void)profile_update_file_state(profile, path);
    return ESP_OK;
}

esp_err_t profile_reload_if_changed(app_profile_t *profile, bool *out_reloaded)
{
    char path[PROFILE_LOAD_PATH_MAX_LEN];
    char profile_name[PROFILE_NAME_MAX_LEN];
    struct stat st = {0};
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(profile != NULL, ESP_ERR_INVALID_ARG, TAG, "profile is null");
    ESP_RETURN_ON_FALSE(profile->name[0] != '\0', ESP_ERR_INVALID_ARG, TAG, "profile name is empty");
    snprintf(profile_name, sizeof(profile_name), "%s", profile->name);

    if (out_reloaded != NULL) {
        *out_reloaded = false;
    }

    ESP_RETURN_ON_ERROR(profile_build_path(profile_name, path, sizeof(path)), TAG, "profile path build failed");

    if (stat(path, &st) != 0) {
        ESP_LOGW(TAG, "stat failed for %s: errno=%d", path, errno);
        profile_clear_file_state(profile);
        return (errno == ENOENT) ? ESP_ERR_NOT_FOUND : ESP_FAIL;
    }

    if (profile->file_state_valid &&
        profile->file_size_bytes == (int64_t)st.st_size &&
        profile->file_mtime == (int64_t)st.st_mtime) {
        return ESP_OK;
    }

    ret = profile_load(profile_name, profile);
    ESP_RETURN_ON_ERROR(ret, TAG, "profile reload failed");

    if (out_reloaded != NULL) {
        *out_reloaded = true;
    }

    return ESP_OK;
}

esp_err_t profile_init_default(app_profile_t *profile, const char *profile_name)
{
    if (profile == NULL || profile_name == NULL || profile_name[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    if (strchr(profile_name, '/') != NULL || strchr(profile_name, '\\') != NULL) {
        ESP_LOGE(TAG, "Invalid profile name: %s", profile_name);
        return ESP_ERR_INVALID_ARG;
    }

    memset(profile, 0, sizeof(*profile));

    snprintf(profile->name, sizeof(profile->name), "%s", profile_name);
    profile->general.target_weight = PROFILE_DEFAULT_TARGET_WEIGHT;
    profile->general.tolerance = PROFILE_DEFAULT_TOLERANCE;
    profile->general.trickle_gap = PROFILE_DEFAULT_TRICKLE_GAP;
    profile->general.alarm_threshold = PROFILE_DEFAULT_ALARM_THRESHOLD;

    profile->actuator.stepper1.enabled = true;
    profile->actuator.stepper1.units_per_throw = PROFILE_DEFAULT_UNITS_PER_THROW;
    profile->actuator.stepper1.units_per_throw_speed = PROFILE_DEFAULT_STEPPER_SPEED;

    profile_init_default_rs232_trickle(profile);

    profile_clear_file_state(profile);

    return ESP_OK;
}

esp_err_t profile_save(const app_profile_t *profile)
{
    char path[PROFILE_LOAD_PATH_MAX_LEN];
    cJSON *root = NULL;
    cJSON *general = NULL;
    cJSON *throw_calc_param = NULL;
    cJSON *stepper1 = NULL;
    cJSON *stepper2 = NULL;
    cJSON *rs232_trickle_map = NULL;
    char *json = NULL;
    esp_err_t ret = ESP_OK;
    size_t i = 0;

    if (profile == NULL || profile->name[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    if (strchr(profile->name, '/') != NULL || strchr(profile->name, '\\') != NULL) {
        ESP_LOGE(TAG, "Invalid profile name: %s", profile->name);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(profile_build_path(profile->name, path, sizeof(path)), TAG, "profile path build failed");

    root = cJSON_CreateObject();
    if (root == NULL) {
        return ESP_ERR_NO_MEM;
    }

    general = cJSON_AddObjectToObject(root, "general");
    throw_calc_param = cJSON_AddObjectToObject(root, "actuator");
    if (throw_calc_param != NULL && profile->actuator.stepper1.enabled) {
        stepper1 = cJSON_AddObjectToObject(throw_calc_param, PROFILE_ACTUATOR_NAME_STEPPER1);
    }
    if (throw_calc_param != NULL && profile->actuator.stepper2.enabled) {
        stepper2 = cJSON_AddObjectToObject(throw_calc_param, PROFILE_ACTUATOR_NAME_STEPPER2);
    }
    if (general == NULL || throw_calc_param == NULL ||
        (profile->actuator.stepper1.enabled && stepper1 == NULL) ||
        (profile->actuator.stepper2.enabled && stepper2 == NULL)) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    if (!profile_add_fixed3_number(general, "targetWeight", profile->general.target_weight) ||
        !profile_add_fixed3_number(general, "tolerance", profile->general.tolerance) ||
        !profile_add_fixed3_number(general, "alarmThreshold", profile->general.alarm_threshold)) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    if (profile->actuator.stepper1.enabled &&
        (!profile_add_fixed3_number(stepper1, "unitsPerThrow", profile->actuator.stepper1.units_per_throw) ||
         !cJSON_AddNumberToObject(stepper1, "unitsPerThrowSpeed", profile->actuator.stepper1.units_per_throw_speed) ||
         !cJSON_AddBoolToObject(stepper1, "enabled", true))) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    if (profile->actuator.stepper2.enabled &&
        (!profile_add_fixed3_number(stepper2, "unitsPerThrow", profile->actuator.stepper2.units_per_throw) ||
         !cJSON_AddNumberToObject(stepper2, "unitsPerThrowSpeed", profile->actuator.stepper2.units_per_throw_speed) ||
         !cJSON_AddBoolToObject(stepper2, "enabled", true))) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    if (profile->general.trickle_gap != 0.0 &&
        !profile_add_fixed3_number(general, "weightGap", profile->general.trickle_gap)) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    rs232_trickle_map = cJSON_AddArrayToObject(root, "rs232TrickleMap");
    if (rs232_trickle_map == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    for (i = 0; i < profile->rs232_trickle_count; ++i) {
        const profile_trickle_step_t *step = &profile->rs232_trickle[i];
        const char *actuator_name = NULL;
        cJSON *step_obj = cJSON_CreateObject();
        bool step_added = false;

        if (!step->has_actuator) {
            ESP_LOGE(TAG, "Cannot save rs232TrickleMap[%u]: actuator is invalid", (unsigned)i);
            ret = ESP_ERR_INVALID_STATE;
            goto cleanup;
        }

        actuator_name = profile_actuator_name(step->actuator);
        if (strcmp(actuator_name, "unknown") == 0) {
            ESP_LOGE(TAG, "Cannot save rs232TrickleMap[%u]: actuator enum %d is invalid", (unsigned)i, (int)step->actuator);
            ret = ESP_ERR_INVALID_STATE;
            goto cleanup;
        }

        if (step_obj == NULL) {
            ret = ESP_ERR_NO_MEM;
            goto cleanup;
        }

        if (!cJSON_AddItemToArray(rs232_trickle_map, step_obj)) {
            cJSON_Delete(step_obj);
            ret = ESP_ERR_NO_MEM;
            goto cleanup;
        }
        step_added = true;

        if (
            !profile_add_fixed3_number(step_obj, "diffWeight", step->diff_weight) ||
            !cJSON_AddStringToObject(step_obj, "actuator", actuator_name) ||
            !cJSON_AddNumberToObject(step_obj, "steps", step->steps) ||
            !cJSON_AddNumberToObject(step_obj, "speed", step->speed) ||
            !cJSON_AddBoolToObject(step_obj, "reverse", step->reverse) ||
            !cJSON_AddNumberToObject(step_obj, "measurements", step->measurements)) {
            if (!step_added) {
                cJSON_Delete(step_obj);
            }
            ret = ESP_ERR_NO_MEM;
            goto cleanup;
        }

        if (step->has_time && !profile_add_fixed3_number(step_obj, "time", step->time)) {
            ret = ESP_ERR_NO_MEM;
            goto cleanup;
        }
    }

    json = cJSON_Print(root);
    if (json == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    ret = storage_utils_write_text_file(path, json, TAG);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Profile saved to %s", path);
        (void)profile_update_file_state((app_profile_t *)profile, path);
    }

cleanup:
    if (json != NULL) {
        cJSON_free(json);
    }
    cJSON_Delete(root);
    return ret;
}

esp_err_t profile_delete(const char *profile_name)
{
    char path[PROFILE_LOAD_PATH_MAX_LEN];

    ESP_RETURN_ON_ERROR(profile_build_path(profile_name, path, sizeof(path)), TAG, "profile path build failed");

    if (remove(path) != 0) {
        ESP_LOGE(TAG, "Failed to delete profile %s: errno=%d", path, errno);
        return (errno == ENOENT) ? ESP_ERR_NOT_FOUND : ESP_FAIL;
    }

    ESP_LOGI(TAG, "Deleted profile %s", path);
    return ESP_OK;
}
