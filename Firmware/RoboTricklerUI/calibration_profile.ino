String nextCalibrationProfileName()
{
  for (int i = 0; i <= 999; i++)
  {
    char profileName[16];
    snprintf(profileName, sizeof(profileName), "powder_%03d", i);
    String filename = "/profiles/" + String(profileName) + ".txt";
    if (!SD.exists(filename.c_str()))
    {
      return String(profileName);
    }
  }

  return "";
}

bool createProfileFromCalibration(float calibrationWeight, String &profileName)
{
  if (calibrationWeight <= 0.0)
  {
    updateDisplayLog(languageText("msg_calibration_weight_invalid"), true);
    return false;
  }

  profileName = nextCalibrationProfileName();
  if (profileName.length() <= 0)
  {
    updateDisplayLog(languageText("msg_no_free_profile_name"), true);
    return false;
  }

  String infoText = String(languageText("status_creating_profile")) + profileName;
  updateDisplayLog(infoText, true);

  const float diffWeights[5] = {1.929, 0.965, 0.482, 0.241, 0.000};
  const int measurements[5] = {2, 2, 5, 10, 15};
  const float rs232LimitFactor = 0.65;
  const float unitsPerThrow = calibrationWeight / 100.0;
  int profileSpeed = config.profile_step[0].speed;
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
  general["measurements"] = config.profile_generalMeasurements > 0 ? config.profile_generalMeasurements : 20;

  JsonObject actuator = doc["actuator"].to<JsonObject>();
  JsonObject stepper1 = actuator["stepper1"].to<JsonObject>();
  stepper1["enabled"] = true;
  stepper1["unitsPerThrow"] = serialized(String(unitsPerThrow, 3));
  stepper1["unitsPerThrowSpeed"] = profileSpeed;
  JsonObject stepper2 = actuator["stepper2"].to<JsonObject>();
  ProfileActuator &profileActuator2 = config.profile_actuator[2];
  stepper2["enabled"] = profileActuator2.enabled;
  stepper2["unitsPerThrow"] = serialized(String(profileActuator2.unitsPerThrow > 0.0 ? profileActuator2.unitsPerThrow : 10.0, 3));
  stepper2["unitsPerThrowSpeed"] = profileActuator2.unitsPerThrowSpeed > 0 ? profileActuator2.unitsPerThrowSpeed : 200;

  JsonArray rs232TrickleMap = doc["rs232TrickleMap"].to<JsonArray>();
  for (int i = 0; i < 5; i++)
  {
    long steps = lround(((diffWeights[i] * 200.0) / unitsPerThrow) * rs232LimitFactor);
    if (steps < 5)
    {
      steps = 5;
    }

    JsonObject profileEntry = rs232TrickleMap.add<JsonObject>();
    profileEntry["diffWeight"] = serialized(String(diffWeights[i], 3));
    profileEntry["actuator"] = "stepper1";
    profileEntry["steps"] = steps;
    profileEntry["speed"] = profileSpeed;
    profileEntry["reverse"] = false;
    profileEntry["measurements"] = measurements[i];
  }

  if (!SD.exists("/profiles") && !SD.mkdir("/profiles"))
  {
    updateDisplayLog(languageText("msg_profiles_folder_failed"), true);
    return false;
  }

  String filename = "/profiles/" + profileName + ".txt";
  infoText = String(languageText("status_writing_profile")) + profileName;
  updateDisplayLog(infoText, true);
  File file = SD.open(filename.c_str(), FILE_WRITE);
  if (!file)
  {
    updateDisplayLog(languageText("msg_profile_file_create_failed"), true);
    return false;
  }

  bool written = serializeJsonPretty(doc, file) > 0;
  file.close();
  if (!written)
  {
    SD.remove(filename.c_str());
    updateDisplayLog(languageText("msg_profile_file_write_failed"), true);
    return false;
  }

  infoText = languageText("status_refresh_profile_list");
  updateDisplayLog(infoText, true);
  refreshProfileList();
  int profileIndex = profileListIndexOf(profileName.c_str());
  if (profileIndex >= 0)
  {
    selectProfile(profileIndex);
  }

  updateDisplayLog(String(languageText("status_profile_created")) + profileName, true);
  return true;
}
