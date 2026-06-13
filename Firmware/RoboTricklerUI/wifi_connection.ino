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
static String wifiSetupApPassword;
static bool wifiEventLoggingRegistered = false;
static IPAddress wifiConfiguredDNS = IPAddress(8, 8, 8, 8);
static bool wifiUsesStaticIp = false;
static unsigned long wifiConnectStartedMillis = 0;
static bool wifiConnectTimeoutReported = false;
static wl_status_t wifiLastLoggedStatus = WL_NO_SHIELD;

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

  if (WIFI_SETUP_AP_ACTIVE)
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
    if (!WEB_SERVER_ACTIVE)
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

void initWebServer()
{
  wifiActive = false;
  WIFI_SETUP_AP_ACTIVE = false;
  WEB_SERVER_ACTIVE = false;
  String configuredSsid(config.wifi_ssid);
  configuredSsid.trim();
  if (configuredSsid.length() <= 0)
  {
    startWifiSetupAccessPoint();
    return;
  }

  if (configuredSsid.length() > 0)
  {
    updateDisplayLog(langText("status_connect_wifi"));
    updateDisplayLog(config.wifi_ssid);

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
    WiFi.setHostname(host);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    IPAddress ipDNS(8, 8, 8, 8);
    IPAddress parsedDNS;
    if (parseConfigIPAddress("IPDNS", config.IPDns, parsedDNS))
    {
      ipDNS = parsedDNS;
    }
    else if (configStringHasValue(config.IPDns))
    {
      updateDisplayLog(langText("status_dns_failed"));
    }

    bool useStaticIp = false;
    if (configStringHasValue(config.IPStatic))
    {
      IPAddress staticIP;
      IPAddress gateway;
      IPAddress subnet;
      bool staticIpConfigValid = parseConfigIPAddress("IPStatic", config.IPStatic, staticIP) &&
                                 parseConfigIPAddress("IPGateway", config.IPGateway, gateway) &&
                                 parseConfigIPAddress("IPSubnet", config.IPSubnet, subnet);

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
    WiFi.begin(config.wifi_ssid, config.wifi_psk);
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
  wifiSetupApPassword = WiFi.macAddress();
  wifiSetupApPassword.replace(":", "");

  if (!WiFi.softAPConfig(WIFI_SETUP_AP_IP, WIFI_SETUP_AP_IP, WIFI_SETUP_AP_SUBNET))
  {
    DEBUG_PRINTLN("WiFi setup AP IP configuration failed.");
    return false;
  }
  if (!WiFi.softAP(WIFI_SETUP_AP_SSID, wifiSetupApPassword.c_str()))
  {
    DEBUG_PRINTLN("WiFi setup AP creation failed.");
    return false;
  }

  WIFI_SETUP_AP_ACTIVE = true;
  wifiActive = true;
  registerWebServerRoutes();
  server.begin();
  WEB_SERVER_ACTIVE = true;

  if (!dnsServer.start(53, "*", WIFI_SETUP_AP_IP))
  {
    DEBUG_PRINTLN("Captive portal DNS server failed to start.");
  }

  updateDisplayLog(String("WiFi AP: ") + WIFI_SETUP_AP_SSID);
  updateDisplayLog(String("WiFi password: ") + wifiSetupApPassword);
  updateDisplayLog(String("Open http://") + WIFI_SETUP_AP_IP.toString());
  showWifiSetupInfo();
  return true;
}

void showWifiSetupInfo()
{
  String info = "WiFi setup mode\n\n";
  info += "Connect to:\n";
  info += WIFI_SETUP_AP_SSID;
  info += "\nPassword:\n";
  info += wifiSetupApPassword;
  info += "\n\nOpen:\nhttp://";
  info += WIFI_SETUP_AP_IP.toString();
  info += "\n\nSelect your WiFi and save.";
  setLabelText(ui_LabelInfo, info.c_str());
}
