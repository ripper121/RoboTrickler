void handleHomePage()
{
  if (!loadWebFile("/"))
  {
    server.send(404, "text/plain", "Home page not found");
  }
}

void handleWifiSetupPortal()
{
  if (!loadWebFile("/system/ap/index.html"))
  {
    server.send(500, "text/plain", "WiFi setup page not found");
  }
}

void handleWifiScan()
{
  int networkCount = WiFi.scanNetworks(false, true);
  if (networkCount < 0)
  {
    server.send(500, "application/json", "{\"error\":\"WiFi scan failed\"}");
    return;
  }

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  server.sendContent("[");
  for (int i = 0; i < networkCount; i++)
  {
    if (i > 0)
    {
      server.sendContent(",");
    }

    String item = "{\"ssid\":\"";
    item += jsonEscape(WiFi.SSID(i));
    item += "\",\"rssi\":";
    item += String(WiFi.RSSI(i));
    item += ",\"channel\":";
    item += String(WiFi.channel(i));
    item += ",\"secure\":";
    item += WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true";
    item += "}";
    server.sendContent(item);
  }
  server.sendContent("]");
  WiFi.scanDelete();
}

void handleWifiSave()
{
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  ssid.trim();
  if ((ssid.length() <= 0) || (ssid.length() >= sizeof(config.wifiSsid)) ||
      (password.length() >= sizeof(config.wifiPsk)))
  {
    server.send(400, "application/json", "{\"error\":\"invalid SSID or password length\"}");
    return;
  }

  strlcpy(config.wifiSsid, ssid.c_str(), sizeof(config.wifiSsid));
  strlcpy(config.wifiPsk, password.c_str(), sizeof(config.wifiPsk));
  saveConfiguration("/config.txt", config);

  server.send(200, "application/json", "{\"saved\":true,\"rebooting\":true}");
  delay(500);
  ESP.restart();
}

// Firmware-served web pages take their text from the web language store
// (SD-Files/system/lang/<lang>.json, "web.firmware" object) so they match the
// localized static pages. The doc is parsed per request and freed on return, so
// it never sits resident in heap. Use loadWebLang() once per page, then webFwText().
bool loadWebLang(JsonDocument &doc)
{
  String language = String(config.language);
  language.trim();
  language.toLowerCase();
  int separator = language.indexOf('-');
  if (separator < 0)
  {
    separator = language.indexOf('_');
  }
  if (separator > 0)
  {
    language = language.substring(0, separator);
  }
  if (language.length() <= 0)
  {
    language = "en";
  }

  String candidates[] = {
      "/system/lang/" + language + ".json",
      "/system/lang/en.json"};

  for (int i = 0; i < (int)(sizeof(candidates) / sizeof(candidates[0])); i++)
  {
    if (!ACTIVE_FS.exists(candidates[i].c_str()))
    {
      continue;
    }
    File file = ACTIVE_FS.open(candidates[i].c_str());
    if (!file)
    {
      continue;
    }
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (!error && doc["web"]["firmware"].is<JsonObject>())
    {
      return true;
    }
    doc.clear();
  }
  return false;
}

const char *webFwText(JsonDocument &doc, const char *key, const char *fallback)
{
  JsonVariant value = doc["web"]["firmware"][key];
  if (!value.isNull())
  {
    const char *text = value.as<const char *>();
    if ((text != nullptr) && (text[0] != '\0'))
    {
      return text;
    }
  }
  return fallback;
}

String webPageHead(const char *title)
{
  return String("<!DOCTYPE html><html><head><meta charset='UTF-8'>"
                "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                "<link rel='icon' type='image/x-icon' href='/system/resources/favicon.ico'>"
                "<title>") +
         title +
         "</title><link rel='stylesheet' href='/system/resources/style.css'></head>"
         "<body><div class='card'>";
}

String webPageFoot()
{
  return String("</div></body></html>");
}

String webBackButtonHtml(JsonDocument &doc)
{
  return String("<button class='button' onClick='javascript:history.back()'>") + webFwText(doc, "back", "Back") + "</button>";
}

String webStatusPage(const char *messageKey, const char *messageFallback)
{
  JsonDocument doc;
  loadWebLang(doc);
  return webPageHead(webFwText(doc, "statusTitle", "Robo-Trickler")) +
         "<h3>" + webFwText(doc, messageKey, messageFallback) + "</h3><br>" +
         webBackButtonHtml(doc) + webPageFoot();
}

void handleReboot()
{
  server.send(200, "text/html", webStatusPage("rebootNow", "Reboot now."));
  ESP.restart();
}

void handleGetTarget()
{
  server.send(200, "text/plain", String(config.targetWeight, 3));
}

void handleGetTricklerState()
{
  // Polled frequently by the web UI, so build the small fixed-shape response in a
  // stack buffer instead of churning a JsonDocument + String on every request.
  char response[48];
  char weightText[16];
  if (isfinite(weight))
  {
    snprintf(weightText, sizeof(weightText), "%.3f", weight);
  }
  else
  {
    strlcpy(weightText, "null", sizeof(weightText));
  }
  snprintf(response, sizeof(response), "{\"weight\":%s,\"running\":%s}",
           weightText, isTricklerRunning() ? "true" : "false");
  server.send(200, "application/json", response);
}

void handleSetTarget()
{
  // Browser/UI clients pass targetWeight as a query/form argument; the profile
  // file is updated only when the value actually changes.
  for (uint8_t i = 0; i < server.args(); i++)
  {
    if (server.argName(i) == "targetWeight")
    {
      if ((server.arg(i).toFloat() > 0) && (server.arg(i).toFloat() < MAX_TARGET_WEIGHT))
      {
        if (config.targetWeight != server.arg(i).toFloat())
        {
          saveTargetWeight(server.arg(i).toFloat());
        }
      }
    }
  }
  server.send(200, "text/html", webStatusPage("valueSet", "Value set."));
}

void handleSetProfile()
{
  if (isTricklerRunning())
  {
    server.send(409, "text/html", webStatusPage("running", "Running..."));
    return;
  }

  for (uint8_t i = 0; i < server.args(); i++)
  {
    if (server.argName(i) == "profileNumber")
    {
      if (server.arg(i).toFloat() >= 0)
      {
        setProfile(server.arg(i).toInt());
      }
    }
  }
  server.send(200, "text/html", webStatusPage("valueSet", "Value set."));
}

void handleGetProfile()
{
  server.send(200, "text/plain", String(config.profileName));
}

void handleGetLanguage()
{
  server.send(200, "text/plain", String(config.language));
}

void handleGetProfileList()
{
  // Keep this legacy object shape for the filesystem-hosted pages: {"0":"name", ...}.
  // Stream the entries instead of concatenating one growing String in heap.
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/json", "");
  server.sendContent("{");
  for (int i = 0; i < profileListCount; i++)
  {
    char prefix[16];
    snprintf(prefix, sizeof(prefix), "%s\"%d\":\"", (i > 0) ? "," : "", i);
    server.sendContent(prefix);
    server.sendContent(jsonEscape(String(profileList[i])));
    server.sendContent("\"");
  }
  server.sendContent("}");
}

void handleStart()
{
  startTrickler();
  bool running = isTricklerRunning();
  server.send(running ? 200 : 409, "text/html",
              webStatusPage(running ? "running" : "invalidProfile",
                            running ? "Running..." : "Invalid profile!"));
}
void handleStop()
{
  stopTrickler();
  server.send(200, "text/html", webStatusPage("stopped", "Stopped..."));
}

