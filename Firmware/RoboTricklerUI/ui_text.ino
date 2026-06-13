const char *langText(const char *key)
{
  if (strcmp(key, "tab_trickler") == 0)
    return "Trickler";
  if (strcmp(key, "tab_profile") == 0)
    return "Profile";
  if (strcmp(key, "tab_info") == 0)
    return "Info";
  if (strcmp(key, "placeholder_profile") == 0)
    return "Profile";
  if (strcmp(key, "status_ready") == 0)
    return "Ready";
  if (strcmp(key, "status_stopped") == 0)
    return "Stopped";
  if (strcmp(key, "status_get_weight") == 0)
    return "Get Weight...";
  if (strcmp(key, "status_running") == 0)
    return "Running...";
  if (strcmp(key, "status_waiting_zero") == 0)
    return "Waiting for 0.000...";
  if (strcmp(key, "status_done") == 0)
    return "Done :)";
  if (strcmp(key, "status_count") == 0)
    return " Count:";
  if (strcmp(key, "status_init_steppers") == 0)
    return "Init steppers...";
  if (strcmp(key, "status_loading_config") == 0)
    return "Loading config...";
  if (strcmp(key, "status_reading_profiles") == 0)
    return "Reading profiles...";
  if (strcmp(key, "status_starting_scale") == 0)
    return "Starting scale serial...";
  if (strcmp(key, "status_saving_target") == 0)
    return "Writing target to profile...";
  if (strcmp(key, "status_target_saved") == 0)
    return "Target saved";
  if (strcmp(key, "status_search_profiles") == 0)
    return "Search Profiles...";
  if (strcmp(key, "status_refresh_profile_list") == 0)
    return "Refreshing profile list...";
  if (strcmp(key, "status_saving_target_failed") == 0)
    return "Target weight save failed!";
  if (strcmp(key, "status_timeout") == 0)
    return "Timeout! Check RS232 Wiring & Settings!";
  if (strcmp(key, "status_bulk_failed") == 0)
    return "Bulk trickle failed!";
  if (strcmp(key, "status_invalid_profile") == 0)
    return "Invalid profile!";
  if (strcmp(key, "status_invalid_stepper") == 0)
    return "Invalid stepper number!";
  if (strcmp(key, "status_reconnecting_wifi") == 0)
    return "Reconnecting to WiFi...";
  if (strcmp(key, "msg_config_default") == 0)
    return "Default Config generated.";
  if (strcmp(key, "msg_profile_corrupted") == 0)
    return "Profile Corrupted / Not Found:\n\n";
  if (strcmp(key, "msg_calibration_profile_loaded") == 0)
    return "\n\nCalibration Profile Loaded.";
  if (strcmp(key, "msg_unknown_config_read_error") == 0)
    return "Unknown config read error";
  if (strcmp(key, "msg_config_corrupted") == 0)
    return "Config File Corrupted / Not Found!\n\n";
  if (strcmp(key, "msg_over_trickle") == 0)
    return "!Over trickle!\n!Check weight!";
  if (strcmp(key, "msg_create_profile_prompt") == 0)
    return "Create profile from calibration?\n\nWeight: ";
  if (strcmp(key, "msg_profile_created") == 0)
    return "Profile created:\n\n";
  if (strcmp(key, "msg_create_profile_failed") == 0)
    return "Could not create profile!";
  if (strcmp(key, "msg_calibration_weight_invalid") == 0)
    return "Calibration weight invalid!";
  if (strcmp(key, "msg_no_free_profile_name") == 0)
    return "No free powder profile name!";
  if (strcmp(key, "msg_profiles_folder_failed") == 0)
    return "Failed to create profiles folder!";
  if (strcmp(key, "msg_profile_file_create_failed") == 0)
    return "Failed to create profile!";
  if (strcmp(key, "msg_profile_file_write_failed") == 0)
    return "Failed to write profile!";
  if (strcmp(key, "msg_invalid_profiles_found") == 0)
    return "Invalid profiles found:";
  if (strcmp(key, "msg_invalid_profiles_ignored") == 0)
    return "\n\nThey were ignored.";
  if (strcmp(key, "status_invalid_profile_ignored") == 0)
    return "Invalid profile ignored: ";
  if (strcmp(key, "status_profile_created") == 0)
    return "Profile created: ";
  if (strcmp(key, "status_creating_profile") == 0)
    return "Creating profile: ";
  if (strcmp(key, "status_writing_profile") == 0)
    return "Writing profile: ";
  if (strcmp(key, "status_selecting_profile") == 0)
    return "Selecting profile: ";
  if (strcmp(key, "status_loading_profile") == 0)
    return "Loading profile: ";
  if (strcmp(key, "status_profile_ready") == 0)
    return "Profile ready: ";
  if (strcmp(key, "status_tuning_profile") == 0)
    return "Tuning profile: ";
  if (strcmp(key, "status_starting_trickler") == 0)
    return "Starting trickler...";
  if (strcmp(key, "status_profile_selected_suffix") == 0)
    return " selected!";
  if (strcmp(key, "status_connect_wifi") == 0)
    return "Connect to Wifi: ";
  if (strcmp(key, "status_static_ip_failed") == 0)
    return "Static IP Failed to configure!";
  if (strcmp(key, "status_dns_failed") == 0)
    return "DNS Failed to configure!";
  if (strcmp(key, "status_wifi_connected") == 0)
    return "Wifi Connected";
  if (strcmp(key, "status_no_wifi") == 0)
    return "No Wifi";
  if (strcmp(key, "status_open_browser_prefix") == 0)
    return "Open http://";
  if (strcmp(key, "status_open_browser_suffix") == 0)
    return ".local in your browser";
  if (strcmp(key, "msg_new_firmware") == 0)
    return "New firmware available:\n\nv";
  if (strcmp(key, "msg_check_url") == 0)
    return "\n\nCheck: https://robo-trickler.de";
  if (strcmp(key, "status_update_failed") == 0)
    return "Update failed: ";
  if (strcmp(key, "status_update_upload") == 0)
    return "Update: ";
  if (strcmp(key, "status_update_success") == 0)
    return "Update Success: ";
  if (strcmp(key, "status_update_write_failed") == 0)
    return "Update write failed: ";
  if (strcmp(key, "status_update_end_failed") == 0)
    return "Update end failed: ";
  if (strcmp(key, "status_update_unexpected") == 0)
    return "Update Failed Unexpectedly (likely broken connection)";
  if (strcmp(key, "web_back") == 0)
    return "Back";
  if (strcmp(key, "web_reboot_now") == 0)
    return "Reboot now.";
  if (strcmp(key, "web_value_set") == 0)
    return "Value set.";
  if (strcmp(key, "web_running") == 0)
    return "Running...";
  if (strcmp(key, "web_stopped") == 0)
    return "Stopped...";
  if (strcmp(key, "web_fw_version") == 0)
    return "FW-Version";
  if (strcmp(key, "web_update") == 0)
    return "Update";
  if (strcmp(key, "web_update_ok") == 0)
    return "OK";
  if (strcmp(key, "web_update_fail") == 0)
    return "FAIL";
  if (strcmp(key, "err_incomplete_profile_entry") == 0)
    return "Incomplete profile entry:\n";
  if (strcmp(key, "err_entry") == 0)
    return "\nEntry: ";
  if (strcmp(key, "err_required_profile_entry") == 0)
    return "\nRequired: diffWeight, actuator, steps, speed, measurements";
  if (strcmp(key, "err_invalid_profile_values") == 0)
    return "Invalid profile values:\n";
  if (strcmp(key, "err_calibration_profile_incomplete") == 0)
    return "Calibration profile is incomplete:\n";
  if (strcmp(key, "err_required_calibration_profile") == 0)
    return "\nRequired: actuator, steps, speed";
  if (strcmp(key, "err_profile_missing_map") == 0)
    return "Profile has no rs232TrickleMap:\n";
  if (strcmp(key, "err_profile_file_not_found") == 0)
    return "Profile file not found:\n";
  if (strcmp(key, "err_could_not_open_profile_file") == 0)
    return "Could not open profile file:\n";
  if (strcmp(key, "err_root_not_json") == 0)
    return "Root is not a JSON object";
  if (strcmp(key, "err_profile_json_parse_failed") == 0)
    return "Profile JSON parse failed:\n";
  if (strcmp(key, "err_profile_has_no_entries") == 0)
    return "Profile has no entries:\n";
  if (strcmp(key, "err_could_not_open_config_file") == 0)
    return "Could not open config file:\n";
  if (strcmp(key, "err_config_json_parse_failed") == 0)
    return "Config JSON parse failed:\n";
  if (strcmp(key, "err_could_not_write_profile_file") == 0)
    return "Could not write profile file:\n";
  if (strcmp(key, "err_could_not_replace_profile_file") == 0)
    return "Could not replace profile file:\n";
  if (strcmp(key, "msg_cannot_delete_calibrate_profile") == 0)
    return "Cannot delete calibrate profile";
  if (strcmp(key, "msg_delete_profile_file_not_found") == 0)
    return "Profile file not found: ";
  if (strcmp(key, "msg_delete_profile_confirm_prefix") == 0)
    return "Delete profile ";
  if (strcmp(key, "msg_delete_profile_confirm_suffix") == 0)
    return "?";
  if (strcmp(key, "msg_cannot_delete_profile") == 0)
    return "Cannot delete profile";
  if (strcmp(key, "msg_delete_profile_calibrate_missing") == 0)
    return "Cannot delete profile: calibrate profile missing";
  if (strcmp(key, "msg_delete_profile_calibrate_load_failed") == 0)
    return "Cannot delete profile: failed to load calibrate";
  if (strcmp(key, "msg_could_not_delete_profile") == 0)
    return "Could not delete profile: ";
  if (strcmp(key, "msg_profile_deleted") == 0)
    return "Profile deleted: ";
  if (strcmp(key, "msg_stop_trickler_before_tune_profile") == 0)
    return "Stop trickler before tuning profile";
  if (strcmp(key, "msg_cannot_tune_profile") == 0)
    return "Cannot tune profile";
  if (strcmp(key, "msg_tune_profile_title") == 0)
    return "units/throw";
  if (strcmp(key, "msg_could_not_tune_profile") == 0)
    return "Could not tune profile";
  if (strcmp(key, "msg_profile_tuned") == 0)
    return "Profile tuned: ";
  return key;
}
