String sdReadError = "";

void setSdReadError(const String &message)
{
  sdReadError = message;
}

String getSdReadError()
{
  return sdReadError;
}

static bool readJsonObjectFile(const char *filename, JsonDocument &doc, const char *label, bool showErrors)
{
  File file = SD.open(filename);
  if (!file)
  {
    if (showErrors)
    {
      setSdReadError(String("Could not open ") + label + " file:\n" + filename);
      DEBUG_PRINT("Failed to open ");
      DEBUG_PRINT(label);
      DEBUG_PRINT(": ");
      DEBUG_PRINTLN(filename);
    }
    return false;
  }

  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error || !doc.is<JsonObject>())
  {
    if (showErrors)
    {
      String errorText = error ? String(error.c_str()) : String("Root is not a JSON object");
      setSdReadError(String(label) + " JSON parse failed:\n" + filename + "\n" + errorText);
      DEBUG_PRINT("deserializeJson() failed on ");
      DEBUG_PRINT(label);
      DEBUG_PRINT(": ");
      DEBUG_PRINTLN(errorText);
    }
    return false;
  }

  return true;
}

#define LOAD_CONFIG_TEXT(target, value) strlcpy(target, (value) | target, sizeof(target))

void setDefaultConfiguration(Config &config)
{
  struct TextDefault
  {
    char *target;
    size_t size;
    const char *value;
  };

  const TextDefault defaults[] = {
      {config.wifi_ssid, sizeof(config.wifi_ssid), ""},
      {config.wifi_psk, sizeof(config.wifi_psk), ""},
      {config.IPStatic, sizeof(config.IPStatic), ""},
      {config.IPGateway, sizeof(config.IPGateway), ""},
      {config.IPSubnet, sizeof(config.IPSubnet), ""},
      {config.IPDns, sizeof(config.IPDns), ""},
      {config.scale_protocol, sizeof(config.scale_protocol), "GG"},
      {config.scale_customCode, sizeof(config.scale_customCode), ""},
      {config.profile, sizeof(config.profile), "calibrate"},
      {config.beeper, sizeof(config.beeper), "done"},
      {config.language, sizeof(config.language), "en"},
      {config.fwUpdateUrl, sizeof(config.fwUpdateUrl), DEFAULT_FW_UPDATE_URL},
  };

  for (size_t i = 0; i < ARRAY_COUNT(defaults); i++)
  {
    strlcpy(defaults[i].target, defaults[i].value, defaults[i].size);
  }
  config.scale_baud = 9600;
  config.targetWeight = 40.0;
  config.fwCheck = true;
}

// Loads the configuration from a file
bool loadConfiguration(const char *filename, Config &config)
{
  setSdReadError("");
  setDefaultConfiguration(config);
  // Dump config file
  printFile(filename);

  JsonDocument doc;
  if (!readJsonObjectFile(filename, doc, "Config", true))
  {
    return false;
  }

  LOAD_CONFIG_TEXT(config.wifi_ssid, doc["wifi"]["ssid"]);
  LOAD_CONFIG_TEXT(config.wifi_psk, doc["wifi"]["psk"]);
  LOAD_CONFIG_TEXT(config.IPStatic, doc["wifi"]["IPStatic"]);
  LOAD_CONFIG_TEXT(config.IPGateway, doc["wifi"]["IPGateway"]);
  LOAD_CONFIG_TEXT(config.IPSubnet, doc["wifi"]["IPSubnet"]);
  LOAD_CONFIG_TEXT(config.IPDns, doc["wifi"]["IPDNS"]);
  LOAD_CONFIG_TEXT(config.scale_protocol, doc["scale"]["protocol"]);
  LOAD_CONFIG_TEXT(config.scale_customCode, doc["scale"]["customCode"]);
  config.scale_baud = doc["scale"]["baud"] | config.scale_baud;
  LOAD_CONFIG_TEXT(config.profile, doc["profile"]);
  LOAD_CONFIG_TEXT(config.beeper, doc["beeper"]);
  LOAD_CONFIG_TEXT(config.language, doc["language"]);

  JsonObject fwUpdate = doc["fw_update"].as<JsonObject>();
  config.fwCheck = fwUpdate["check"] | (doc["fw_check"] | config.fwCheck);
  LOAD_CONFIG_TEXT(config.fwUpdateUrl, fwUpdate["url"]);

  return true;
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
  doc["beeper"] = config.beeper;
  doc["language"] = config.language;
  doc["fw_update"]["check"] = config.fwCheck;
  doc["fw_update"]["url"] = config.fwUpdateUrl;

  // Serialize JSON to file
  if (serializeJsonPretty(doc, file) == 0)
  {
    DEBUG_PRINTLN("Failed to write to file");
  }

  // Close the file
  file.close();
}
