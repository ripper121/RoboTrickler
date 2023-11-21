bool readPowder(const char *filename, Config &config) {
  // Dump config file
  printFile(filename);

  // Open file for reading
  File file = SD.open(filename);

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use https://arduinojson.org/v6/assistant to compute the capacity.
  StaticJsonDocument<2048> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, file);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return false;
  }

  if (filename == "PID.txt" || filename == "pid.txt") {
    config.pidThreshold = doc["threshold"] | 0.10;
    config.pidStepMin = doc["stepMin"] | 5;
    config.pidStepMax = doc["stepMax"] | 36000;
    config.pidOscillate = doc["oscillate"] | false;
    config.pidReverse = doc["reverse"] | false;
    config.pidConsKp = doc["consKp"] | 1.00;
    config.pidConsKi = doc["consKi"] | 0.05;
    config.pidConsKd = doc["consKd"] | 0.25;
    config.pidAggKp = doc["aggKp"] | 4.00;
    config.pidAggKi = doc["aggKi"] | 0.2;
    config.pidAggKd = doc["aggKd"] | 1.00;
    PID_AKTIVE = true;
  } else {
    for (JsonPair item : doc.as<JsonObject>()) {
      int item_key = ((String(item.key().c_str()).toInt()) - 1);
      config.profile_num[item_key] = item.value()["number"] | 1;
      config.profile_weight[item_key] = item.value()["weight"];
      config.profile_steps[item_key] = item.value()["steps"];
      config.profile_speed[item_key] = item.value()["speed"];
      config.profile_measurements[item_key] = item.value()["measurements"];
      config.profile_oscillate[item_key] = item.value()["oscillate"] | true;
      config.profile_reverse[item_key] = item.value()["reverse"] | false;
      config.profile_count = item_key + 1;
    }
    PID_AKTIVE = false;
  }
  file.close();
  return true;
}

// Loads the configuration from a file
int loadConfiguration(const char *filename, Config &config) {
  // Dump config file
  printFile(filename);

  // Open file for reading
  File file = SD.open(filename);

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use https://arduinojson.org/v6/assistant to compute the capacity.
  StaticJsonDocument<512> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, file);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return 1;
  }

  strlcpy(config.wifi_ssid,                  // <- destination
          doc["wifi"]["ssid"] | "",  // <- source
          sizeof(config.wifi_ssid));         // <- destination's capacity

  strlcpy(config.wifi_psk,                  // <- destination
          doc["wifi"]["psk"] | "",  // <- source
          sizeof(config.wifi_psk));         // <- destination's capacity

  strlcpy(config.IPStatic,                  // <- destination
          doc["wifi"]["IPStatic"] | "",  // <- source
          sizeof(config.IPStatic));         // <- destination's capacity

  strlcpy(config.IPGateway,                  // <- destination
          doc["wifi"]["IPGateway"] | "",  // <- source
          sizeof(config.IPGateway));         // <- destination's capacity

  strlcpy(config.IPSubnet,                  // <- destination
          doc["wifi"]["IPSubnet"] | "",  // <- source
          sizeof(config.IPSubnet));         // <- destination's capacity

  strlcpy(config.scale_protocol,                  // <- destination
          doc["scale"]["protocol"] | "GUG",  // <- source
          sizeof(config.scale_protocol));         // <- destination's capacity

  config.scale_baud = doc["scale"]["baud"] | 9600;

  strlcpy(config.powder,                  // <- destination
          doc["powder"] | "calibrate",  // <- source
          sizeof(config.powder));         // <- destination's capacity



  if (doc["mode"] == "trickler") {
    config.mode = trickler;
  } else if (doc["mode"] == "logger") {
    config.mode = logger;
    config.log_measurements = doc["log_measurements"] | 20;
  } else {
    config.mode = trickler;
  }

  config.microsteps = doc["microsteps"] | 1;

  if (doc["beeper"] == "off") {
    config.beeper = off;
  } else if (doc["beeper"] == "done") {
    config.beeper = done;
  } else if (doc["beeper"] == "button") {
    config.beeper = button;
  } else if (doc["beeper"] == "both") {
    config.beeper = both;
  } else {
    config.beeper = off;
  }

  config.arduino_ota = doc["arduino_ota"] | false;

  config.debugLog = doc["debug_log"] | false;

  file.close();

  String powder_filename = "/" + String(config.powder) + ".txt";
  if (!readPowder(powder_filename.c_str(), config))
    return 2;

  return 0;
}

// Prints the content of a file to the Serial
void printFile(const char *filename) {
  // Open file for reading
  File file = SD.open(filename);

  Serial.println(filename);

  if (!file) {
    Serial.println(F("Failed to read file"));
    return;
  }

  // Extract each characters by one by one
  while (file.available()) {
    Serial.print((char)file.read());
  }
  Serial.println();

  // Close the file
  file.close();
}
