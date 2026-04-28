#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "actuator.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PROFILE_DIR                "/sdcard/profiles"
#define PROFILE_NAME_MAX_LEN       32
#define PROFILE_LOAD_PATH_MAX_LEN  96
#define PROFILE_MAX_TRICKLE_STEPS  16

typedef struct {
    bool   enabled;
    double units_per_throw;
    int    units_per_throw_speed;
} profile_stepper_actuator_t;

typedef struct {
    double target_weight;
    double tolerance;
    double trickle_gap;
    double alarm_threshold;
} profile_general_t;

typedef struct {
    bool          has_time;
    bool          has_actuator;
    double        diff_weight;
    actuator_id_t actuator;
    int           steps;
    double        time;
    int           speed;
    bool          reverse;
    int           measurements;
} profile_trickle_step_t;

typedef struct {
    profile_stepper_actuator_t stepper1;
    profile_stepper_actuator_t stepper2;
} profile_actuator_t;

typedef struct {
    char                    name[PROFILE_NAME_MAX_LEN];
    profile_general_t       general;
    profile_actuator_t      actuator;
    profile_trickle_step_t  rs232_trickle[PROFILE_MAX_TRICKLE_STEPS];
    size_t                  rs232_trickle_count;
    int64_t                 file_size_bytes;
    int64_t                 file_mtime;
    bool                    file_state_valid;
} app_profile_t;

/**
 * @brief Load a profile file from PROFILE_DIR into *profile.
 *
 * The caller passes a profile name without the .json suffix, for example
 * "basic_s". The SD card must already be mounted.
 *
 * @param[in]  profile_name  Profile base name, without path or suffix.
 * @param[out] profile       Caller-provided struct to populate.
 * @return
 *   - ESP_OK               on success
 *   - ESP_ERR_INVALID_ARG  if an argument is invalid
 *   - ESP_ERR_NOT_FOUND    if the file does not exist
 *   - ESP_ERR_NO_MEM       if allocation fails
 *   - ESP_FAIL             on parse or I/O errors
 */
esp_err_t profile_load(const char *profile_name, app_profile_t *profile);
esp_err_t profile_reload_if_changed(app_profile_t *profile, bool *out_reloaded);
esp_err_t profile_init_default(app_profile_t *profile, const char *profile_name);
esp_err_t profile_save(const app_profile_t *profile);
esp_err_t profile_delete(const char *profile_name);
const char *profile_actuator_name(actuator_id_t actuator_id);
esp_err_t profile_actuator_id_from_name(const char *name, actuator_id_t *out_id);
uint32_t profile_trickle_measurements(const profile_trickle_step_t *step);
const profile_stepper_actuator_t *profile_get_stepper_actuator_config(const app_profile_t *profile,
                                                                      actuator_id_t actuator_id);

#ifdef __cplusplus
}
#endif
