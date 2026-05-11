static byte profileActuatorNumber(const char *actuatorName)
{
  if ((actuatorName == NULL) || (strcmp(actuatorName, "stepper1") == 0))
  {
    return PROFILE_ACTUATOR_MIN;
  }
  if (strcmp(actuatorName, "stepper2") == 0)
  {
    return PROFILE_ACTUATOR_MAX;
  }
  return 0;
}

static const char *profileActuatorName(byte stepperNumber)
{
  return stepperNumber == PROFILE_ACTUATOR_MAX ? "stepper2" : "stepper1";
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
         (normalized == "calibration.txt") ||
         normalized.endsWith("/calibrate.txt") ||
         normalized.endsWith("/calibration.txt");
}

static bool isProfileMetadataKey(const char *key)
{
  return (strcmp(key, "general") == 0) ||
         (strcmp(key, "actuator") == 0) ||
         (strcmp(key, "rs232TrickleMap") == 0);
}

static bool loadProfileEntry(JsonObject profileEntry,
                             int itemNumber,
                             const char *filename,
                             Config &cfg,
                             bool showErrors,
                             bool allowCalibrationDefaults)
{
  bool hasRequiredFields = hasRequiredProfileFields(profileEntry);
  if (!hasRequiredFields && (!allowCalibrationDefaults || !hasCalibrationProfileFields(profileEntry)))
  {
    if (showErrors)
    {
      String message = String("Incomplete profile entry:\n") + filename +
                       "\nEntry: " + String(itemNumber) +
                       "\nRequired: diffWeight, actuator, steps, speed, measurements";
      setSdReadError(message);
      DEBUG_PRINT("Incomplete profile entry: ");
      DEBUG_PRINTLN(itemNumber);
    }
    return false;
  }

  byte stepperNumber = profileEntry["number"].isNull()
                           ? profileActuatorNumber(profileEntry["actuator"] | "stepper1")
                           : (byte)(profileEntry["number"] | 1);
  int stepperSpeed = profileEntry["speed"] | DEFAULT_PROFILE_SPEED;
  int measurements = profileEntry["measurements"] | DEFAULT_PROFILE_STEP_MEASUREMENTS;
  float profileWeight = profileEntry["diffWeight"] | 0.0;
  if (profileEntry["diffWeight"].isNull())
  {
    profileWeight = profileEntry["weight"] | 0.0;
  }
  long profileSteps = profileEntry["steps"] | 0;
  if ((stepperNumber < PROFILE_ACTUATOR_MIN) ||
      (stepperNumber > PROFILE_ACTUATOR_MAX) ||
      (stepperSpeed <= 0) ||
      (measurements < 0) ||
      (profileWeight < 0) ||
      (profileSteps <= 0))
  {
    if (showErrors)
    {
      setSdReadError(String("Invalid profile values:\n") + filename + "\nEntry: " + String(itemNumber));
      DEBUG_PRINT("Invalid profile values in entry: ");
      DEBUG_PRINTLN(itemNumber);
    }
    return false;
  }

  if (cfg.profile_count >= MAX_PROFILE_STEPS)
  {
    return true;
  }

  int item_key = cfg.profile_count;
  ProfileStep &step = cfg.profile_step[item_key];
  if (item_key == 0)
  {
    cfg.profile_tolerance = profileEntry["tolerance"] | cfg.profile_tolerance;
    cfg.profile_alarmThreshold = profileEntry["alarmThreshold"] | cfg.profile_alarmThreshold;
  }
  step.actuator = stepperNumber;
  step.weight = profileWeight;
  step.steps = profileSteps;
  step.speed = stepperSpeed;
  step.measurements = measurements;
  step.reverse = profileEntry["reverse"] | false;
  cfg.profile_count++;
  return true;
}

static bool loadProfileEntries(JsonObject doc, const char *filename, Config &cfg, bool showErrors)
{
  bool allowCalibrationDefaults = isCalibrationProfileFile(filename);
  JsonArray rs232TrickleMap = doc["rs232TrickleMap"].as<JsonArray>();
  if (!rs232TrickleMap.isNull())
  {
    int itemNumber = 1;
    for (JsonVariant item : rs232TrickleMap)
    {
      if (!loadProfileEntry(item.as<JsonObject>(), itemNumber, filename, cfg, showErrors, allowCalibrationDefaults))
      {
        return false;
      }
      itemNumber++;
    }
    return cfg.profile_count > 0;
  }

  if (!allowCalibrationDefaults)
  {
    if (showErrors)
    {
      setSdReadError(String("Profile has no rs232TrickleMap:\n") + filename);
      DEBUG_PRINTLN("Profile has no rs232TrickleMap");
    }
    return false;
  }

  if (hasCalibrationProfileFields(doc))
  {
    return loadProfileEntry(doc, 1, filename, cfg, showErrors, true);
  }

  for (JsonPair item : doc)
  {
    const char *key = item.key().c_str();
    if (isProfileMetadataKey(key))
    {
      continue;
    }

    int itemNumber = String(key).toInt();
    if ((itemNumber < 1) || (itemNumber > MAX_PROFILE_STEPS))
    {
      if (showErrors)
      {
        setSdReadError(String("Invalid profile entry number:\n") + filename + "\nEntry: " + key);
        DEBUG_PRINT("Invalid profile entry: ");
        DEBUG_PRINTLN(key);
      }
      return false;
    }

    if (!loadProfileEntry(item.value().as<JsonObject>(), itemNumber, filename, cfg, showErrors, true))
    {
      return false;
    }
  }

  return cfg.profile_count > 0;
}

static void resetProfileRuntimeConfig(Config &cfg)
{
  cfg.profile_tolerance = 0.000;
  cfg.profile_alarmThreshold = 0.000;
  cfg.profile_weightGap = 1.000;
  cfg.profile_generalMeasurements = DEFAULT_PROFILE_GENERAL_MEASUREMENTS;
  cfg.profile_count = 0;

  for (int i = 0; i < PROFILE_ACTUATOR_SLOTS; i++)
  {
    cfg.profile_actuator[i] = {false, 0.0, 0};
  }
  for (int i = 0; i < MAX_PROFILE_STEPS; i++)
  {
    cfg.profile_step[i] = {PROFILE_ACTUATOR_MIN, 0.0, 0, 0, 0, false};
  }
}

static bool loadProfileGeneralSettings(JsonObject general, Config &cfg)
{
  if (general.isNull())
  {
    return false;
  }

  cfg.targetWeight = general["targetWeight"] | cfg.targetWeight;
  cfg.profile_tolerance = general["tolerance"] | 0.000;
  cfg.profile_alarmThreshold = general["alarmThreshold"] | 0.000;
  cfg.profile_weightGap = general["weightGap"] | 1.000;
  bool hasGeneralMeasurements = !general["measurements"].isNull();
  cfg.profile_generalMeasurements = general["measurements"] | DEFAULT_PROFILE_GENERAL_MEASUREMENTS;
  if (cfg.profile_generalMeasurements < 0)
  {
    cfg.profile_generalMeasurements = DEFAULT_PROFILE_GENERAL_MEASUREMENTS;
  }
  return hasGeneralMeasurements;
}

static void loadProfileActuatorSettings(JsonObject actuator, Config &cfg)
{
  if (actuator.isNull())
  {
    return;
  }

  for (int stepperNumber = PROFILE_ACTUATOR_MIN; stepperNumber <= PROFILE_ACTUATOR_MAX; stepperNumber++)
  {
    JsonObject stepper = actuator[profileActuatorName(stepperNumber)].as<JsonObject>();
    if (stepper.isNull())
    {
      continue;
    }

    ProfileActuator &profileActuator = cfg.profile_actuator[stepperNumber];
    profileActuator.enabled = stepper["enabled"] | true;
    profileActuator.unitsPerThrow = stepper["unitsPerThrow"] | 0.0;
    profileActuator.unitsPerThrowSpeed = stepper["unitsPerThrowSpeed"] | 0;
  }
}

String profileFilename(const char *profileName)
{
  return "/profiles/" + String(profileName) + ".txt";
}

bool readProfile(const char *filename, Config &cfg)
{
  setSdReadError("");
  String infoText = String(languageText("status_loading_profile")) + filename;
  updateDisplayLog(infoText, true);

  if (!SD.exists(filename))
  {
    setSdReadError(String("Profile file not found:\n") + filename);
    DEBUG_PRINTLN("Profile not found:");
    DEBUG_PRINTLN(filename);
    return false;
  }

  // Dump config file
  printFile(filename);

  JsonDocument doc;
  if (!readJsonObjectFile(filename, doc, "Profile", true))
  {
    return false;
  }

  resetProfileRuntimeConfig(cfg);
  bool hasGeneralMeasurements = loadProfileGeneralSettings(doc["general"].as<JsonObject>(), cfg);
  loadProfileActuatorSettings(doc["actuator"].as<JsonObject>(), cfg);

  if (!loadProfileEntries(doc.as<JsonObject>(), filename, cfg, true))
  {
    if (getSdReadError().length() <= 0)
    {
      setSdReadError(String("Profile has no entries:\n") + filename);
      DEBUG_PRINTLN("Profile has no entries");
    }
    return false;
  }
  if (!hasGeneralMeasurements && isCalibrationProfileFile(filename) && (cfg.profile_count > 0))
  {
    cfg.profile_generalMeasurements = cfg.profile_step[0].measurements;
  }
  DEBUG_PRINTLN("PROFILE_ACTIVE");

  infoText = String(languageText("status_profile_ready")) + cfg.profile;
  updateDisplayLog(infoText, true);
  return true;
}

bool saveProfileTargetWeight(const char *profileName, float targetWeight)
{
  String filename = profileFilename(profileName);
  JsonDocument doc;
  if (!readJsonObjectFile(filename.c_str(), doc, "Profile", true))
  {
    return false;
  }

  JsonObject general = doc["general"].as<JsonObject>();
  if (general.isNull())
  {
    general = doc["general"].to<JsonObject>();
  }
  general["targetWeight"] = serialized(String(targetWeight, 3));

  String tempFilename = filename + ".tmp";
  SD.remove(tempFilename.c_str());
  File file = SD.open(tempFilename.c_str(), FILE_WRITE);
  if (!file)
  {
    setSdReadError(String("Could not write profile file:\n") + tempFilename);
    DEBUG_PRINT("Failed to create profile temp file: ");
    DEBUG_PRINTLN(tempFilename);
    return false;
  }

  bool written = serializeJsonPretty(doc, file) > 0;
  file.close();
  if (!written)
  {
    SD.remove(tempFilename.c_str());
    setSdReadError(String("Could not write profile file:\n") + filename);
    DEBUG_PRINTLN("Failed to write profile target weight");
    return false;
  }

  SD.remove(filename.c_str());
  if (!SD.rename(tempFilename.c_str(), filename.c_str()))
  {
    SD.remove(tempFilename.c_str());
    setSdReadError(String("Could not replace profile file:\n") + filename);
    DEBUG_PRINT("Failed to replace profile file: ");
    DEBUG_PRINTLN(filename);
    return false;
  }

  return true;
}

bool isValidProfileFile(const char *filename)
{
  JsonDocument doc;
  if (!readJsonObjectFile(filename, doc, "Profile", false))
  {
    return false;
  }

  Config validationConfig = config;
  validationConfig.profile_count = 0;
  return loadProfileEntries(doc.as<JsonObject>(), filename, validationConfig, false);
}
