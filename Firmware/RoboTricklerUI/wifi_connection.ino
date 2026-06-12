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
  WEB_SERVER_ACTIVE = false;
  if (String(config.wifi_ssid).length() > 0)
  {
    updateDisplayLog(langText("status_connect_wifi"));
    updateDisplayLog(config.wifi_ssid);

    #if DEBUG
      registerWifiDebugLogging();
    #endif
    // Fully reset WiFi before entering STA mode. This avoids ESP32 reconnect
    // edge cases seen when the radio was already active during startup.
    WiFi.disconnect();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);

    delay(500);

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
