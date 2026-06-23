IPAddress stringToIPAddress(const String &ipAddressString)
{
  IPAddress ipAddress;
  if (!ipAddress.fromString(ipAddressString))
  {
    DEBUG_PRINTLN("Invalid IP Address format.");
  }

  return ipAddress;
}

bool configStringHasValue(const char *value)
{
  String text(value);
  text.trim();
  return text.length() > 0;
}

bool parseConfigIPAddress(const char *name, const char *value, IPAddress &ipAddress)
{
  String text(value);
  text.trim();
  if (text.length() <= 0)
  {
    return false;
  }

  if (!ipAddress.fromString(text))
  {
    DEBUG_PRINT("Invalid ");
    DEBUG_PRINT(name);
    DEBUG_PRINT(": ");
    DEBUG_PRINTLN(text);
    return false;
  }

  return true;
}

static const unsigned long WIFI_CONNECT_TIMEOUT_MS = 30000;
static const char *WIFI_SETUP_AP_SSID = "Robo-Trickler-AP";
static const IPAddress WIFI_SETUP_AP_IP(192, 168, 4, 1);
static const IPAddress WIFI_SETUP_AP_SUBNET(255, 255, 255, 0);
static char wifiSetupApPassword[13];
static bool wifiEventLoggingRegistered = false;
static IPAddress wifiConfiguredDNS = IPAddress(8, 8, 8, 8);
static bool wifiUsesStaticIp = false;
static unsigned long wifiConnectStartedMillis = 0;
static bool wifiConnectTimeoutReported = false;
static wl_status_t wifiLastLoggedStatus = WL_NO_SHIELD;

bool createWifiSetupPassword()
{
  uint8_t mac[6] = {0};
  if (esp_efuse_mac_get_default(mac) != ESP_OK)
  {
    return false;
  }

  bool hasNonZeroByte = false;
  for (uint8_t value : mac)
  {
    hasNonZeroByte = hasNonZeroByte || (value != 0);
  }
  if (!hasNonZeroByte)
  {
    return false;
  }

  snprintf(wifiSetupApPassword, sizeof(wifiSetupApPassword), "%02X%02X%02X%02X%02X%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return true;
}

const char *wifiStatusName(wl_status_t status)
{
  switch (status)
  {
  case WL_IDLE_STATUS:
    return "WL_IDLE_STATUS";
  case WL_NO_SSID_AVAIL:
    return "WL_NO_SSID_AVAIL";
  case WL_SCAN_COMPLETED:
    return "WL_SCAN_COMPLETED";
  case WL_CONNECTED:
    return "WL_CONNECTED";
  case WL_CONNECT_FAILED:
    return "WL_CONNECT_FAILED";
  case WL_CONNECTION_LOST:
    return "WL_CONNECTION_LOST";
  case WL_DISCONNECTED:
    return "WL_DISCONNECTED";
  case WL_STOPPED:
    return "WL_STOPPED";
  case WL_NO_SHIELD:
    return "WL_NO_SHIELD";
  default:
    return "WL_UNKNOWN";
  }
}

void logWifiDisconnectReason(WiFiEvent_t event, WiFiEventInfo_t info)
{
  if (event != ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
  {
    return;
  }

#if DEBUG
  uint8_t reason = info.wifi_sta_disconnected.reason;
  DEBUG_PRINT("WiFi disconnected, reason ");
  DEBUG_PRINT(reason);
  DEBUG_PRINT(" (");
  DEBUG_PRINT(WiFi.disconnectReasonName((wifi_err_reason_t)reason));
  DEBUG_PRINTLN(")");
#else
  (void)info;
#endif
}

void registerWifiDebugLogging()
{
  if (!wifiEventLoggingRegistered)
  {
    WiFi.onEvent(logWifiDisconnectReason, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    wifiEventLoggingRegistered = true;
  }
}

void logWifiStatusChange()
{
  wl_status_t status = WiFi.status();
  if (status != wifiLastLoggedStatus)
  {
    DEBUG_PRINT("WiFi status: ");
    DEBUG_PRINTLN(wifiStatusName(status));
    wifiLastLoggedStatus = status;
  }
}

void applyWifiDnsIfNeeded()
{
  if (!wifiUsesStaticIp && !WiFi.setDNS(wifiConfiguredDNS))
  {
    DEBUG_PRINTLN("WiFi DNS configuration failed.");
  }
}

void maintainWifiConnection()
{
  updateWifiTabIndicator(false);

  if (wifiSetupApActive)
  {
    return;
  }

  if (!wifiActive || isTricklerRunning())
  {
    return;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    wifiConnectTimeoutReported = false;
    wifiLastLoggedStatus = WL_CONNECTED;
    if (!webServerActive)
    {
      startWebServerServices();
    }
    return;
  }

  logWifiStatusChange();
  if (!wifiConnectTimeoutReported && ((millis() - wifiConnectStartedMillis) >= WIFI_CONNECT_TIMEOUT_MS))
  {
    updateDisplayLog(langText("status_no_wifi"));
    wifiConnectTimeoutReported = true;
    startWifiSetupAccessPoint();
    return;
  }

  if (millis() - wifiPreviousMillis >= wifiInterval)
  {
    updateDisplayLog(langText("status_reconnecting_wifi"));
    if (!WiFi.reconnect())
    {
      DEBUG_PRINTLN("WiFi reconnect failed to start.");
    }
    wifiPreviousMillis = millis();
  }
}

void updateWifiButtonState()
{
  if ((ui_ButtonWifi == NULL) || !lvglLock())
  {
    return;
  }

  if (config.wifiEnabled)
  {
    lv_obj_add_state(ui_ButtonWifi, LV_STATE_CHECKED);
  }
  else
  {
    lv_obj_remove_state(ui_ButtonWifi, LV_STATE_CHECKED);
  }
  lvglUnlock();
}

void stopWifiServices()
{
  dnsServer.stop();
  server.stop();
  MDNS.end();
  webServerActive = false;
  wifiSetupApActive = false;
  wifiActive = false;
  WiFi.setAutoReconnect(false);

  wifi_mode_t currentMode = WiFi.getMode();
  if ((currentMode & WIFI_MODE_AP) != 0)
  {
    WiFi.softAPdisconnect(true);
  }
  if ((currentMode & WIFI_MODE_STA) != 0)
  {
    WiFi.disconnect(true, false);
  }
  else if (currentMode != WIFI_MODE_NULL)
  {
    WiFi.mode(WIFI_OFF);
  }

  updateWifiSetupQrCode();
  updateWifiTabIndicator(true);
}

void applyWifiEnabled()
{
  updateWifiButtonState();
  if (config.wifiEnabled)
  {
    initWebServer();
  }
  else
  {
    stopWifiServices();
  }
}

void initWebServer()
{
  wifiActive = false;
  wifiSetupApActive = false;
  webServerActive = false;
  updateWifiButtonState();
  if (!config.wifiEnabled)
  {
    stopWifiServices();
    return;
  }

  String configuredSsid(config.wifiSsid);
  configuredSsid.trim();
  if (configuredSsid.length() <= 0)
  {
    startWifiSetupAccessPoint();
    return;
  }

  if (configuredSsid.length() > 0)
  {
    updateDisplayLog(langText("status_connect_wifi"));
    updateDisplayLog(config.wifiSsid);

    #if DEBUG
      registerWifiDebugLogging();
    #endif
    // Only reset interfaces that are already running. esp_wifi_disconnect()
    // reports ESP_ERR_WIFI_NOT_INIT when called during a normal cold start.
    wifi_mode_t currentMode = WiFi.getMode();
    if (currentMode != WIFI_MODE_NULL)
    {
      if ((currentMode & WIFI_MODE_STA) != 0)
      {
        WiFi.disconnect();
      }
      if ((currentMode & WIFI_MODE_AP) != 0)
      {
        WiFi.softAPdisconnect(false);
      }
      WiFi.mode(WIFI_OFF);
      delay(100);
    }

    WiFi.persistent(false);
    WiFi.setHostname(MDNS_HOST);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    IPAddress ipDNS(8, 8, 8, 8);
    IPAddress parsedDNS;
    if (parseConfigIPAddress("ipDns", config.wifiIpDns, parsedDNS))
    {
      ipDNS = parsedDNS;
    }
    else if (configStringHasValue(config.wifiIpDns))
    {
      updateDisplayLog(langText("status_dns_failed"));
    }

    bool useStaticIp = false;
    if (configStringHasValue(config.wifiIpStatic))
    {
      IPAddress staticIP;
      IPAddress gateway;
      IPAddress subnet;
      bool staticIpConfigValid = parseConfigIPAddress("ipStatic", config.wifiIpStatic, staticIP) &&
                                 parseConfigIPAddress("ipGateway", config.wifiIpGateway, gateway) &&
                                 parseConfigIPAddress("ipSubnet", config.wifiIpSubnet, subnet);

      if (staticIpConfigValid && WiFi.config(staticIP, gateway, subnet, ipDNS))
      {
        useStaticIp = true;
      }
      else
      {
        updateDisplayLog(langText("status_static_ip_failed"));
      }
    }
    if (!useStaticIp)
    {
      if (!WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE))
      {
        updateDisplayLog(langText("status_dns_failed"));
      }
    }

    wifiConfiguredDNS = ipDNS;
    wifiUsesStaticIp = useStaticIp;
    WiFi.begin(config.wifiSsid, config.wifiPsk);
    WiFi.setSleep(false);
    wifiActive = true;
    wifiPreviousMillis = millis();
    wifiConnectStartedMillis = wifiPreviousMillis;
    wifiConnectTimeoutReported = false;
    wifiLastLoggedStatus = WL_NO_SHIELD;
  }
}

bool startWifiSetupAccessPoint()
{
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  if (!createWifiSetupPassword())
  {
    DEBUG_PRINTLN("Could not read hardware MAC for setup AP password.");
    return false;
  }

  if (!WiFi.softAPConfig(WIFI_SETUP_AP_IP, WIFI_SETUP_AP_IP, WIFI_SETUP_AP_SUBNET))
  {
    DEBUG_PRINTLN("WiFi setup AP IP configuration failed.");
    return false;
  }
  if (!WiFi.softAP(WIFI_SETUP_AP_SSID, wifiSetupApPassword))
  {
    DEBUG_PRINTLN("WiFi setup AP creation failed.");
    return false;
  }

  wifiSetupApActive = true;
  wifiActive = true;
  registerWebServerRoutes();
  server.begin();
  webServerActive = true;

  if (!dnsServer.start(53, "*", WIFI_SETUP_AP_IP))
  {
    DEBUG_PRINTLN("Captive portal DNS server failed to start.");
  }

  char logLine[96];
  snprintf(logLine, sizeof(logLine), "%s%s", langText("status_wifi_ap"), WIFI_SETUP_AP_SSID);
  updateDisplayLog(logLine);
  snprintf(logLine, sizeof(logLine), "%s%s", langText("status_wifi_password"), wifiSetupApPassword);
  updateDisplayLog(logLine);
  snprintf(logLine, sizeof(logLine), "%s%u.%u.%u.%u", langText("status_wifi_open"),
           WIFI_SETUP_AP_IP[0], WIFI_SETUP_AP_IP[1], WIFI_SETUP_AP_IP[2], WIFI_SETUP_AP_IP[3]);
  updateDisplayLog(logLine);
  if (!generateWifiSetupQrCode(WIFI_SETUP_AP_SSID, wifiSetupApPassword))
  {
    DEBUG_PRINTLN("WiFi setup QR code generation failed.");
  }
  showWifiSetupInfo();
  return true;
}

void showWifiSetupInfo()
{
  char info[320];
  snprintf(info, sizeof(info),
           "%s\n\n%s\n%s\n%s\n%s\n\n%s\nhttp://%u.%u.%u.%u\n\n%s",
           langText("wifi_setup_mode"),
           langText("wifi_connect_to"),
           WIFI_SETUP_AP_SSID,
           langText("wifi_password"),
           wifiSetupApPassword,
           langText("wifi_open"),
           WIFI_SETUP_AP_IP[0], WIFI_SETUP_AP_IP[1], WIFI_SETUP_AP_IP[2], WIFI_SETUP_AP_IP[3],
           langText("wifi_select_and_save"));
  setLabelText(ui_LabelInfo, info);
}
