static const char *languageFallback(const char *key)
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
  if (strcmp(key, "status_storage_sd") == 0)
    return "Storage: SD Card";
  if (strcmp(key, "status_storage_flash") == 0)
    return "Storage: Internal Flash";
  if (strcmp(key, "status_wifi_ap") == 0)
    return "WiFi AP: ";
  if (strcmp(key, "status_wifi_password") == 0)
    return "WiFi password: ";
  if (strcmp(key, "status_wifi_open") == 0)
    return "Open http://";
  if (strcmp(key, "wifi_setup_mode") == 0)
    return "WiFi setup mode";
  if (strcmp(key, "wifi_connect_to") == 0)
    return "Connect to:";
  if (strcmp(key, "wifi_password") == 0)
    return "Password:";
  if (strcmp(key, "wifi_open") == 0)
    return "Open:";
  if (strcmp(key, "wifi_select_and_save") == 0)
    return "Select your WiFi and save.";
  if (strcmp(key, "msg_filesystem_mount_failed") == 0)
    return "Filesystem mount failed";
  if (strcmp(key, "msg_config_default") == 0)
    return "Default Config generated.";
  if (strcmp(key, "msg_profile_corrupted") == 0)
    return "Profile Corrupted / Not Found:\n\n";
  if (strcmp(key, "msg_calibration_profile_loaded") == 0)
    return "\n\nCalibration Profile Loaded.";
  if (strcmp(key, "msg_unknown_config_read_error") == 0)
    return "Unknown config read error";
  if (strcmp(key, "msg_sd_card_not_connected") == 0)
    return "SD card not connected.\n\nInternal Flash will be used instead.";
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
  if (strcmp(key, "status_update_begin_failed_suffix") == 0)
    return " update begin failed: ";
  if (strcmp(key, "status_update_write_failed_suffix") == 0)
    return " update write failed: ";
  if (strcmp(key, "status_update_failed_suffix") == 0)
    return " update failed: ";
  if (strcmp(key, "status_update_not_finished_suffix") == 0)
    return " update was not finished";
  if (strcmp(key, "status_update_completed_suffix") == 0)
    return " update completed";
  if (strcmp(key, "status_update_not_file_suffix") == 0)
    return " update is not a file: ";
  if (strcmp(key, "status_update_empty_suffix") == 0)
    return " update file is empty: ";
  if (strcmp(key, "status_update_start_prefix") == 0)
    return "Starting ";
  if (strcmp(key, "status_update_start_suffix") == 0)
    return " update from ";
  if (strcmp(key, "status_update_delete_failed") == 0)
    return "Could not delete completed update file: ";
  if (strcmp(key, "status_check_sd_updates") == 0)
    return "Checking SD card updates...";
  if (strcmp(key, "status_sd_update_complete") == 0)
    return "SD card update complete. Rebooting...";
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
  if (strcmp(key, "web_firmware_image") == 0)
    return "Firmware image";
  if (strcmp(key, "web_littlefs_image") == 0)
    return "LittleFS image";
  if (strcmp(key, "web_update_filesystem_warning") == 0)
    return "Uploading replaces all files in the internal filesystem.";
  if (strcmp(key, "web_update_firmware") == 0)
    return "Update Firmware";
  if (strcmp(key, "web_update_littlefs") == 0)
    return "Update LittleFS";
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

String activeUiLangJson = "";
String activeUiLangFilename = "";
JsonDocument activeUiLangDoc;

const char *langText(const char *key)
{
  JsonVariant translatedVariant = activeUiLangDoc["ui"][key];
  if (!translatedVariant.isNull())
  {
    const char *translated = translatedVariant.as<const char *>();
    if ((translated != nullptr) && (translated[0] != '\0'))
    {
      return translated;
    }
  }
  return languageFallback(key);
}

bool loadLanguage()
{
  activeUiLangJson = "";
  activeUiLangFilename = "";
  activeUiLangDoc.clear();

  String language = String(config.language);
  language.trim();
  language.toLowerCase();

  int separator = language.indexOf('-');
  if (separator < 0)
  {
    separator = language.indexOf('_');
  }
  if (separator > 0)
  {
    language = language.substring(0, separator);
  }
  if (language.length() <= 0)
  {
    language = "en";
  }
  strlcpy(config.language, language.c_str(), sizeof(config.language));

  String candidates[] = {
      "/lang/" + language + ".json",
      "/lang/en.json"};

  String filename = "";
  for (int i = 0; i < (int)(sizeof(candidates) / sizeof(candidates[0])); i++)
  {
    if (ACTIVE_FS.exists(candidates[i].c_str()))
    {
      filename = candidates[i];
      break;
    }
  }

  if (filename.length() <= 0)
  {
    DEBUG_PRINTLN("Language file not found; using built-in fallback");
    return false;
  }

  File file = ACTIVE_FS.open(filename.c_str());
  if (!file)
  {
    DEBUG_PRINT("Language file could not be opened: ");
    DEBUG_PRINTLN(filename);
    return false;
  }
  activeUiLangFilename = filename;

  activeUiLangJson.reserve(file.size() + 1);
  while (file.available())
  {
    activeUiLangJson += (char)file.read();
  }
  file.close();

  DeserializationError error = deserializeJson(activeUiLangDoc, activeUiLangJson);
  if (error || !activeUiLangDoc["ui"].is<JsonObject>())
  {
    DEBUG_PRINT("Language JSON parse failed: ");
    DEBUG_PRINTLN(error.c_str());
    activeUiLangJson = "";
    activeUiLangDoc.clear();
    return false;
  }

  DEBUG_PRINT("Loaded language: ");
  DEBUG_PRINT(config.language);
  DEBUG_PRINT(" from ");
  DEBUG_PRINTLN(activeUiLangFilename);
  return true;
}

void applyLanguage()
{
  if (!loadLanguage())
  {
    DEBUG_PRINTLN("Using built-in language fallback");
  }

  if (lvglLock())
  {
    lv_tabview_set_tab_text(ui_TabView, 0, langText("tab_trickler"));
    lv_tabview_set_tab_text(ui_TabView, 1, langText("tab_profile"));
    lv_tabview_set_tab_text(ui_TabView, 2, langText("tab_info"));

    if (!isTricklerRunning())
    {
      lv_label_set_text(ui_LabelToggleTrickler, UI_SYMBOL_START);
    }
    lv_label_set_text(ui_LabelMessageOk, UI_SYMBOL_OK);
    if (strcmp(lv_label_get_text(ui_LabelProfile), "Profile") == 0)
    {
      lv_label_set_text(ui_LabelProfile, langText("placeholder_profile"));
    }
    lvglUnlock();
  }

  updateWifiTabIndicator(true);
}
