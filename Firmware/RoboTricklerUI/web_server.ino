const char *host = "robo-trickler";

void handleReboot()
{
  sendHtmlBackPage("Reboot now.");
  ESP.restart();
}

void handleGetWeight()
{
  server.send(200, "text/plain", String(weight, 3));
}

void handleGetTarget()
{
  server.send(200, "text/plain", String(config.targetWeight, 3));
}

void handleSetTarget()
{
  if (server.hasArg("targetWeight"))
  {
    float targetWeight = server.arg("targetWeight").toFloat();
    if ((targetWeight > 0) && (targetWeight < MAX_TARGET_WEIGHT) && (config.targetWeight != targetWeight))
    {
      saveTargetWeight(targetWeight);
    }
  }
  sendHtmlBackPage("Value Set.");
}

void handleSetProfile()
{
  if (server.hasArg("profileNumber"))
  {
    int profileNumber = server.arg("profileNumber").toInt();
    if (profileNumber >= 0)
    {
      selectProfile(profileNumber);
    }
  }
  sendHtmlBackPage("Value Set.");
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
  String message = "{";
  for (int i = 0; i < profileListCount; i++)
  {
    if (i > 0)
    {
      message += ",";
    }
    message += "\"";
    message += String(i);
    message += "\":\"";
    message += jsonEscape(String(profileListBuff[i]));
    message += "\"";
  }
  message += "}";

  server.send(200, "text/json", message);
}

void handleStart()
{
  startTrickler();
  sendHtmlBackPage("Running...");
}

void handleStop()
{
  stopTrickler();
  sendHtmlBackPage("Stopped...");
}

void handleWifiReconnect()
{
  if (!wifiActive || running)
  {
    return;
  }

  if ((WiFi.status() != WL_CONNECTED) && (millis() - wifiPreviousMillis >= wifiInterval))
  {
    updateDisplayLog(languageText("status_reconnecting_wifi"));
    WiFi.disconnect();
    WiFi.reconnect();
    wifiPreviousMillis = millis();
  }
}

void handleWebServerClient()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    server.handleClient();
  }
}

IPAddress stringToIPAddress(const String &ipAddressString)
{
  IPAddress ipAddress;
  if (!ipAddress.fromString(ipAddressString))
  {
    DEBUG_PRINTLN("Invalid IP Address format.");
  }

  return ipAddress;
}

static void prepareWifiForStationMode()
{
  // Reset WiFi state before STA mode; this avoids crashes on some reconnect paths.
  WiFi.disconnect();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(WIFI_RESET_DELAY_MS);
}

static IPAddress configuredDnsAddress()
{
  if (String(config.IPDns).length() > 0)
  {
    return stringToIPAddress(String(config.IPDns));
  }
  return IPAddress(8, 8, 8, 8);
}

static void applyWifiIpConfig(IPAddress ipDNS)
{
  if (String(config.IPStatic).length() > 0)
  {
    IPAddress staticIP = stringToIPAddress(String(config.IPStatic));
    IPAddress gateway = stringToIPAddress(String(config.IPGateway));
    IPAddress subnet = stringToIPAddress(String(config.IPSubnet));

    if (!WiFi.config(staticIP, gateway, subnet, ipDNS))
    {
      updateDisplayLog(languageText("status_static_ip_failed"));
      delay(WIFI_CONFIG_ERROR_DELAY_MS);
    }
    return;
  }

  if (!WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, ipDNS))
  {
    updateDisplayLog(languageText("status_dns_failed"));
    delay(WIFI_CONFIG_ERROR_DELAY_MS);
  }
}

static void registerWebRoutes()
{
  server.on("/list", HTTP_GET, handleListDirectory);
  server.on("/system/resources/edit", HTTP_DELETE, handleDelete);
  server.on("/system/resources/edit", HTTP_PUT, handleCreate);
  server.on("/system/resources/edit", HTTP_POST, []()
            { sendOkResponse(); }, handleFileUpload);
  server.onNotFound(handleNotFound);
  server.on("/generate_204", handleNotFound);
  server.on("/favicon.ico", handleNotFound);
  server.on("/fwlink", handleNotFound);
  server.on("/reboot", handleReboot);
  server.on("/getWeight", handleGetWeight);
  server.on("/setProfile", handleSetProfile);
  server.on("/getProfile", handleGetProfile);
  server.on("/getLanguage", handleGetLanguage);
  server.on("/getProfileList", handleGetProfileList);
  server.on("/getTarget", handleGetTarget);
  server.on("/setTarget", handleSetTarget);
  server.on("/system/start", handleStart);
  server.on("/system/stop", handleStop);
  server.on("/fwupdate", HTTP_GET, handleFirmwareUpdatePage);
  server.on("/update", HTTP_POST, handleFirmwareUpdatePostResult, handleFirmwareUpdateUpload);
}

void initWebServer()
{
  wifiActive = false;
  if (String(config.wifi_ssid).length() <= 0)
  {
    return;
  }

  updateDisplayLog(languageText("status_connect_wifi"));
  updateDisplayLog(config.wifi_ssid);

  prepareWifiForStationMode();
  applyWifiIpConfig(configuredDnsAddress());

  WiFi.mode(WIFI_STA);
  WiFi.begin(config.wifi_ssid, config.wifi_psk);

  String message = String(languageText("status_connect_wifi")) +
                   String(config.wifi_ssid) +
                   languageText("msg_connect_wifi_wait");
  messageBox(message, &lv_font_montserrat_14, lv_color_hex(UI_COLOR_TEXT), false);

  if (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    updateDisplayLog(languageText("status_no_wifi"));
    messageBox(languageText("status_no_wifi"), &lv_font_montserrat_14, lv_color_hex(UI_COLOR_WARN), true);
    return;
  }

  updateDisplayLog(languageText("status_wifi_connected"));
  MDNS.begin(host);

  registerWebRoutes();
  server.begin();
  MDNS.addService("http", "tcp", 80);

  updateDisplayLog(String(languageText("status_open_browser_prefix")) + host + languageText("status_open_browser_suffix"));

  if (config.fwCheck)
  {
    checkFirmwareUpdate();
  }

  wifiActive = true;
  if (lvglLock())
  {
    lv_obj_add_flag(ui_PanelMessages, LV_OBJ_FLAG_HIDDEN);
    lvglUnlock();
  }
}
