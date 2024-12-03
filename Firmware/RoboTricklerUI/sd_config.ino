bool readProfile(const char *filename, Config &config)
{
  String infoText = "Loading Profile...";
  // labelInfo.drawButton(false, infoText);

  delay(500);

  if (!SD.exists(filename))
  {
    DEBUG_PRINTLN("Profile not found:");
    DEBUG_PRINTLN(filename);
    DEBUG_PRINTLN("Set to calibrate.txt as default");
    filename = "/calibrate.txt";

    strlcpy(config.profile,          // <- destination
            "calibrate",             // <- source
            sizeof(config.profile)); // <- destination's capacity

    saveConfiguration("/config.txt", config);
  }

  // Dump config file
  printFile(filename);

  // Open file for reading
  File file = SD.open(filename);

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use https://arduinojson.org/v6/assistant to compute the capacity.
  JsonDocument doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, file);

  if (error)
  {
    DEBUG_PRINT("deserializeJson() failed on : ");
    DEBUG_PRINT(filename);
    DEBUG_PRINT(" /");
    DEBUG_PRINTLN(error.c_str());
    return false;
  }

  if (String(filename).indexOf("pid") != -1)
  {
    config.pidThreshold = doc["threshold"] | 0.10;
    config.pidStepMin = doc["stepMin"] | 5;
    config.pidStepMax = doc["stepMax"] | 36000;
    config.pidSpeed = doc["speed"] | 200;
    config.pidConMeasurements = doc["conMeasurements"] | 2;
    config.pidAggMeasurements = doc["aggMeasurements"] | 25;
    config.pidTolerance = doc["tolerance"] | 0.000;
    config.pidAlarmThreshold = doc["alarmThreshold"] | 1.000;
    config.pidConsNum = doc["consNum"] | 1;
    config.pidConsKp = doc["consKp"] | 1.00;
    config.pidConsKi = doc["consKi"] | 0.05;
    config.pidConsKd = doc["consKd"] | 0.25;
    config.pidAggNum = doc["aggNum"] | 1;
    config.pidAggKp = doc["aggKp"] | 4.00;
    config.pidAggKi = doc["aggKi"] | 0.2;
    config.pidAggKd = doc["aggKd"] | 1.00;
    config.pidOscillate = doc["oscillate"] | false;
    config.pidReverse = doc["reverse"] | false;
    config.pidAcceleration = doc["acceleration"] | false;
    DEBUG_PRINTLN("PID_AKTIVE");
    PID_AKTIVE = true;
  }
  else
  {
    for (JsonPair item : doc.as<JsonObject>())
    {
      int item_key = ((String(item.key().c_str()).toInt()) - 1);
      if (item_key == 0)
      {
        config.profile_stepsPerUnit = item.value()["stepsPerUnit"] | 0;
        config.profile_tolerance = item.value()["tolerance"] | 0.000;
        config.profile_alarmThreshold = item.value()["alarmThreshold"] | 1.000;
        config.profile_powderMeasure = item.value()["powderMeasure"] | false;
      }
      config.profile_num[item_key] = item.value()["number"] | 1;
      config.profile_weight[item_key] = item.value()["weight"];
      config.profile_steps[item_key] = item.value()["steps"];
      config.profile_speed[item_key] = item.value()["speed"];
      config.profile_measurements[item_key] = item.value()["measurements"];
      config.profile_oscillate[item_key] = item.value()["oscillate"] | false;
      config.profile_reverse[item_key] = item.value()["reverse"] | false;
      config.profile_acceleration[item_key] = item.value()["acceleration"] | false;
      config.profile_count = item_key + 1;
    }
    DEBUG_PRINTLN("POWDER_AKTIVE");
    PID_AKTIVE = false;
  }
  file.close();

  // doc.garbageCollect();

  infoText = "Profile Loaded:";
  infoText += filename;
  // labelInfo.drawButton(false, infoText);
  return true;
}

// Loads the configuration from a file
int loadConfiguration(const char *filename, Config &config)
{
  // Dump config file
  printFile(filename);

  // Open file for reading
  File file = SD.open(filename);

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use https://arduinojson.org/v6/assistant to compute the capacity.
  JsonDocument doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, file);

  if (error)
  {
    DEBUG_PRINT("deserializeJson() failed on config.txt: ");
    DEBUG_PRINTLN(error.c_str());
    return 1;
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

  config.log_measurements = doc["log_Measurements"] | 20;

  config.weight = doc["weight"] | 1.0;

  config.microsteps = doc["microsteps"] | 1;

  strlcpy(config.beeper,          // <- destination
          doc["beeper"] | "done", // <- source
          sizeof(config.beeper)); // <- destination's capacity

  config.debugLog = doc["debug_log"] | false;

  config.fwCheck = doc["fw_check"] | true;

  file.close();

  // doc.garbageCollect();

  String profile_filename = "/" + String(config.profile) + ".txt";
  if (!readProfile(profile_filename.c_str(), config))
    return 2;

  return 0;
}

void getProfileList()
{
  File root = SD.open("/");
  if (!root)
  {
    DEBUG_PRINTLN("Failed to open directory");
    return;
  }
  if (!root.isDirectory())
  {
    DEBUG_PRINTLN("Not a directory");
    return;
  }

  File file = root.openNextFile();

  updateDisplayLog("Search Profiles...");

  byte profileCounter = 0;
  while (file)
  {
    if (profileCounter > 31)
      break;
    if (file.isDirectory())
    {
      DEBUG_PRINT("  DIR : ");
      DEBUG_PRINTLN(file.name());
    }
    else
    {
      if ((String(file.name()).indexOf(".txt") != -1) && (String(file.name()).indexOf("config.txt") == -1))
      {
        String filename = String(file.name());
        filename.replace(".txt", "");
        profileListBuff[profileCounter] = filename;
        profileCounter++;
      }

      DEBUG_PRINT("  FILE: ");
      DEBUG_PRINT(file.name());
      DEBUG_PRINT("  SIZE: ");
      DEBUG_PRINTLN(file.size());
    }
    file = root.openNextFile();
  }

  profileListCount = profileCounter;
  DEBUG_PRINT("  profileListCount: ");
  DEBUG_PRINTLN(profileListCount);
  for (int i = 0; i < profileListCount; i++)
  {
    DEBUG_PRINTLN(profileListBuff[i]);
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
  doc["weight"] = serialized(String(config.weight, 3));
  doc["log_Measurements"] = config.log_measurements;
  doc["microsteps"] = config.microsteps;
  doc["beeper"] = config.beeper;
  doc["debug_log"] = config.debugLog;
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
}
