struct LanguageFallback
{
  const char *key;
  const char *text;
};

static const LanguageFallback languageFallbacks[] = {
    {"tab_trickler", "Trickler"},
    {"tab_profile", "Profile"},
    {"tab_info", "Info"},
    {"button_start", "Start"},
    {"button_stop", "Stop"},
    {"button_ok", "OK"},
    {"button_yes", "Yes"},
    {"button_no", "No"},
    {"placeholder_profile", "Profile"},
    {"status_ready", "Ready"},
    {"status_stopped", "Stopped"},
    {"status_get_weight", "Get Weight..."},
    {"status_running", "Running..."},
    {"status_done", "Done :)"},
    {"status_init_steppers", "Init steppers..."},
    {"status_mount_sd", "Mounting SD card..."},
    {"status_loading_config", "Loading config..."},
    {"status_reading_profiles", "Reading profiles..."},
    {"status_starting_scale", "Starting scale serial..."},
    {"status_saving_target", "Writing target to profile..."},
    {"status_target_saved", "Target saved"},
    {"status_search_profiles", "Search Profiles..."},
    {"status_refresh_profile_list", "Refreshing profile list..."},
    {"status_saving_target_failed", "Target weight save failed!"},
    {"status_timeout", "Timeout! Check RS232 Wiring & Settings!"},
    {"status_over_trickle", "!Over trickle!"},
    {"status_bulk_failed", "Bulk trickle failed!"},
    {"status_invalid_profile", "Invalid profile!"},
    {"status_invalid_stepper", "Invalid stepper number!"},
    {"status_reconnecting_wifi", "Reconnecting to WiFi..."},
    {"status_card_unknown", "CardType UNKNOWN"},
    {"msg_card_mount_failed", "Card Mount Failed!\n"},
    {"msg_no_sd_card", "No SD card attached!\n"},
    {"msg_config_default", "Default Config generated."},
    {"msg_over_trickle", "!Over trickle!\n!Check weight!"},
    {"msg_create_profile_prompt", "Create profile from calibration?\n\nWeight: "},
    {"msg_profile_created", "Profile created:\n\n"},
    {"msg_create_profile_failed", "Could not create profile!"},
    {"msg_calibration_weight_invalid", "Calibration weight invalid!"},
    {"msg_no_free_profile_name", "No free powder profile name!"},
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
    {"status_starting_trickler", "Starting trickler..."},
    {"status_profile_selected_suffix", " selected!"},
    {"status_connect_wifi", "Connect to WiFi: "},
    {"status_static_ip_failed", "Static IP Failed to configure!"},
    {"status_dns_failed", "DNS Failed to configure!"},
    {"status_wifi_connected", "WiFi Connected"},
    {"status_no_wifi", "No WiFi"},
    {"status_open_browser_prefix", "Open http://"},
    {"status_open_browser_suffix", ".local in your browser"},
    {"msg_connect_wifi_wait", "\n\nPlease wait..."},
    {"msg_new_firmware", "New firmware available:\n\nv"},
    {"msg_check_url", "\n\nCheck: https://robo-trickler.de"},
    {"status_fw_update_done", "FW Update done!"},
    {"status_update_completed", "Update successfully completed. Rebooting."},
    {"status_update_not_finished", "Update not finished? Something went wrong!"},
    {"status_update_failed", "Update failed: "},
    {"status_fw_update_begin_failed", "FW Update begin failed: "},
    {"status_update_not_file", "Error, update.bin is not a file"},
    {"status_update_start", "Try to start update"},
    {"status_update_empty", "Error, file is empty"},
    {"status_no_new_firmware", "No new firmware found"},
    {"status_check_fw_update", "Check for Firmware Update..."},
    {"status_update_upload", "Update: "},
    {"status_update_success", "Update Success: "},
    {"status_update_write_failed", "Update write failed: "},
    {"status_update_end_failed", "Update end failed: "},
    {"status_update_unexpected", "Update Failed Unexpectedly (likely broken connection)"},
};

static const char *languageFallback(const char *key)
{
  for (size_t i = 0; i < ARRAY_COUNT(languageFallbacks); i++)
  {
    if (strcmp(key, languageFallbacks[i].key) == 0)
    {
      return languageFallbacks[i].text;
    }
  }
  return key;
}

String activeUiLangJson = "";
String activeUiLangFilename = "";
JsonDocument activeUiLangDoc;

const char *languageText(const char *key)
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
  for (int i = 0; i < (int)ARRAY_COUNT(candidates); i++)
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
    lv_tabview_set_tab_text(ui_TabView, 0, languageText("tab_trickler"));
    lv_tabview_set_tab_text(ui_TabView, 1, languageText("tab_profile"));
    lv_tabview_set_tab_text(ui_TabView, 2, languageText("tab_info"));

    if (!running)
    {
      lv_label_set_text(ui_LabelTricklerStart, languageText("button_start"));
    }
    lv_label_set_text(ui_LabelButtonMessage, languageText("button_ok"));
    if (strcmp(lv_label_get_text(ui_LabelProfile), "Profile") == 0)
    {
      lv_label_set_text(ui_LabelProfile, languageText("placeholder_profile"));
    }
    lvglUnlock();
  }
}
