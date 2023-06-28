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

  for (JsonPair item : doc.as<JsonObject>()) {
    int item_key = ((String(item.key().c_str()).toInt()) - 1);
    config.trickler_weight[item_key] = item.value()["weight"];
    config.trickler_steps[item_key] = item.value()["steps"];
    config.trickler_speed[item_key] = item.value()["speed"];
    config.trickler_measurements[item_key] = item.value()["measurements"];
    config.trickler_count = item_key + 1;
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

  strlcpy(config.scale_protocol,                  // <- destination
          doc["scale"]["protocol"] | "",  // <- source
          sizeof(config.scale_protocol));         // <- destination's capacity

  config.arduino_ota = doc["arduino_ota"] | false;

  config.scale_baud = doc["scale"]["baud"] | 9600;

  strlcpy(config.powder,                  // <- destination
          doc["powder"] | "",  // <- source
          sizeof(config.powder));         // <- destination's capacity

  if (doc["mode"] == "trickler") {
    config.mode = trickler;
  } else if (doc["mode"] == "logger") {
    config.mode = logger;
    config.log_measurements = doc["log_measurements"] | 20;
  } else {
    config.mode = trickler;
  }

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
