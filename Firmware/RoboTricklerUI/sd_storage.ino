static byte profileStepperNumber(int stepperId)
{
  if (stepperId == 1)
  {
    return 1;
  }
  if (stepperId == 2)
  {
    return 2;
  }
  return 0;
}

// JSON object keys are always strings, so the stepper map is keyed by "1"/"2".
static const char *profileStepperName(byte stepperNumber)
{
  return stepperNumber == 2 ? "2" : "1";
}

static bool hasRequiredProfileFields(JsonObject profileEntry)
{
  JsonObject stepper = profileEntry["stepper"].as<JsonObject>();
  return !profileEntry.isNull() &&
         !profileEntry["diffWeight"].isNull() &&
         !stepper.isNull() &&
         !stepper["id"].isNull() &&
         !stepper["steps"].isNull() &&
         !stepper["rpm"].isNull() &&
         !profileEntry["measurements"].isNull();
}

static bool hasCalibrationProfileFields(JsonObject profileEntry)
{
  // The calibration profile is the one special case that expresses its throw in
  // motor revolutions (hardware-independent) instead of raw steps; steps are
  // derived from config.motorStepsPerRev when the entry is loaded.
  JsonObject stepper = profileEntry["stepper"].as<JsonObject>();
  return !profileEntry.isNull() &&
         !stepper.isNull() &&
         !stepper["id"].isNull() &&
         !stepper["revolutions"].isNull() &&
         !stepper["rpm"].isNull();
}

static bool isCalibrationProfileFile(const char *filename)
{
  String normalized = String(filename);
  normalized.replace("\\", "/");
  normalized.toLowerCase();
  return (normalized == CALIBRATE_PROFILE_NAME ".txt") ||
         normalized.endsWith("/" CALIBRATE_PROFILE_NAME ".txt");
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

  JsonObject stepper = profileEntry["stepper"].as<JsonObject>();
  byte stepperNumber = profileStepperNumber(stepper["id"] | 1);
  int stepperRpm = stepper["rpm"] | 200;
  int measurements = profileEntry["measurements"] | 5;
  float profileWeight = profileEntry["diffWeight"] | 0.0;
  // The calibration profile is special: it stores its throw as motor
  // revolutions, which we convert to steps using the configured steps-per-rev.
  // Normal map entries continue to carry raw steps.
  bool calibrationEntry = allowCalibrationDefaults && !hasRequiredFields;
  long profileSteps;
  if (calibrationEntry)
  {
    float profileRevolutions = stepper["revolutions"] | 0.0;
    profileSteps = lround((double)profileRevolutions * (double)config.motorStepsPerRev);
  }
  else
  {
    profileSteps = stepper["steps"] | 0;
  }
  if ((stepperNumber < 1) || (stepperNumber > 2) || (stepperRpm <= 0) || (measurements < 0) || (profileWeight < 0) || (profileSteps <= 0))
  {
    if (showErrors)
    {
      setSdReadError(sdProfileEntryError("err_invalid_profile_values", filename, itemNumber));
      DEBUG_PRINT("Invalid profile values in entry: ");
      DEBUG_PRINTLN(itemNumber);
    }
    return false;
  }

  if (config.profileEntryCount >= PROFILE_MAX_ENTRIES)
  {
    return true;
  }

  int entryIndex = config.profileEntryCount;
  config.profileStepper[entryIndex] = stepperNumber;
  config.profileDiffWeight[entryIndex] = profileWeight;
  config.profileSteps[entryIndex] = profileSteps;
  config.profileRpm[entryIndex] = stepperRpm;
  config.profileMeasurements[entryIndex] = measurements;
  config.profileEntryCount++;
  return true;
}

static bool loadProfileEntries(JsonObject doc, const char *filename, Config &config, bool showErrors)
{
  bool allowCalibrationDefaults = isCalibrationProfileFile(filename);
  JsonArray trickleMap = doc["trickleMap"].as<JsonArray>();
  if (!trickleMap.isNull())
  {
    int itemNumber = 1;
    for (JsonVariant item : trickleMap)
    {
      if (!loadProfileEntry(item.as<JsonObject>(), itemNumber, filename, config, showErrors, allowCalibrationDefaults))
      {
        return false;
      }
      itemNumber++;
    }
    return config.profileEntryCount > 0;
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
    DEBUG_PRINTLN("Profile has no trickleMap");
  }
  return false;
}

String profileFilename(const char *profileName)
{
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
  config.wifiEnabled = true;
  strlcpy(config.wifiSsid, "", sizeof(config.wifiSsid));
  strlcpy(config.wifiPsk, "", sizeof(config.wifiPsk));
  strlcpy(config.wifiIpStatic, "", sizeof(config.wifiIpStatic));
  strlcpy(config.wifiIpGateway, "", sizeof(config.wifiIpGateway));
  strlcpy(config.wifiIpSubnet, "", sizeof(config.wifiIpSubnet));
  strlcpy(config.wifiIpDns, "", sizeof(config.wifiIpDns));
  strlcpy(config.scaleProtocol, "GG", sizeof(config.scaleProtocol));
  strlcpy(config.scaleCustomCode, "", sizeof(config.scaleCustomCode));
  config.scaleBaud = 9600;
  config.motorStepsPerRev = DEFAULT_MOTOR_STEPS_PER_REV;
  strlcpy(config.profileName, CALIBRATE_PROFILE_NAME, sizeof(config.profileName));
  config.targetWeight = 40.0;
  strlcpy(config.beeper, "done", sizeof(config.beeper));
  strlcpy(config.language, "en", sizeof(config.language));
  config.fwUpdateCheck = true;
  config.totalCounterEnable = false;
  config.totalCount = 0;
  config.profileStartAtZero = false;
  config.profileSessionCounter = false;
}

// Recreate /profiles/calibrate.txt from the built-in template. The calibration
// profile is a fixed firmware concept that must always be available, so if the
// file is lost or corrupt we regenerate it instead of boot-looping. The fields
// mirror the shipped SD-Files/profiles/calibrate.txt. Returns true once the
// default file is written to disk.
bool writeDefaultCalibrateProfile()
{
  if (!ACTIVE_FS.exists("/profiles") && !ACTIVE_FS.mkdir("/profiles"))
  {
    updateDisplayLog(langText("msg_profiles_folder_failed"), true);
    return false;
  }

  JsonDocument doc;
  doc["measurements"] = 10;
  JsonObject stepper = doc["stepper"].to<JsonObject>();
  stepper["id"] = 1;
  stepper["revolutions"] = 100;
  stepper["rpm"] = 200;

  const char *filename = "/profiles/calibrate.txt";
  ACTIVE_FS.remove(filename);
  File file = ACTIVE_FS.open(filename, FILE_WRITE);
  if (!file)
  {
    updateDisplayLog(langText("msg_profile_file_create_failed"), true);
    return false;
  }

  bool written = serializeJsonPretty(doc, file) > 0;
  file.close();
  if (!written)
  {
    ACTIVE_FS.remove(filename);
    updateDisplayLog(langText("msg_profile_file_write_failed"), true);
    return false;
  }

  DEBUG_PRINTLN("Wrote default calibration profile");
  return true;
}

// Load the calibration profile into config, regenerating its file from the
// built-in template first if the existing one is missing or corrupt. Returns
// true only when config ends up holding a valid calibration profile.
bool ensureCalibrateProfile(Config &config)
{
  if (loadProfile(profileFilename(CALIBRATE_PROFILE_NAME).c_str(), config))
  {
    return true;
  }

  DEBUG_PRINTLN("Calibration profile unusable; rewriting default");
  if (!writeDefaultCalibrateProfile())
  {
    return false;
  }
  refreshProfileList();
  return loadProfile(profileFilename(CALIBRATE_PROFILE_NAME).c_str(), config);
}

bool loadProfile(const char *filename, Config &config)
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
  config.targetWeight = 40.0;
  config.profileTolerance = 0.000;
  config.profileAlarmThreshold = 0.000;
  config.profileWeightGap = 1.000;
  config.profileBulkStepper = 1;
  config.profileGeneralMeasurements = 20;
  config.profileStartAtZero = false;
  config.profileSessionCounter = false;
  config.profileEntryCount = 0;
  for (int i = 0; i < 3; i++)
  {
    config.profileStepperEnabled[i] = false;
    config.profileStepperWeightPerRev[i] = 0.0;
    config.profileStepperRpm[i] = 0;
  }
  for (int i = 0; i < PROFILE_MAX_ENTRIES; i++)
  {
    config.profileStepper[i] = 1;
    config.profileDiffWeight[i] = 0;
    config.profileSteps[i] = 0;
    config.profileRpm[i] = 0;
    config.profileMeasurements[i] = 0;
  }

  JsonObject general = doc["general"].as<JsonObject>();
  bool hasGeneralMeasurements = false;
  if (!general.isNull())
  {
    config.targetWeight = general["targetWeight"] | config.targetWeight;
    config.profileTolerance = general["tolerance"] | 0.000;
    config.profileAlarmThreshold = general["alarmThreshold"] | 0.000;
    config.profileWeightGap = general["weightGap"] | 1.000;
    config.profileBulkStepper = profileStepperNumber(general["bulkStepper"] | 1);
    config.profileStartAtZero = general["startAtZero"] | false;
    config.profileSessionCounter = general["sessionCounter"] | false;
    if (config.profileBulkStepper == 0)
    {
      config.profileBulkStepper = 1;
    }
    hasGeneralMeasurements = !general["measurements"].isNull();
    config.profileGeneralMeasurements = general["measurements"] | 20;
    if (config.profileGeneralMeasurements < 0)
    {
      config.profileGeneralMeasurements = 20;
    }
  }
  // Keep a profile file with an out-of-range targetWeight inside the weight domain.
  config.targetWeight = clampWeight(config.targetWeight);

  JsonObject stepperMap = doc["stepper"].as<JsonObject>();
  if (!stepperMap.isNull())
  {
    for (int stepperNumber = 1; stepperNumber <= 2; stepperNumber++)
    {
      JsonObject stepper = stepperMap[profileStepperName(stepperNumber)].as<JsonObject>();
      if (!stepper.isNull())
      {
        config.profileStepperEnabled[stepperNumber] = stepper["enabled"] | true;
        config.profileStepperWeightPerRev[stepperNumber] = stepper["weightPerRev"] | 0.0;
        config.profileStepperRpm[stepperNumber] = stepper["rpm"] | 0;
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
  if (!hasGeneralMeasurements && isCalibrationProfileFile(filename) && (config.profileEntryCount > 0))
  {
    config.profileGeneralMeasurements = config.profileMeasurements[0];
  }
  
  file.close();

  infoText = String(langText("status_profile_ready")) + config.profileName;
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

  config.wifiEnabled = doc["wifi"]["enabled"] | config.wifiEnabled;
  strlcpy(config.wifiSsid, doc["wifi"]["ssid"] | config.wifiSsid, sizeof(config.wifiSsid));
  strlcpy(config.wifiPsk, doc["wifi"]["psk"] | config.wifiPsk, sizeof(config.wifiPsk));
  strlcpy(config.wifiIpStatic, doc["wifi"]["ipStatic"] | config.wifiIpStatic, sizeof(config.wifiIpStatic));
  strlcpy(config.wifiIpGateway, doc["wifi"]["ipGateway"] | config.wifiIpGateway, sizeof(config.wifiIpGateway));
  strlcpy(config.wifiIpSubnet, doc["wifi"]["ipSubnet"] | config.wifiIpSubnet, sizeof(config.wifiIpSubnet));
  strlcpy(config.wifiIpDns, doc["wifi"]["ipDns"] | config.wifiIpDns, sizeof(config.wifiIpDns));
  strlcpy(config.scaleProtocol, doc["scale"]["protocol"] | config.scaleProtocol, sizeof(config.scaleProtocol));
  strlcpy(config.scaleCustomCode, doc["scale"]["customCode"] | config.scaleCustomCode, sizeof(config.scaleCustomCode));
  config.scaleBaud = doc["scale"]["baud"] | config.scaleBaud;
  config.motorStepsPerRev = doc["stepper"]["stepsPerRev"] | config.motorStepsPerRev;
  if (config.motorStepsPerRev <= 0)
  {
    config.motorStepsPerRev = DEFAULT_MOTOR_STEPS_PER_REV;
  }
  strlcpy(config.profileName, doc["activeProfile"] | config.profileName, sizeof(config.profileName));
  strlcpy(config.beeper, doc["beeper"] | config.beeper, sizeof(config.beeper));
  strlcpy(config.language, doc["language"] | config.language, sizeof(config.language));
  config.totalCounterEnable = doc["totalCounter"]["enable"] | config.totalCounterEnable;
  config.totalCount = doc["totalCounter"]["count"] | config.totalCount;
  if (config.totalCount < 0)
  {
    config.totalCount = 0;
  }

  // firmwareUpdate.url is intentionally firmware-only; only the check flag is configurable.
  config.fwUpdateCheck = doc["firmwareUpdate"]["check"] | config.fwUpdateCheck;

  file.close();

  persistedTotalCount = config.totalCount;
  return 1;
}

// Open a profile file and parse it into doc. Sets the SD read error and returns
// false on open/parse failure. Shared by the read-modify-write profile helpers.
static bool loadProfileDocument(const String &filename, JsonDocument &doc)
{
  File file = ACTIVE_FS.open(filename.c_str());
  if (!file)
  {
    setSdReadError(sdFilenameError("err_could_not_open_profile_file", filename.c_str()));
    DEBUG_PRINT("Failed to open profile: ");
    DEBUG_PRINTLN(filename);
    return false;
  }

  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error || !doc.is<JsonObject>())
  {
    String errorText = error ? String(error.c_str()) : String(langText("err_root_not_json"));
    setSdReadError(sdFilenameError("err_profile_json_parse_failed", filename.c_str()) + "\n" + errorText);
    DEBUG_PRINT("deserializeJson() failed on profile: ");
    DEBUG_PRINTLN(errorText);
    return false;
  }
  return true;
}

// Persist doc back over filename via a temp-file swap so a write that fails
// partway cannot corrupt the existing profile.
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

bool saveProfileTargetWeight(const char *profileName, float targetWeight)
{
  String filename = profileFilename(profileName);
  JsonDocument doc;
  if (!loadProfileDocument(filename, doc))
  {
    return false;
  }

  JsonObject general = doc["general"].as<JsonObject>();
  if (general.isNull())
  {
    general = doc["general"].to<JsonObject>();
  }
  general["targetWeight"] = serialized(weightToString(targetWeight));

  return writeProfileDocument(filename, doc);
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
  validationConfig.profileEntryCount = 0;
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

static void populateCalibrationTrickleMap(JsonDocument &doc, float weightPerRev, int profileRpm)
{
  const float diffWeights[8] = {1.929, 0.965, 0.482, 0.241, 0.121, 0.060, 0.030, 0.000};
  const size_t diffWeightsCount = sizeof(diffWeights) / sizeof(diffWeights[0]);
  const int measurements[8] = {2, 2, 5, 5, 10, 10, 15, 20};
  const float rs232LimitFactor = 0.65;

  JsonArray trickleMap = doc["trickleMap"].to<JsonArray>();
  for (int i = 0; i < diffWeightsCount; i++)
  {
    long steps = (weightPerRev > 0.0)
                     ? lround(((diffWeights[i] * (double)config.motorStepsPerRev) / weightPerRev) * rs232LimitFactor)
                     : 5;
    if (steps < 5)
    {
      steps = 5;
    }

    JsonObject profileEntry = trickleMap.add<JsonObject>();
    profileEntry["diffWeight"] = serialized(weightToString(diffWeights[i]));
    profileEntry["measurements"] = measurements[i];
    JsonObject stepper = profileEntry["stepper"].to<JsonObject>();
    stepper["id"] = 1;
    stepper["steps"] = steps;
    stepper["rpm"] = profileRpm;
  }
}


bool createProfileFromCalibration(float calibrationWeight, String &profileName)
{
  if (calibrationWeight <= 0.0)
  {
    updateDisplayLog(langText("msg_calibration_weight_invalid"), true);
    return false;
  }

  // The profile list (and selection) is capped at PROFILE_LIST_MAX total
  // profiles, so refuse to create another one that would never show up or be
  // auto-selected once that limit is reached.
  refreshProfileList();
  if (profileListCount >= PROFILE_LIST_MAX)
  {
    updateDisplayLog(langText("msg_no_free_profile_name"), true);
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

  // weightPerRev is the dispensed weight per full motor revolution. The
  // calibration throw runs config.profileSteps[0] steps (calibrate profile),
  // so the number of revolutions depends on config.motorStepsPerRev. With the
  // defaults (20000 steps / 200 steps-per-rev) this resolves to 100 revolutions.
  long calibrationSteps = config.profileSteps[0];
  if (calibrationSteps <= 0)
  {
    calibrationSteps = 20000;
  }
  double calibrationRevolutions = (double)calibrationSteps / (double)config.motorStepsPerRev;
  const float weightPerRev = calibrationRevolutions > 0.0
                                ? (float)(calibrationWeight / calibrationRevolutions)
                                : calibrationWeight;
  int profileRpm = config.profileRpm[0];
  if (profileRpm <= 0)
  {
    profileRpm = 200;
  }

  JsonDocument doc;
  JsonObject general = doc["general"].to<JsonObject>();
  general["targetWeight"] = serialized(weightToString(config.targetWeight));
  general["tolerance"] = serialized(weightToString(0.0f));
  general["alarmThreshold"] = serialized(weightToString(1.0f));
  general["weightGap"] = serialized(weightToString(1.0f));
  general["bulkStepper"] = 1;
  general["startAtZero"] = false;
  general["sessionCounter"] = false;
  general["measurements"] = config.profileGeneralMeasurements > 0 ? config.profileGeneralMeasurements : 20;

  JsonObject stepperMap = doc["stepper"].to<JsonObject>();
  JsonObject stepper1 = stepperMap["1"].to<JsonObject>();
  stepper1["enabled"] = true;
  stepper1["weightPerRev"] = serialized(weightToString(weightPerRev));
  stepper1["rpm"] = profileRpm;
  JsonObject stepper2 = stepperMap["2"].to<JsonObject>();
  stepper2["enabled"] = config.profileStepperEnabled[2];
  stepper2["weightPerRev"] = serialized(weightToString(config.profileStepperWeightPerRev[2] > 0.0 ? config.profileStepperWeightPerRev[2] : 10.0));
  stepper2["rpm"] = config.profileStepperRpm[2] > 0 ? config.profileStepperRpm[2] : 200;

  populateCalibrationTrickleMap(doc, weightPerRev, profileRpm);

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
  refreshProfileList();
  for (int i = 0; i < profileListCount; i++)
  {
    if (strcmp(profileList[i], profileName.c_str()) == 0)
    {
      setProfile(i);
      // setProfile() defers config saves; persist the freshly created profile
      // as the active one so it survives a reboot without a run in between.
      saveConfiguration("/config.txt", config);
      profileSelectionUnsaved = false;
      break;
    }
  }

  updateDisplayLog(String(langText("status_profile_created")) + profileName, true);
  return true;
}

bool tuneProfileWeightPerRev(const char *profileName, float weightPerRev)
{
  if (weightPerRev <= 0.0)
  {
    updateDisplayLog(langText("msg_calibration_weight_invalid"), true);
    return false;
  }

  String filename = profileFilename(profileName);
  JsonDocument doc;
  if (!loadProfileDocument(filename, doc))
  {
    return false;
  }

  int profileRpm = config.profileStepperRpm[1];
  if (profileRpm <= 0)
  {
    profileRpm = config.profileRpm[0];
  }
  if (profileRpm <= 0)
  {
    profileRpm = 200;
  }

  JsonObject stepperMap = doc["stepper"].as<JsonObject>();
  if (stepperMap.isNull())
  {
    stepperMap = doc["stepper"].to<JsonObject>();
  }
  JsonObject stepper1 = stepperMap["1"].as<JsonObject>();
  if (stepper1.isNull())
  {
    stepper1 = stepperMap["1"].to<JsonObject>();
  }
  stepper1["enabled"] = true;
  stepper1["weightPerRev"] = serialized(weightToString(weightPerRev));
  stepper1["rpm"] = profileRpm;

  populateCalibrationTrickleMap(doc, weightPerRev, profileRpm);

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
    if (profileCounter >= PROFILE_LIST_MAX)
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
          if (strcmp(profileList[i], filename.c_str()) == 0)
          {
            duplicate = true;
            break;
          }
        }
        if (!duplicate)
        {
          strlcpy(profileList[profileCounter], filename.c_str(), PROFILE_NAME_LEN);
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

void refreshProfileList()
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

  updateDisplayLog(String(langText("status_search_profiles")) + " " + String(profileListCount) + " / " + String(PROFILE_LIST_MAX));

  for (int i = 0; i < profileListCount; i++)
  {
    DEBUG_PRINTLN(profileList[i]);
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
  doc["wifi"]["enabled"] = config.wifiEnabled;
  doc["wifi"]["ssid"] = config.wifiSsid;
  doc["wifi"]["psk"] = config.wifiPsk;
  doc["wifi"]["ipStatic"] = config.wifiIpStatic;
  doc["wifi"]["ipGateway"] = config.wifiIpGateway;
  doc["wifi"]["ipSubnet"] = config.wifiIpSubnet;
  doc["wifi"]["ipDns"] = config.wifiIpDns;
  doc["scale"]["protocol"] = config.scaleProtocol;
  doc["scale"]["customCode"] = config.scaleCustomCode;
  doc["scale"]["baud"] = config.scaleBaud;
  doc["stepper"]["stepsPerRev"] = config.motorStepsPerRev;
  doc["activeProfile"] = config.profileName;
  doc["beeper"] = config.beeper;
  doc["language"] = config.language;
  doc["totalCounter"]["enable"] = config.totalCounterEnable;
  doc["totalCounter"]["count"] = config.totalCount;
  doc["firmwareUpdate"]["check"] = config.fwUpdateCheck;

  // Serialize JSON to file
  if (serializeJsonPretty(doc, file) == 0)
  {
    DEBUG_PRINTLN("Failed to write to file");
  }
  else
  {
    persistedTotalCount = config.totalCount;
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
