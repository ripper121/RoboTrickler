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
  if ((ssid.length() <= 0) || (ssid.length() >= sizeof(config.wifi_ssid)) ||
      (password.length() >= sizeof(config.wifi_psk)))
  {
    server.send(400, "application/json", "{\"error\":\"invalid SSID or password length\"}");
    return;
  }

  strlcpy(config.wifi_ssid, ssid.c_str(), sizeof(config.wifi_ssid));
  strlcpy(config.wifi_psk, password.c_str(), sizeof(config.wifi_psk));
  saveConfiguration("/config.txt", config);

  server.send(200, "application/json", "{\"saved\":true,\"rebooting\":true}");
  delay(500);
  ESP.restart();
}

String webBackButtonHtml()
{
  return String("<button onClick='javascript:history.back()'>") + langText("web_back") + "</button>";
}

String webStatusPage(const char *messageKey)
{
  return String("<h3>") + langText(messageKey) + "</h3><br>" + webBackButtonHtml();
}

void handleReboot()
{
  server.send(200, "text/html", webStatusPage("web_reboot_now"));
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
  snprintf(response, sizeof(response), "{\"weight\":%.3f,\"running\":%s}",
           weight, isTricklerRunning() ? "true" : "false");
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
  server.send(200, "text/html", webStatusPage("web_value_set"));
}

void handleSetProfile()
{
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
  server.send(200, "text/html", webStatusPage("web_value_set"));
}

void handleGetProfile()
{
  server.send(200, "text/plain", String(config.profile));
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
    server.sendContent(jsonEscape(String(profileListBuff[i])));
    server.sendContent("\"");
  }
  server.sendContent("}");
}

void handleStart()
{
  startTrickler();
  server.send(200, "text/html", webStatusPage("web_running"));
}
void handleStop()
{
  stopTrickler();
  server.send(200, "text/html", webStatusPage("web_stopped"));
}

