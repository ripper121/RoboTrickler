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

static bool loadProfileEntry(JsonObject profileEntry, int itemNumber, const char *filename, Config &config, bool showErrors, bool allowCalibrationDefaults)
{
  bool hasRequiredFields = hasRequiredProfileFields(profileEntry);
  if (!hasRequiredFields && (!allowCalibrationDefaults || !hasCalibrationProfileFields(profileEntry)))
  {
    if (showErrors)
    {
      setSdReadError(String("Incomplete profile entry:\n") + filename + "\nEntry: " + String(itemNumber) + "\nRequired: diffWeight, actuator, steps, speed, measurements");
      DEBUG_PRINT("Incomplete profile entry: ");
      DEBUG_PRINTLN(itemNumber);
    }
    return false;
  }

  byte stepperNumber = profileEntry["number"].isNull()
                           ? profileActuatorNumber(profileEntry["actuator"] | "stepper1")
                           : (byte)(profileEntry["number"] | 1);
  int stepperSpeed = profileEntry["speed"] | 200;
  int measurements = profileEntry["measurements"] | 5;
  float profileWeight = profileEntry["diffWeight"] | 0.0;
  if (profileEntry["diffWeight"].isNull())
  {
    profileWeight = profileEntry["weight"] | 0.0;
  }
  long profileSteps = profileEntry["steps"] | 0;
  if ((stepperNumber < 1) || (stepperNumber > 2) || (stepperSpeed <= 0) || (measurements < 0) || (profileWeight < 0) || (profileSteps <= 0))
  {
    if (showErrors)
    {
      setSdReadError(String("Invalid profile values:\n") + filename + "\nEntry: " + String(itemNumber));
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
  ProfileStep &step = config.profile_step[item_key];
  if (item_key == 0)
  {
    config.profile_tolerance = profileEntry["tolerance"] | config.profile_tolerance;
    config.profile_alarmThreshold = profileEntry["alarmThreshold"] | config.profile_alarmThreshold;
  }
  step.actuator = stepperNumber;
  step.weight = profileWeight;
  step.steps = profileSteps;
  step.speed = stepperSpeed;
  step.measurements = measurements;
  step.reverse = profileEntry["reverse"] | false;
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
    return loadProfileEntry(doc, 1, filename, config, showErrors, true);
  }

  for (JsonPair item : doc)
  {
    const char *key = item.key().c_str();
    if (isProfileMetadataKey(key))
    {
      continue;
    }

    int itemNumber = String(key).toInt();
    if ((itemNumber < 1) || (itemNumber > 16))
    {
      if (showErrors)
      {
        setSdReadError(String("Invalid profile entry number:\n") + filename + "\nEntry: " + key);
        DEBUG_PRINT("Invalid profile entry: ");
        DEBUG_PRINTLN(key);
      }
      return false;
    }

    if (!loadProfileEntry(item.value().as<JsonObject>(), itemNumber, filename, config, showErrors, true))
    {
      return false;
    }
  }

  return config.profile_count > 0;
}

String profileFilename(const char *profileName)
{
  return "/profiles/" + String(profileName) + ".txt";
}

bool readProfile(const char *filename, Config &config)
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

  config.profile_tolerance = 0.000;
  config.profile_alarmThreshold = 0.000;
  config.profile_weightGap = 1.000;
  config.profile_generalMeasurements = 20;
  config.profile_count = 0;
  for (int i = 0; i < 3; i++)
  {
    config.profile_actuator[i] = {false, 0.0, 0};
  }
  for (int i = 0; i < 16; i++)
  {
    config.profile_step[i] = {1, 0.0, 0, 0, 0, false};
  }

  JsonObject general = doc["general"].as<JsonObject>();
  bool hasGeneralMeasurements = false;
  if (!general.isNull())
  {
    config.targetWeight = general["targetWeight"] | config.targetWeight;
    config.profile_tolerance = general["tolerance"] | 0.000;
    config.profile_alarmThreshold = general["alarmThreshold"] | 0.000;
    config.profile_weightGap = general["weightGap"] | 1.000;
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
        ProfileActuator &profileActuator = config.profile_actuator[stepperNumber];
        profileActuator.enabled = stepper["enabled"] | true;
        profileActuator.unitsPerThrow = stepper["unitsPerThrow"] | 0.0;
        profileActuator.unitsPerThrowSpeed = stepper["unitsPerThrowSpeed"] | 0;
      }
    }
  }

  if (!loadProfileEntries(doc.as<JsonObject>(), filename, config, true))
  {
    if (getSdReadError().length() <= 0)
    {
      setSdReadError(String("Profile has no entries:\n") + filename);
      DEBUG_PRINTLN("Profile has no entries");
    }
    return false;
  }
  if (!hasGeneralMeasurements && isCalibrationProfileFile(filename) && (config.profile_count > 0))
  {
    config.profile_generalMeasurements = config.profile_step[0].measurements;
  }
  DEBUG_PRINTLN("PROFILE_ACTIVE");

  infoText = String(languageText("status_profile_ready")) + config.profile;
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
