// Built-in English fallback strings, used when the active SD lang file is
// missing or lacks a key. Kept as a flat lookup table instead of a strcmp
// if-chain: less code, and adding a key is one line.
struct LangFallbackEntry
{
  const char *key;
  const char *value;
};

static const LangFallbackEntry LANG_FALLBACKS[] = {
    {"tab_trickler", "Trickler"},
    {"tab_profile", "Profile"},
    {"tab_info", "Info"},
    {"placeholder_profile", "Profile"},
    {"label_scale", "Scale"},
    {"status_ready", "Ready"},
    {"status_stopped", "Stopped"},
    {"status_get_weight", "Get Weight..."},
    {"status_running", "Running..."},
    {"status_waiting_zero", "Waiting for 0.000..."},
    {"status_done", "Done :)"},
    {"status_count", " Count:"},
    {"status_init_steppers", "Init steppers..."},
    {"status_stepper_i2s_failed", "Stepper I2S init failed!"},
    {"status_loading_config", "Loading config..."},
    {"status_reading_profiles", "Reading profiles..."},
    {"status_starting_scale", "Starting scale serial..."},
    {"status_saving_target", "Writing target to profile..."},
    {"status_target_saved", "Target saved"},
    {"status_search_profiles", "Search Profiles..."},
    {"status_refresh_profile_list", "Refreshing profile list..."},
    {"status_saving_target_failed", "Target weight save failed!"},
    {"status_timeout", "Timeout! Check RS232 Wiring & Settings!"},
    {"status_bulk_failed", "Bulk trickle failed!"},
    {"status_invalid_profile", "Invalid profile!"},
    {"status_invalid_stepper", "Invalid stepper number!"},
    {"status_reconnecting_wifi", "Reconnecting to WiFi..."},
    {"status_storage_sd", "Storage: SD Card"},
    {"status_storage_flash", "Storage: Internal Flash"},
    {"label_sync_flash_to_sd", "Upload Flash > SD"},
    {"label_sync_sd_to_flash", "Download SD > Flash"},
    {"msg_sync_flash_to_sd_confirm", "Copy config and profiles\nfrom Flash to SD?"},
    {"msg_sync_sd_to_flash_confirm", "Copy config and profiles\nfrom SD to Flash?"},
    {"msg_stop_trickler_before_sync", "Stop trickler before syncing files"},
    {"msg_sync_filesystems_unavailable", "SD card and LittleFS must both be mounted"},
    {"msg_sync_failed", "Config/profile sync failed"},
    {"msg_sync_complete", "Sync complete: %d files copied"},
    {"msg_sync_restarting", "\n\nRestarting to load SD files."},
    {"status_wifi_ap", "WiFi AP: "},
    {"status_wifi_password", "WiFi password: "},
    {"status_wifi_open", "Open http://"},
    {"wifi_setup_mode", "WiFi setup mode"},
    {"wifi_connect_to", "Connect to:"},
    {"wifi_password", "Password:"},
    {"wifi_open", "Open:"},
    {"wifi_select_and_save", "Select your WiFi and save."},
    {"wifi_qr_click_to_hide", "Click to Hide"},
    {"msg_filesystem_mount_failed", "Filesystem mount failed"},
    {"msg_config_default", "Default Config generated."},
    {"msg_profile_corrupted", "Profile Corrupted / Not Found:\n\n"},
    {"msg_calibration_profile_loaded", "\n\nCalibration Profile Loaded."},
    {"msg_unknown_config_read_error", "Unknown config read error"},
#if ENABLE_LITTLEFS
    {"msg_sd_card_not_connected", "SD card not connected!\n\nInternal Flash will be used instead!"},
#else
    {"msg_sd_card_not_connected", "SD card not connected!"},
#endif
    {"msg_config_corrupted", "Config File Corrupted / Not Found!\n\n"},
    {"msg_over_trickle", "!Over trickle!\n!Check weight!"},
    {"msg_create_profile_prompt", "Create profile from calibration?\n\nWeight: "},
    {"msg_profile_created", "Profile created:\n\n"},
    {"msg_create_profile_failed", "Could not create profile!"},
    {"msg_calibration_weight_invalid", "Calibration weight invalid!"},
    {"msg_no_free_profile_name", "No free powder profile name!"},
    {"msg_max_profiles_reached", "Profile limit reached!\nCannot calibrate."},
    {"msg_profiles_folder_failed", "Failed to create profiles folder!"},
    {"msg_profile_file_create_failed", "Failed to create profile!"},
    {"msg_profile_file_write_failed", "Failed to write profile!"},
    {"msg_invalid_profiles_found", "Invalid profiles found:"},
    {"msg_invalid_profiles_ignored", "\n\nThey were ignored."},
    {"status_invalid_profile_ignored", "Invalid profile ignored: "},
    {"status_profile_created", "Profile created: "},
    {"status_creating_profile", "Creating profile: "},
    {"status_writing_profile", "Writing profile: "},
    {"status_selecting_profile", "Selecting profile: "},
    {"status_loading_profile", "Loading profile: "},
    {"status_profile_ready", "Profile ready: "},
    {"status_tuning_profile", "Tuning profile: "},
    {"status_starting_trickler", "Starting trickler..."},
    {"status_profile_selected_suffix", " selected!"},
    {"status_connect_wifi", "Connect to Wifi: "},
    {"status_static_ip_failed", "Static IP Failed to configure!"},
    {"status_dns_failed", "DNS Failed to configure!"},
    {"status_wifi_connected", "Wifi Connected"},
    {"status_no_wifi", "No Wifi"},
    {"status_open_browser_prefix", "Open http://"},
    {"status_open_browser_suffix", ".local in your browser"},
    {"msg_new_firmware", "New firmware available:\n\nv"},
    {"msg_check_url", "\n\nCheck: https://robo-trickler.de"},
    {"status_update_failed", "Update failed: "},
    {"status_update_upload", "Update: "},
    {"status_update_success", "Update Success: "},
    {"status_update_write_failed", "Update write failed: "},
    {"status_update_end_failed", "Update end failed: "},
    {"status_update_unexpected", "Update Failed Unexpectedly (likely broken connection)"},
    {"status_update_begin_failed_suffix", " update begin failed: "},
    {"status_update_write_failed_suffix", " update write failed: "},
    {"status_update_failed_suffix", " update failed: "},
    {"status_update_not_finished_suffix", " update was not finished"},
    {"status_update_completed_suffix", " update completed"},
    {"status_update_not_file_suffix", " update is not a file: "},
    {"status_update_empty_suffix", " update file is empty: "},
    {"status_update_start_prefix", "Starting "},
    {"status_update_start_suffix", " update from "},
    {"status_update_delete_failed", "Could not delete completed update file: "},
    {"status_check_sd_updates", "Checking SD card updates..."},
    {"status_sd_update_complete", "SD card update complete. Rebooting..."},
    {"err_incomplete_profile_entry", "Incomplete profile entry:\n"},
    {"err_entry", "\nEntry: "},
    {"err_required_profile_entry", "\nRequired: diffWeight, stepper, steps, rpm, measurements"},
    {"err_invalid_profile_values", "Invalid profile values:\n"},
    {"err_calibration_profile_incomplete", "Calibration profile is incomplete:\n"},
    {"err_required_calibration_profile", "\nRequired: stepper, steps, rpm"},
    {"err_profile_missing_map", "Profile has no trickleMap:\n"},
    {"err_profile_file_not_found", "Profile file not found:\n"},
    {"err_could_not_open_profile_file", "Could not open profile file:\n"},
    {"err_root_not_json", "Root is not a JSON object"},
    {"err_profile_json_parse_failed", "Profile JSON parse failed:\n"},
    {"err_profile_has_no_entries", "Profile has no entries:\n"},
    {"err_could_not_open_config_file", "Could not open config file:\n"},
    {"err_config_json_parse_failed", "Config JSON parse failed:\n"},
    {"err_could_not_write_profile_file", "Could not write profile file:\n"},
    {"err_could_not_replace_profile_file", "Could not replace profile file:\n"},
    {"msg_cannot_delete_calibrate_profile", "Cannot delete calibrate profile"},
    {"msg_delete_profile_file_not_found", "Profile file not found: "},
    {"msg_delete_profile_confirm_prefix", "Delete profile "},
    {"msg_delete_profile_confirm_suffix", "?"},
    {"msg_cannot_delete_profile", "Cannot delete profile"},
    {"msg_delete_profile_calibrate_missing", "Cannot delete profile: calibrate profile missing"},
    {"msg_delete_profile_calibrate_load_failed", "Cannot delete profile: failed to load calibrate"},
    {"msg_could_not_delete_profile", "Could not delete profile: "},
    {"msg_profile_deleted", "Profile deleted: "},
    {"msg_stop_trickler_before_tune_profile", "Stop trickler before tuning profile"},
    {"msg_cannot_tune_profile", "Cannot tune profile"},
    {"msg_tune_profile_title", "weight/rev"},
    {"msg_could_not_tune_profile", "Could not tune profile"},
    {"msg_profile_tuned", "Profile tuned: "},
};

static const char *languageFallback(const char *key)
{
  for (size_t i = 0; i < sizeof(LANG_FALLBACKS) / sizeof(LANG_FALLBACKS[0]); i++)
  {
    if (strcmp(key, LANG_FALLBACKS[i].key) == 0)
    {
      return LANG_FALLBACKS[i].value;
    }
  }
  return key;
}

JsonDocument activeUiLangDoc;

const char *langText(const char *key)
{
  if (strcmp(key, "msg_sd_card_not_connected") == 0)
  {
    return languageFallback(key);
  }

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

// Normalize config.language to a lowercase base code: trim, lowercase, and drop
// any region suffix ("en-US"/"en_US" -> "en"), defaulting to "en" when empty.
// Shared by the display (loadLanguage) and web (loadWebLang) language loaders.
String normalizedLanguageCode()
{
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
  return language;
}

bool loadLanguage()
{
  activeUiLangDoc.clear();

  String language = normalizedLanguageCode();
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

  // Parse straight from the file stream. ArduinoJson copies keys/values into the
  // document's own pool, so there is no need to keep the raw JSON text in heap.
  DeserializationError error = deserializeJson(activeUiLangDoc, file);
  file.close();
  if (error || !activeUiLangDoc["ui"].is<JsonObject>())
  {
    DEBUG_PRINT("Language JSON parse failed: ");
    DEBUG_PRINTLN(error.c_str());
    activeUiLangDoc.clear();
    return false;
  }

  DEBUG_PRINT("Loaded language: ");
  DEBUG_PRINT(config.language);
  DEBUG_PRINT(" from ");
  DEBUG_PRINTLN(filename);
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
      lv_label_set_text_static(ui_LabelToggleTrickler, UI_SYMBOL_START);
    }
    lv_label_set_text_static(ui_LabelMessageOk, UI_SYMBOL_OK);
    if (strcmp(lv_label_get_text(ui_LabelProfile), "Profile") == 0)
    {
      lv_label_set_text(ui_LabelProfile, langText("placeholder_profile"));
    }
    lvglUnlock();
  }

  updateFilesystemSyncControls();
  updateWifiTabIndicator(true);
}
