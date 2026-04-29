static bool hasRequiredProfileFields(JsonObject profileEntry)
{
  return !profileEntry.isNull() &&
         !profileEntry["weight"].isNull() &&
         !profileEntry["steps"].isNull() &&
         !profileEntry["speed"].isNull() &&
         !profileEntry["measurements"].isNull();
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

bool readProfile(const char *filename, Config &config)
{
  setSdReadError("");
  String infoText = "Loading Profile...";
  // labelInfo.drawButton(false, infoText);

  delay(500);

  if (!SD.exists(filename))
  {
    setSdReadError(String("Profile file not found:\n") + filename);
    DEBUG_PRINTLN("Profile not found:");
    DEBUG_PRINTLN(filename);
    DEBUG_PRINTLN("Set to calibrate.txt as default");
    filename = "/calibrate.txt";
    return false;
  }

  // Dump config file
  printFile(filename);

  // Open file for reading
  File file = SD.open(filename);
  if (!file)
  {
    setSdReadError(String("Could not open profile file:\n") + filename);
    DEBUG_PRINT("Failed to open profile: ");
    DEBUG_PRINTLN(filename);
    return false;
  }

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use https://arduinojson.org/v6/assistant to compute the capacity.
  JsonDocument doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, file);

  if (error || !doc.is<JsonObject>())
  {
    String errorText = error ? String(error.c_str()) : String("Root is not a JSON object");
    setSdReadError(String("Profile JSON parse failed:\n") + filename + "\n" + errorText);
    DEBUG_PRINT("deserializeJson() failed on : ");
    DEBUG_PRINT(filename);
    DEBUG_PRINT(" /");
    DEBUG_PRINTLN(errorText);
    file.close();
    return false;
  }

  config.profile_stepsPerUnit = 0;
  config.profile_tolerance = 0.000;
  config.profile_alarmThreshold = 1.000;
  config.profile_count = 0;
  for (int i = 0; i < 16; i++)
  {
    config.profile_num[i] = 1;
    config.profile_weight[i] = 0;
    config.profile_steps[i] = 0;
    config.profile_speed[i] = 0;
    config.profile_measurements[i] = 0;
    config.profile_reverse[i] = false;
  }

  bool profileSeen[16] = {false};
  for (JsonPair item : doc.as<JsonObject>())
  {
    int item_key = ((String(item.key().c_str()).toInt()) - 1);
    if ((item_key < 0) || (item_key >= 16))
    {
      setSdReadError(String("Invalid profile entry number:\n") + filename + "\nEntry: " + item.key().c_str());
      DEBUG_PRINT("Invalid profile entry: ");
      DEBUG_PRINTLN(item.key().c_str());
      file.close();
      return false;
    }

    JsonObject profileEntry = item.value().as<JsonObject>();
    if (!hasRequiredProfileFields(profileEntry))
    {
      setSdReadError(String("Incomplete profile entry:\n") + filename + "\nEntry: " + item.key().c_str() + "\nRequired: weight, steps, speed, measurements");
      DEBUG_PRINT("Incomplete profile entry: ");
      DEBUG_PRINTLN(item.key().c_str());
      file.close();
      return false;
    }

    int stepperNumber = profileEntry["number"] | 1;
    int stepperSpeed = profileEntry["speed"] | 200;
    int measurements = profileEntry["measurements"] | 5;
    float profileWeight = profileEntry["weight"] | 0.0;
    long profileSteps = profileEntry["steps"] | 0;
    if ((stepperNumber < 1) || (stepperNumber > 2) || (stepperSpeed <= 0) || (measurements < 0) || (profileWeight < 0) || (profileSteps <= 0))
    {
      setSdReadError(String("Invalid profile values:\n") + filename + "\nEntry: " + item.key().c_str());
      DEBUG_PRINT("Invalid profile values in entry: ");
      DEBUG_PRINTLN(item.key().c_str());
      file.close();
      return false;
    }

    if (item_key == 0)
    {
      config.profile_stepsPerUnit = profileEntry["stepsPerUnit"] | 0;
      config.profile_tolerance = profileEntry["tolerance"] | 0.000;
      config.profile_alarmThreshold = profileEntry["alarmThreshold"] | 1.000;
    }
    config.profile_num[item_key] = stepperNumber;
    config.profile_weight[item_key] = profileWeight;
    config.profile_steps[item_key] = profileSteps;
    config.profile_speed[item_key] = stepperSpeed;
    config.profile_measurements[item_key] = measurements;
    config.profile_reverse[item_key] = profileEntry["reverse"] | false;
    profileSeen[item_key] = true;
    if ((item_key + 1) > config.profile_count)
    {
      config.profile_count = item_key + 1;
    }
  }
  if (config.profile_count <= 0)
  {
    setSdReadError(String("Profile has no entries:\n") + filename);
    DEBUG_PRINTLN("Profile has no entries");
    file.close();
    return false;
  }
  for (int i = 0; i < config.profile_count; i++)
  {
    if (!profileSeen[i])
    {
      setSdReadError(String("Profile entries must be contiguous:\n") + filename + "\nMissing entry: " + String(i + 1));
      DEBUG_PRINT("Profile entries must be contiguous. Missing entry: ");
      DEBUG_PRINTLN(i + 1);
      file.close();
      return false;
    }
  }
  DEBUG_PRINTLN("POWDER_AKTIVE");
  file.close();

  // doc.garbageCollect();

  infoText = "Profile Loaded:";
  infoText += filename;
  // labelInfo.drawButton(false, infoText);
  return true;
}

// Loads the configuration from a file
bool loadConfiguration(const char *filename, Config &config)
{
  setSdReadError("");
  // Dump config file
  printFile(filename);

  // Open file for reading
  File file = SD.open(filename);
  if (!file)
  {
    setSdReadError(String("Could not open config file:\n") + filename);
    DEBUG_PRINT("Failed to open config: ");
    DEBUG_PRINTLN(filename);
    return 0;
  }

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use https://arduinojson.org/v6/assistant to compute the capacity.
  JsonDocument doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, file);

  if (error || !doc.is<JsonObject>())
  {
    String errorText = error ? String(error.c_str()) : String("Root is not a JSON object");
    setSdReadError(String("Config JSON parse failed:\n") + filename + "\n" + errorText);
    DEBUG_PRINT("deserializeJson() failed on config.txt: ");
    DEBUG_PRINTLN(errorText);
    file.close();
    return 0;
  }

  strlcpy(config.wifi_ssid,          // <- destination
          doc["wifi"]["ssid"] | "",  // <- source
          sizeof(config.wifi_ssid)); // <- destination's capacity

  strlcpy(config.wifi_psk,          // <- destination
          doc["wifi"]["psk"] | "",  // <- source
          sizeof(config.wifi_psk)); // <- destination's capacity

  strlcpy(config.IPStatic,              // <- destination
          doc["wifi"]["IPStatic"] | "", // <- source
          sizeof(config.IPStatic));     // <- destination's capacity

  strlcpy(config.IPGateway,              // <- destination
          doc["wifi"]["IPGateway"] | "", // <- source
          sizeof(config.IPGateway));     // <- destination's capacity

  strlcpy(config.IPSubnet,              // <- destination
          doc["wifi"]["IPSubnet"] | "", // <- source
          sizeof(config.IPSubnet));     // <- destination's capacity

  strlcpy(config.IPDns,              // <- destination
          doc["wifi"]["IPDNS"] | "", // <- source
          sizeof(config.IPDns));     // <- destination's capacity

  strlcpy(config.scale_protocol,           // <- destination
          doc["scale"]["protocol"] | "GG", // <- source
          sizeof(config.scale_protocol));  // <- destination's capacity

  strlcpy(config.scale_customCode,          // <- destination
          doc["scale"]["customCode"] | "",  // <- source
          sizeof(config.scale_customCode)); // <- destination's capacity

  config.scale_baud = doc["scale"]["baud"] | 9600;

  strlcpy(config.profile,               // <- destination
          doc["profile"] | "calibrate", // <- source
          sizeof(config.profile));      // <- destination's capacity

  config.targetWeight = doc["weight"] | 1.0;

  strlcpy(config.beeper,          // <- destination
          doc["beeper"] | "done", // <- source
          sizeof(config.beeper)); // <- destination's capacity

  config.fwCheck = doc["fw_check"] | true;

  file.close();

  // doc.garbageCollect();
  return 1;
}

bool isValidProfileFile(const char *filename)
{
  File file = SD.open(filename);
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

  bool hasProfileEntries = false;
  bool profileSeen[16] = {false};
  int profileCount = 0;
  for (JsonPair item : doc.as<JsonObject>())
  {
    int item_key = String(item.key().c_str()).toInt();
    if ((item_key < 1) || (item_key > 16))
    {
      return false;
    }

    JsonObject profileEntry = item.value().as<JsonObject>();
    if (!hasRequiredProfileFields(profileEntry))
    {
      return false;
    }

    int stepperNumber = profileEntry["number"] | 1;
    int stepperSpeed = profileEntry["speed"] | 0;
    int measurements = profileEntry["measurements"] | -1;
    float profileWeight = profileEntry["weight"] | -1.0;
    long profileSteps = profileEntry["steps"] | 0;
    if ((stepperNumber < 1) || (stepperNumber > 2) || (stepperSpeed <= 0) || (measurements < 0) || (profileWeight < 0) || (profileSteps <= 0))
    {
      return false;
    }

    profileSeen[item_key - 1] = true;
    if (item_key > profileCount)
    {
      profileCount = item_key;
    }
    hasProfileEntries = true;
  }

  for (int i = 0; i < profileCount; i++)
  {
    if (!profileSeen[i])
    {
      return false;
    }
  }

  return hasProfileEntries;
}

String nextCalibrationProfileName()
{
  for (int i = 0; i <= 999; i++)
  {
    char profileName[16];
    snprintf(profileName, sizeof(profileName), "powder_%03d", i);
    String filename = "/" + String(profileName) + ".txt";
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
    updateDisplayLog("Calibration weight invalid!", true);
    return false;
  }

  profileName = nextCalibrationProfileName();
  if (profileName.length() <= 0)
  {
    updateDisplayLog("No free powder profile name!", true);
    return false;
  }

  const float grainWeights[10] = {
      15.432, 7.716, 3.858, 1.929, 0.965,
      0.482, 0.241, 0.121, 0.060, 0.000};
  const float calcTolerance = 65.0;
  const float stepsForCalibration = (20000.0 / 100.0) * calcTolerance;
  const int measurementsInput = 5;
  int profileSpeed = config.profile_speed[0];
  if (profileSpeed <= 0)
  {
    profileSpeed = 200;
  }

  JsonDocument doc;
  for (int i = 0; i < 10; i++)
  {
    int measurements = measurementsInput;
    if (i == 0)
    {
      measurements = 10;
    }
    if (i == 7)
    {
      measurements = 10;
    }
    if (i == 8)
    {
      measurements = 15;
    }
    if (i == 9)
    {
      measurements = 20;
    }

    long steps = lround((grainWeights[i] / calibrationWeight) * stepsForCalibration);
    if (steps < 10)
    {
      steps = 10;
    }

    JsonObject profileEntry = doc[String(i + 1)].to<JsonObject>();
    profileEntry["weight"] = serialized(String(grainWeights[i], 3));
    profileEntry["steps"] = steps;
    profileEntry["speed"] = profileSpeed;
    profileEntry["measurements"] = measurements;

    if (i == 0)
    {
      profileEntry["stepsPerUnit"] = lround((1.0 / calibrationWeight) * stepsForCalibration);
      profileEntry["tolerance"] = serialized(String(0.0, 3));
      profileEntry["alarmThreshold"] = serialized(String(0.1, 3));
    }
  }

  String filename = "/" + profileName + ".txt";
  File file = SD.open(filename.c_str(), FILE_WRITE);
  if (!file)
  {
    updateDisplayLog("Failed to create profile!", true);
    return false;
  }

  bool written = serializeJsonPretty(doc, file) > 0;
  file.close();
  if (!written)
  {
    SD.remove(filename.c_str());
    updateDisplayLog("Failed to write profile!", true);
    return false;
  }

  getProfileList();
  for (int i = 0; i < profileListCount; i++)
  {
    if (profileListBuff[i] == profileName)
    {
      setProfile(i);
      break;
    }
  }

  updateDisplayLog(String("Profile created: ") + profileName, true);
  return true;
}

void getProfileList()
{
  File root = SD.open("/");
  profileListCount = 0;
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

  updateDisplayLog("Search Profiles...");

  byte profileCounter = 0;
  byte invalidProfileCounter = 0;
  String invalidProfiles = "";
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
        profileListBuff[profileCounter] = filename;
        profileCounter++;
      }
      else if (isProfileCandidate)
      {
        updateDisplayLog(String("Invalid profile ignored: ") + fileName);
        invalidProfileCounter++;
        if (invalidProfileCounter <= 8)
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

  profileListCount = profileCounter;
  DEBUG_PRINT("  profileListCount: ");
  DEBUG_PRINTLN(profileListCount);
  for (int i = 0; i < profileListCount; i++)
  {
    DEBUG_PRINTLN(profileListBuff[i]);
  }
  if (invalidProfileCounter > 0)
  {
    String message = "Invalid profiles found:";
    message += invalidProfiles;
    if (invalidProfileCounter > 8)
    {
      message += "\n...";
    }
    message += "\n\nThey were ignored.";
    messageBox(message.c_str(), &lv_font_montserrat_14, lv_color_hex(0xFF0000), true);
  }
}

void saveConfiguration(const char *filename, const Config &config)
{
  // Delete existing file, otherwise the configuration is appended to the file
  SD.remove(filename);

  // Open file for writing
  File file = SD.open(filename, FILE_WRITE);
  if (!file)
  {
    Serial.println(F("Failed to create file"));
    return;
  }

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use https://arduinojson.org/assistant to compute the capacity.
  JsonDocument doc;
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
  doc["weight"] = serialized(String(config.targetWeight, 3));
  doc["beeper"] = config.beeper;
  doc["fw_check"] = config.fwCheck;

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
  File file = SD.open(filename);

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
