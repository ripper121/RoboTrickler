static byte profileActuatorNumber(const char *actuatorName)
{
  if ((actuatorName == NULL) || (strcmp(actuatorName, "stepper1") == 0))
  {
    return 1;
  }
  if (strcmp(actuatorName, "stepper2") == 0)
  {
    return 2;
  }
  return 0;
}

static const char *profileActuatorName(byte stepperNumber)
{
  return stepperNumber == 2 ? "stepper2" : "stepper1";
}

static bool hasRequiredProfileFields(JsonObject profileEntry)
{
  return !profileEntry.isNull() &&
         !profileEntry["diffWeight"].isNull() &&
         !profileEntry["actuator"].isNull() &&
         !profileEntry["steps"].isNull() &&
         !profileEntry["speed"].isNull() &&
         !profileEntry["measurements"].isNull();
}

static bool hasCalibrationProfileFields(JsonObject profileEntry)
{
  return !profileEntry.isNull() &&
         !profileEntry["actuator"].isNull() &&
         !profileEntry["steps"].isNull() &&
         !profileEntry["speed"].isNull();
}

static bool isCalibrationProfileFile(const char *filename)
{
  String normalized = String(filename);
  normalized.replace("\\", "/");
  normalized.toLowerCase();
  return (normalized == "calibrate.txt") ||
         normalized.endsWith("/calibrate.txt");
}

static String sdFilenameError(const char *key, const char *filename)
{
  return String(langText(key)) + filename;
}

static String sdProfileEntryError(const char *key, const char *filename, int itemNumber)
{
  return String(langText(key)) + filename + langText("err_entry") + String(itemNumber);
}

static bool loadProfileEntry(JsonObject profileEntry, int itemNumber, const char *filename, Config &config, bool showErrors, bool allowCalibrationDefaults)
{
  // Calibration profiles can omit diffWeight and measurements; normal profiles
  // must provide the full map used by the automatic trickling loop.
  bool hasRequiredFields = hasRequiredProfileFields(profileEntry);
  if (!hasRequiredFields && (!allowCalibrationDefaults || !hasCalibrationProfileFields(profileEntry)))
  {
    if (showErrors)
    {
      setSdReadError(sdProfileEntryError("err_incomplete_profile_entry", filename, itemNumber) + langText("err_required_profile_entry"));
      DEBUG_PRINT("Incomplete profile entry: ");
      DEBUG_PRINTLN(itemNumber);
    }
    return false;
  }

  byte stepperNumber = profileActuatorNumber(profileEntry["actuator"] | "stepper1");
  int stepperSpeed = profileEntry["speed"] | 200;
  int measurements = profileEntry["measurements"] | 5;
  float profileWeight = profileEntry["diffWeight"] | 0.0;
  long profileSteps = profileEntry["steps"] | 0;
  if ((stepperNumber < 1) || (stepperNumber > 2) || (stepperSpeed <= 0) || (measurements < 0) || (profileWeight < 0) || (profileSteps <= 0))
  {
    if (showErrors)
    {
      setSdReadError(sdProfileEntryError("err_invalid_profile_values", filename, itemNumber));
      DEBUG_PRINT("Invalid profile values in entry: ");
      DEBUG_PRINTLN(itemNumber);
    }
    return false;
  }

  if (config.profile_count >= 16)
  {
    return true;
  }

  int item_key = config.profile_count;
  config.profile_num[item_key] = stepperNumber;
  config.profile_weight[item_key] = profileWeight;
  config.profile_steps[item_key] = profileSteps;
  config.profile_speed[item_key] = stepperSpeed;
  config.profile_measurements[item_key] = measurements;
  config.profile_count++;
  return true;
}

static bool loadProfileEntries(JsonObject doc, const char *filename, Config &config, bool showErrors)
{
  bool allowCalibrationDefaults = isCalibrationProfileFile(filename);
  JsonArray rs232TrickleMap = doc["rs232TrickleMap"].as<JsonArray>();
  if (!rs232TrickleMap.isNull())
  {
    int itemNumber = 1;
    for (JsonVariant item : rs232TrickleMap)
    {
      if (!loadProfileEntry(item.as<JsonObject>(), itemNumber, filename, config, showErrors, allowCalibrationDefaults))
      {
        return false;
      }
      itemNumber++;
    }
    return config.profile_count > 0;
  }

  if (allowCalibrationDefaults)
  {
    if (hasCalibrationProfileFields(doc))
    {
      return loadProfileEntry(doc, 1, filename, config, showErrors, true);
    }

    if (showErrors)
    {
      setSdReadError(sdFilenameError("err_calibration_profile_incomplete", filename) + langText("err_required_calibration_profile"));
      DEBUG_PRINTLN("Calibration profile is incomplete");
    }
    return false;
  }

  if (showErrors)
  {
    setSdReadError(sdFilenameError("err_profile_missing_map", filename));
    DEBUG_PRINTLN("Profile has no rs232TrickleMap");
  }
  return false;
}

String profileFilename(const char *profileName)
{
  const char *directories[] = {"/profiles/"};

  for (int dirIndex = 0; dirIndex < (int)(sizeof(directories) / sizeof(directories[0])); dirIndex++)
  {
    String candidate = String(directories[dirIndex]) + String(profileName) + ".txt";
    if (ACTIVE_FS.exists(candidate.c_str()))
    {
      return candidate;
    }
  }

  return "/profiles/" + String(profileName) + ".txt";
}

String sdReadError = "";

void setSdReadError(const String &message)
{
  sdReadError = message;
}

String getSdReadError()
{
  return sdReadError;
}

void setDefaultConfiguration(Config &config)
{
  config.wifi_enabled = true;
  strlcpy(config.wifi_ssid, "", sizeof(config.wifi_ssid));
  strlcpy(config.wifi_psk, "", sizeof(config.wifi_psk));
  strlcpy(config.IPStatic, "", sizeof(config.IPStatic));
  strlcpy(config.IPGateway, "", sizeof(config.IPGateway));
  strlcpy(config.IPSubnet, "", sizeof(config.IPSubnet));
  strlcpy(config.IPDns, "", sizeof(config.IPDns));
  strlcpy(config.scale_protocol, "GG", sizeof(config.scale_protocol));
  strlcpy(config.scale_customCode, "", sizeof(config.scale_customCode));
  config.scale_baud = 9600;
  strlcpy(config.profile, "calibrate", sizeof(config.profile));
  config.targetWeight = 40.0;
  strlcpy(config.beeper, "done", sizeof(config.beeper));
  strlcpy(config.language, "en", sizeof(config.language));
  config.fwCheck = true;
  config.trickleCounter = false;
  config.trickleCount = 0;
  config.profile_startAtZero = false;
  config.profile_trickleCounter = false;
}

bool readProfile(const char *filename, Config &config)
{
  setSdReadError("");
  String infoText = String(langText("status_loading_profile")) + filename;
  updateDisplayLog(infoText, true);

  if (!ACTIVE_FS.exists(filename))
  {
    setSdReadError(sdFilenameError("err_profile_file_not_found", filename));
    DEBUG_PRINTLN("Profile not found:");
    DEBUG_PRINTLN(filename);
    return false;
  }

  // Dump config file
  printFile(filename);

  // Open file for reading
  File file = ACTIVE_FS.open(filename);
  if (!file)
  {
    setSdReadError(sdFilenameError("err_could_not_open_profile_file", filename));
    DEBUG_PRINT("Failed to open profile: ");
    DEBUG_PRINTLN(filename);
    return false;
  }

  JsonDocument doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, file);

  if (error || !doc.is<JsonObject>())
  {
    String errorText = error ? String(error.c_str()) : String(langText("err_root_not_json"));
    setSdReadError(sdFilenameError("err_profile_json_parse_failed", filename) + "\n" + errorText);
    DEBUG_PRINT("deserializeJson() failed on : ");
    DEBUG_PRINT(filename);
    DEBUG_PRINT(" /");
    DEBUG_PRINTLN(errorText);
    file.close();
    return false;
  }

  // Reset profile-only fields before applying the selected JSON file so values
  // from the previous profile cannot leak into a partially specified profile.
  config.profile_tolerance = 0.000;
  config.profile_alarmThreshold = 0.000;
  config.profile_weightGap = 1.000;
  config.profile_bulkActuator = 1;
  config.profile_generalMeasurements = 20;
  config.profile_startAtZero = false;
  config.profile_trickleCounter = false;
  config.profile_count = 0;
  for (int i = 0; i < 3; i++)
  {
    config.profile_stepperEnabled[i] = false;
    config.profile_stepperUnitsPerThrow[i] = 0.0;
    config.profile_stepperUnitsPerThrowSpeed[i] = 0;
  }
  for (int i = 0; i < 16; i++)
  {
    config.profile_num[i] = 1;
    config.profile_weight[i] = 0;
    config.profile_steps[i] = 0;
    config.profile_speed[i] = 0;
    config.profile_measurements[i] = 0;
  }

  JsonObject general = doc["general"].as<JsonObject>();
  bool hasGeneralMeasurements = false;
  if (!general.isNull())
  {
    config.targetWeight = general["targetWeight"] | config.targetWeight;
    config.profile_tolerance = general["tolerance"] | 0.000;
    config.profile_alarmThreshold = general["alarmThreshold"] | 0.000;
    config.profile_weightGap = general["weightGap"] | 1.000;
    config.profile_bulkActuator = profileActuatorNumber(general["actuator"] | "stepper1");
    config.profile_startAtZero = general["startAtZero"] | false;
    config.profile_trickleCounter = general["trickleCounter"] | false;
    if (config.profile_bulkActuator == 0)
    {
      config.profile_bulkActuator = 1;
    }
    hasGeneralMeasurements = !general["measurements"].isNull();
    config.profile_generalMeasurements = general["measurements"] | 20;
    if (config.profile_generalMeasurements < 0)
    {
      config.profile_generalMeasurements = 20;
    }
  }

  JsonObject actuator = doc["actuator"].as<JsonObject>();
  if (!actuator.isNull())
  {
    for (int stepperNumber = 1; stepperNumber <= 2; stepperNumber++)
    {
      JsonObject stepper = actuator[profileActuatorName(stepperNumber)].as<JsonObject>();
      if (!stepper.isNull())
      {
        config.profile_stepperEnabled[stepperNumber] = stepper["enabled"] | true;
        config.profile_stepperUnitsPerThrow[stepperNumber] = stepper["unitsPerThrow"] | 0.0;
        config.profile_stepperUnitsPerThrowSpeed[stepperNumber] = stepper["unitsPerThrowSpeed"] | 0;
      }
    }
  }

  if (!loadProfileEntries(doc.as<JsonObject>(), filename, config, true))
  {
    if (getSdReadError().length() <= 0)
    {
      setSdReadError(sdFilenameError("err_profile_has_no_entries", filename));
      DEBUG_PRINTLN("Profile has no entries");
    }
    file.close();
    return false;
  }
  if (!hasGeneralMeasurements && isCalibrationProfileFile(filename) && (config.profile_count > 0))
  {
    config.profile_generalMeasurements = config.profile_measurements[0];
  }
  
  file.close();

  infoText = String(langText("status_profile_ready")) + config.profile;
  updateDisplayLog(infoText, true);
  return true;
}

// Loads the configuration from a file
bool loadConfiguration(const char *filename, Config &config)
{
  setSdReadError("");
  setDefaultConfiguration(config);
  // Dump config file
  printFile(filename);

  // Open file for reading
  File file = ACTIVE_FS.open(filename);
  if (!file)
  {
    setSdReadError(sdFilenameError("err_could_not_open_config_file", filename));
    DEBUG_PRINT("Failed to open config: ");
    DEBUG_PRINTLN(filename);
    return 0;
  }

  JsonDocument doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, file);

  if (error || !doc.is<JsonObject>())
  {
    String errorText = error ? String(error.c_str()) : String(langText("err_root_not_json"));
    setSdReadError(sdFilenameError("err_config_json_parse_failed", filename) + "\n" + errorText);
    DEBUG_PRINT("deserializeJson() failed on config.txt: ");
    DEBUG_PRINTLN(errorText);
    file.close();
    return 0;
  }

  config.wifi_enabled = doc["wifi"]["enabled"] | config.wifi_enabled;
  strlcpy(config.wifi_ssid, doc["wifi"]["ssid"] | config.wifi_ssid, sizeof(config.wifi_ssid));
  strlcpy(config.wifi_psk, doc["wifi"]["psk"] | config.wifi_psk, sizeof(config.wifi_psk));
  strlcpy(config.IPStatic, doc["wifi"]["IPStatic"] | config.IPStatic, sizeof(config.IPStatic));
  strlcpy(config.IPGateway, doc["wifi"]["IPGateway"] | config.IPGateway, sizeof(config.IPGateway));
  strlcpy(config.IPSubnet, doc["wifi"]["IPSubnet"] | config.IPSubnet, sizeof(config.IPSubnet));
  strlcpy(config.IPDns, doc["wifi"]["IPDNS"] | config.IPDns, sizeof(config.IPDns));
  strlcpy(config.scale_protocol, doc["scale"]["protocol"] | config.scale_protocol, sizeof(config.scale_protocol));
  strlcpy(config.scale_customCode, doc["scale"]["customCode"] | config.scale_customCode, sizeof(config.scale_customCode));
  config.scale_baud = doc["scale"]["baud"] | config.scale_baud;
  strlcpy(config.profile, doc["profile"] | config.profile, sizeof(config.profile));
  strlcpy(config.beeper, doc["beeper"] | config.beeper, sizeof(config.beeper));
  strlcpy(config.language, doc["language"] | config.language, sizeof(config.language));
  config.trickleCounter = doc["trickleCounter"] | config.trickleCounter;
  config.trickleCount = doc["trickleCount"] | config.trickleCount;
  if (config.trickleCount < 0)
  {
    config.trickleCount = 0;
  }

  JsonObject fwUpdate = doc["fw_update"].as<JsonObject>();
  // fw_update.url is intentionally firmware-only; only the check flag is configurable.
  config.fwCheck = fwUpdate["check"] | config.fwCheck;

  file.close();

  return 1;
}

bool saveProfileTargetWeight(const char *profileName, float targetWeight)
{
  String filename = profileFilename(profileName);
  File file = ACTIVE_FS.open(filename.c_str());
  if (!file)
  {
    setSdReadError(sdFilenameError("err_could_not_open_profile_file", filename.c_str()));
    DEBUG_PRINT("Failed to open profile: ");
    DEBUG_PRINTLN(filename);
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error || !doc.is<JsonObject>())
  {
    String errorText = error ? String(error.c_str()) : String(langText("err_root_not_json"));
    setSdReadError(sdFilenameError("err_profile_json_parse_failed", filename.c_str()) + "\n" + errorText);
    DEBUG_PRINT("deserializeJson() failed on profile target save: ");
    DEBUG_PRINTLN(errorText);
    return false;
  }

  JsonObject general = doc["general"].as<JsonObject>();
  if (general.isNull())
  {
    general = doc["general"].to<JsonObject>();
  }
  general["targetWeight"] = serialized(String(targetWeight, 3));

  String tempFilename = filename + ".tmp";
  ACTIVE_FS.remove(tempFilename.c_str());
  file = ACTIVE_FS.open(tempFilename.c_str(), FILE_WRITE);
  if (!file)
  {
    setSdReadError(sdFilenameError("err_could_not_write_profile_file", tempFilename.c_str()));
    DEBUG_PRINT("Failed to create profile temp file: ");
    DEBUG_PRINTLN(tempFilename);
    return false;
  }

  bool written = serializeJsonPretty(doc, file) > 0;
  file.close();
  if (!written)
  {
    ACTIVE_FS.remove(tempFilename.c_str());
    setSdReadError(sdFilenameError("err_could_not_write_profile_file", filename.c_str()));
    DEBUG_PRINTLN("Failed to write profile target weight");
    return false;
  }

  ACTIVE_FS.remove(filename.c_str());
  if (!ACTIVE_FS.rename(tempFilename.c_str(), filename.c_str()))
  {
    ACTIVE_FS.remove(tempFilename.c_str());
    setSdReadError(sdFilenameError("err_could_not_replace_profile_file", filename.c_str()));
    DEBUG_PRINT("Failed to replace profile file: ");
    DEBUG_PRINTLN(filename);
    return false;
  }

  return true;
}

static bool writeProfileDocument(const String &filename, JsonDocument &doc)
{
  String tempFilename = filename + ".tmp";
  ACTIVE_FS.remove(tempFilename.c_str());
  File file = ACTIVE_FS.open(tempFilename.c_str(), FILE_WRITE);
  if (!file)
  {
    setSdReadError(sdFilenameError("err_could_not_write_profile_file", tempFilename.c_str()));
    DEBUG_PRINT("Failed to create profile temp file: ");
    DEBUG_PRINTLN(tempFilename);
    return false;
  }

  bool written = serializeJsonPretty(doc, file) > 0;
  file.close();
  if (!written)
  {
    ACTIVE_FS.remove(tempFilename.c_str());
    setSdReadError(sdFilenameError("err_could_not_write_profile_file", filename.c_str()));
    DEBUG_PRINTLN("Failed to write profile file");
    return false;
  }

  ACTIVE_FS.remove(filename.c_str());
  if (!ACTIVE_FS.rename(tempFilename.c_str(), filename.c_str()))
  {
    ACTIVE_FS.remove(tempFilename.c_str());
    setSdReadError(sdFilenameError("err_could_not_replace_profile_file", filename.c_str()));
    DEBUG_PRINT("Failed to replace profile file: ");
    DEBUG_PRINTLN(filename);
    return false;
  }

  return true;
}

bool isValidProfileFile(const char *filename)
{
  File file = ACTIVE_FS.open(filename);
  if (!file)
  {
    return false;
  }
  if (file.isDirectory())
  {
    file.close();
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error || !doc.is<JsonObject>())
  {
    return false;
  }

  Config validationConfig = config;
  validationConfig.profile_count = 0;
  return loadProfileEntries(doc.as<JsonObject>(), filename, validationConfig, false);
}

String nextCalibrationProfileName()
{
  for (int i = 0; i <= 999; i++)
  {
    char profileName[16];
    snprintf(profileName, sizeof(profileName), "powder_%03d", i);
    String filename = "/profiles/" + String(profileName) + ".txt";
    if (!ACTIVE_FS.exists(filename.c_str()))
    {
      return String(profileName);
    }
  }

  return "";
}

static void populateCalibrationTrickleMap(JsonDocument &doc, float unitsPerThrow, int profileSpeed)
{
  const float diffWeights[8] = {1.929, 0.965, 0.482, 0.241, 0.121, 0.060, 0.030, 0.000};
  const size_t diffWeightsCount = sizeof(diffWeights) / sizeof(diffWeights[0]);
  const int measurements[8] = {2, 2, 5, 5, 10, 10, 15, 20};
  const float rs232LimitFactor = 0.65;

  JsonArray rs232TrickleMap = doc["rs232TrickleMap"].to<JsonArray>();
  for (int i = 0; i < diffWeightsCount; i++)
  {
    long steps = lround(((diffWeights[i] * (double)MOTOR_REV_STEPS) / unitsPerThrow) * rs232LimitFactor);
    if (steps < 5)
    {
      steps = 5;
    }

    JsonObject profileEntry = rs232TrickleMap.add<JsonObject>();
    profileEntry["diffWeight"] = serialized(String(diffWeights[i], 3));
    profileEntry["actuator"] = "stepper1";
    profileEntry["steps"] = steps;
    profileEntry["speed"] = profileSpeed;
    profileEntry["measurements"] = measurements[i];
  }
}


bool createProfileFromCalibration(float calibrationWeight, String &profileName)
{
  if (calibrationWeight <= 0.0)
  {
    updateDisplayLog(langText("msg_calibration_weight_invalid"), true);
    return false;
  }

  profileName = nextCalibrationProfileName();
  if (profileName.length() <= 0)
  {
    updateDisplayLog(langText("msg_no_free_profile_name"), true);
    return false;
  }

  String infoText = String(langText("status_creating_profile")) + profileName;
  updateDisplayLog(infoText, true);

  const float unitsPerThrow = calibrationWeight / 100.0;
  int profileSpeed = config.profile_speed[0];
  if (profileSpeed <= 0)
  {
    profileSpeed = 200;
  }

  JsonDocument doc;
  JsonObject general = doc["general"].to<JsonObject>();
  general["targetWeight"] = serialized(String(config.targetWeight, 3));
  general["tolerance"] = serialized(String(0.0, 3));
  general["alarmThreshold"] = serialized(String(1.0, 3));
  general["weightGap"] = serialized(String(1.0, 3));
  general["actuator"] = "stepper1";
  general["startAtZero"] = false;
  general["trickleCounter"] = false;
  general["measurements"] = config.profile_generalMeasurements > 0 ? config.profile_generalMeasurements : 20;

  JsonObject actuator = doc["actuator"].to<JsonObject>();
  JsonObject stepper1 = actuator["stepper1"].to<JsonObject>();
  stepper1["enabled"] = true;
  stepper1["unitsPerThrow"] = serialized(String(unitsPerThrow, 3));
  stepper1["unitsPerThrowSpeed"] = profileSpeed;
  JsonObject stepper2 = actuator["stepper2"].to<JsonObject>();
  stepper2["enabled"] = config.profile_stepperEnabled[2];
  stepper2["unitsPerThrow"] = serialized(String(config.profile_stepperUnitsPerThrow[2] > 0.0 ? config.profile_stepperUnitsPerThrow[2] : 10.0, 3));
  stepper2["unitsPerThrowSpeed"] = config.profile_stepperUnitsPerThrowSpeed[2] > 0 ? config.profile_stepperUnitsPerThrowSpeed[2] : 200;

  populateCalibrationTrickleMap(doc, unitsPerThrow, profileSpeed);

  if (!ACTIVE_FS.exists("/profiles") && !ACTIVE_FS.mkdir("/profiles"))
  {
    updateDisplayLog(langText("msg_profiles_folder_failed"), true);
    return false;
  }

  String filename = "/profiles/" + profileName + ".txt";
  infoText = String(langText("status_writing_profile")) + profileName;
  updateDisplayLog(infoText, true);
  File file = ACTIVE_FS.open(filename.c_str(), FILE_WRITE);
  if (!file)
  {
    updateDisplayLog(langText("msg_profile_file_create_failed"), true);
    return false;
  }

  bool written = serializeJsonPretty(doc, file) > 0;
  file.close();
  if (!written)
  {
    ACTIVE_FS.remove(filename.c_str());
    updateDisplayLog(langText("msg_profile_file_write_failed"), true);
    return false;
  }

  infoText = langText("status_refresh_profile_list");
  updateDisplayLog(infoText, true);
  getProfileList();
  for (int i = 0; i < profileListCount; i++)
  {
    if (strcmp(profileListBuff[i], profileName.c_str()) == 0)
    {
      setProfile(i);
      break;
    }
  }

  updateDisplayLog(String(langText("status_profile_created")) + profileName, true);
  return true;
}

bool tuneProfileUnitsPerThrow(const char *profileName, float unitsPerThrow)
{
  if (unitsPerThrow <= 0.0)
  {
    updateDisplayLog(langText("msg_calibration_weight_invalid"), true);
    return false;
  }

  String filename = profileFilename(profileName);
  File file = ACTIVE_FS.open(filename.c_str());
  if (!file)
  {
    setSdReadError(sdFilenameError("err_could_not_open_profile_file", filename.c_str()));
    DEBUG_PRINT("Failed to open profile: ");
    DEBUG_PRINTLN(filename);
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error || !doc.is<JsonObject>())
  {
    String errorText = error ? String(error.c_str()) : String(langText("err_root_not_json"));
    setSdReadError(sdFilenameError("err_profile_json_parse_failed", filename.c_str()) + "\n" + errorText);
    DEBUG_PRINT("deserializeJson() failed on profile tune: ");
    DEBUG_PRINTLN(errorText);
    return false;
  }

  int profileSpeed = config.profile_stepperUnitsPerThrowSpeed[1];
  if (profileSpeed <= 0)
  {
    profileSpeed = config.profile_speed[0];
  }
  if (profileSpeed <= 0)
  {
    profileSpeed = 200;
  }

  JsonObject actuator = doc["actuator"].as<JsonObject>();
  if (actuator.isNull())
  {
    actuator = doc["actuator"].to<JsonObject>();
  }
  JsonObject stepper1 = actuator["stepper1"].as<JsonObject>();
  if (stepper1.isNull())
  {
    stepper1 = actuator["stepper1"].to<JsonObject>();
  }
  stepper1["enabled"] = true;
  stepper1["unitsPerThrow"] = serialized(String(unitsPerThrow, 3));
  stepper1["unitsPerThrowSpeed"] = profileSpeed;

  populateCalibrationTrickleMap(doc, unitsPerThrow, profileSpeed);

  if (!writeProfileDocument(filename, doc))
  {
    return false;
  }

  return true;
}

void scanProfileDirectory(const char *directory, byte &profileCounter, byte &invalidProfileCounter, String &invalidProfiles)
{
  File root = ACTIVE_FS.open(directory);
  if (!root)
  {
    DEBUG_PRINTLN("Failed to open directory");
    return;
  }
  if (!root.isDirectory())
  {
    DEBUG_PRINTLN("Not a directory");
    root.close();
    return;
  }

  File file = root.openNextFile();

  while (file)
  {
    if (profileCounter > 31)
    {
      file.close();
      break;
    }
    if (file.isDirectory())
    {
      DEBUG_PRINT("  DIR : ");
      DEBUG_PRINTLN(file.name());
    }
    else
    {
      String fileName = String(file.name());
      int slashIndex = fileName.lastIndexOf('/');
      if (slashIndex >= 0)
      {
        fileName = fileName.substring(slashIndex + 1);
      }
      bool isProfileCandidate = fileName.endsWith(".txt") && !fileName.endsWith("config.txt") && (fileName.indexOf(".cor") == -1);
      if (isProfileCandidate && isValidProfileFile(file.path()))
      {
        String filename = fileName;
        filename.replace(".txt", "");
        bool duplicate = false;
        for (int i = 0; i < profileCounter; i++)
        {
          if (strcmp(profileListBuff[i], filename.c_str()) == 0)
          {
            duplicate = true;
            break;
          }
        }
        if (!duplicate)
        {
          strlcpy(profileListBuff[profileCounter], filename.c_str(), PROFILE_NAME_LEN);
          profileCounter++;
        }
      }
      else if (isProfileCandidate)
      {
        updateDisplayLog(String(langText("status_invalid_profile_ignored")) + fileName);
        invalidProfileCounter++;
        if (invalidProfileCounter <= 4)
        {
          invalidProfiles += "\n";
          invalidProfiles += fileName;
        }
      }

      DEBUG_PRINT("  FILE: ");
      DEBUG_PRINT(file.name());
      DEBUG_PRINT("  SIZE: ");
      DEBUG_PRINTLN(file.size());
    }
    file.close();
    file = root.openNextFile();
  }

  root.close();
}

void getProfileList()
{
  profileListCount = 0;

  byte profileCounter = 0;
  byte invalidProfileCounter = 0;
  String invalidProfiles = "";

  if (ACTIVE_FS.exists("/profiles"))
  {
    scanProfileDirectory("/profiles", profileCounter, invalidProfileCounter, invalidProfiles);
  }

  profileListCount = profileCounter;
  DEBUG_PRINT("  profileListCount: ");
  DEBUG_PRINTLN(profileListCount);

  updateDisplayLog(String(langText("status_search_profiles")) + " " + String(profileListCount) + " / 30");

  for (int i = 0; i < profileListCount; i++)
  {
    DEBUG_PRINTLN(profileListBuff[i]);
  }
  if (invalidProfileCounter > 0)
  {
    String message = langText("msg_invalid_profiles_found");
    message += invalidProfiles;
    if (invalidProfileCounter > 4)
    {
      message += "\n...";
    }
    message += langText("msg_invalid_profiles_ignored");
    errorBox(message, true);
  }
}

void saveConfiguration(const char *filename, const Config &config)
{
  // Delete existing file, otherwise the configuration is appended to the file
  ACTIVE_FS.remove(filename);

  // Open file for writing
  File file = ACTIVE_FS.open(filename, FILE_WRITE);
  if (!file)
  {
    Serial.println(F("Failed to create file"));
    return;
  }

  JsonDocument doc;
  doc["wifi"]["enabled"] = config.wifi_enabled;
  doc["wifi"]["ssid"] = config.wifi_ssid;
  doc["wifi"]["psk"] = config.wifi_psk;
  doc["wifi"]["IPStatic"] = config.IPStatic;
  doc["wifi"]["IPGateway"] = config.IPGateway;
  doc["wifi"]["IPSubnet"] = config.IPSubnet;
  doc["wifi"]["IPDNS"] = config.IPDns;
  doc["scale"]["protocol"] = config.scale_protocol;
  doc["scale"]["customCode"] = config.scale_customCode;
  doc["scale"]["baud"] = config.scale_baud;
  doc["profile"] = config.profile;
  doc["beeper"] = config.beeper;
  doc["language"] = config.language;
  doc["trickleCounter"] = config.trickleCounter;
  doc["trickleCount"] = config.trickleCount;
  doc["fw_update"]["check"] = config.fwCheck;

  // Serialize JSON to file
  if (serializeJsonPretty(doc, file) == 0)
  {
    DEBUG_PRINTLN("Failed to write to file");
  }

  // Close the file
  file.close();
}

// Prints the content of a file to the Serial
void printFile(const char *filename)
{
#if DEBUG
  // Open file for reading
  File file = ACTIVE_FS.open(filename);

  DEBUG_PRINTLN(filename);

  if (!file)
  {
    DEBUG_PRINTLN("Failed to read file");
    return;
  }

  // Extract each characters by one by one
  while (file.available())
  {
    DEBUG_PRINT((char)file.read());
  }
  DEBUG_PRINTLN();

  // Close the file
  file.close();
#else
  (void)filename;
#endif
}
