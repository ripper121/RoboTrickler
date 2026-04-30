static const char *languageFallback(const char *key)
{
  if (strcmp(key, "tab_trickler") == 0)
    return "Trickler";
  if (strcmp(key, "tab_profile") == 0)
    return "Profile";
  if (strcmp(key, "tab_info") == 0)
    return "Info";
  if (strcmp(key, "button_start") == 0)
    return "Start";
  if (strcmp(key, "button_stop") == 0)
    return "Stop";
  if (strcmp(key, "button_ok") == 0)
    return "OK";
  if (strcmp(key, "button_yes") == 0)
    return "Yes";
  if (strcmp(key, "button_no") == 0)
    return "No";
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
  if (strcmp(key, "status_done") == 0)
    return "Done :)";
  if (strcmp(key, "status_init_steppers") == 0)
    return "Init steppers...";
  if (strcmp(key, "status_mount_sd") == 0)
    return "Mounting SD card...";
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
  if (strcmp(key, "status_over_trickle") == 0)
    return "!Over trickle!";
  if (strcmp(key, "status_bulk_failed") == 0)
    return "Bulk trickle failed!";
  if (strcmp(key, "status_invalid_profile") == 0)
    return "Invalid profile!";
  if (strcmp(key, "status_invalid_stepper") == 0)
    return "Invalid stepper number!";
  if (strcmp(key, "status_reconnecting_wifi") == 0)
    return "Reconnecting to WiFi...";
  if (strcmp(key, "status_card_unknown") == 0)
    return "CardType UNKNOWN";
  if (strcmp(key, "msg_card_mount_failed") == 0)
    return "Card Mount Failed!\n";
  if (strcmp(key, "msg_no_sd_card") == 0)
    return "No SD card attached!\n";
  if (strcmp(key, "msg_config_default") == 0)
    return "Default Config generated.";
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
  if (strcmp(key, "msg_connect_wifi_wait") == 0)
    return "\n\nPlease wait...";
  if (strcmp(key, "msg_new_firmware") == 0)
    return "New firmware available:\n\nv";
  if (strcmp(key, "msg_check_url") == 0)
    return "\n\nCheck: https://robo-trickler.de";
  if (strcmp(key, "status_fw_update_done") == 0)
    return "FW Update done!";
  if (strcmp(key, "status_update_completed") == 0)
    return "Update successfully completed. Rebooting.";
  if (strcmp(key, "status_update_not_finished") == 0)
    return "Update not finished? Something went wrong!";
  if (strcmp(key, "status_update_failed") == 0)
    return "Update failed: ";
  if (strcmp(key, "status_fw_update_begin_failed") == 0)
    return "FW Update begin failed: ";
  if (strcmp(key, "status_update_not_file") == 0)
    return "Error, update.bin is not a file";
  if (strcmp(key, "status_update_start") == 0)
    return "Try to start update";
  if (strcmp(key, "status_update_empty") == 0)
    return "Error, file is empty";
  if (strcmp(key, "status_no_new_firmware") == 0)
    return "No new firmware found";
  if (strcmp(key, "status_check_fw_update") == 0)
    return "Check for Firmware Update...";
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
      "/system/lang/" + language + ".json",
      "/lang/en.json",
      "/system/lang/en.json"};

  String filename = "";
  for (int i = 0; i < (int)(sizeof(candidates) / sizeof(candidates[0])); i++)
  {
    if (SD.exists(candidates[i].c_str()))
    {
      filename = candidates[i];
      break;
    }
  }

  File file = SD.open(filename.c_str());
  if (!file)
  {
    DEBUG_PRINT("Language file not found: ");
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

    if (!running)
    {
      lv_label_set_text(ui_LabelTricklerStart, langText("button_start"));
    }
    lv_label_set_text(ui_LabelButtonMessage, langText("button_ok"));
    if (strcmp(lv_label_get_text(ui_LabelProfile), "Profile") == 0)
    {
      lv_label_set_text(ui_LabelProfile, langText("placeholder_profile"));
    }
    lvglUnlock();
  }
}
