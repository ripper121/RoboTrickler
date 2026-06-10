const char *host = "robo-trickler";

File uploadFile;
static bool webUpdateStarted = false;
static bool webUpdateSucceeded = false;

void returnOK()
{
  server.send(200, "text/plain", "");
}

void returnFail(String msg)
{
  server.send(500, "text/plain", msg + "\r\n");
}

String jsonEscape(const String &input)
{
  String output;
  output.reserve(input.length() + 4);
  for (uint16_t i = 0; i < input.length(); i++)
  {
    char c = input.charAt(i);
    if ((c == '"') || (c == '\\'))
    {
      output += '\\';
      output += c;
    }
    else if (c == '\n')
    {
      output += "\\n";
    }
    else if (c == '\r')
    {
      output += "\\r";
    }
    else if (c == '\t')
    {
      output += "\\t";
    }
    else if ((uint8_t)c < 0x20)
    {
      char escaped[7];
      snprintf(escaped, sizeof(escaped), "\\u%04x", (unsigned char)c);
      output += escaped;
    }
    else
    {
      output += c;
    }
  }
  return output;
}

bool loadFromSdCard(String path)
{
  String dataType = "text/plain";
  if (path.endsWith("/"))
  {
    path += "system/index.html";
  }

  if (path.endsWith(".src"))
  {
    path = path.substring(0, path.lastIndexOf("."));
  }
  else if (path.endsWith(".html") || path.endsWith(".htm"))
  {
    dataType = "text/html";
  }
  else if (path.endsWith(".css"))
  {
    dataType = "text/css";
  }
  else if (path.endsWith(".js"))
  {
    dataType = "application/javascript";
  }
  else if (path.endsWith(".json"))
  {
    dataType = "application/json";
  }
  else if (path.endsWith(".png"))
  {
    dataType = "image/png";
  }
  else if (path.endsWith(".gif"))
  {
    dataType = "image/gif";
  }
  else if (path.endsWith(".jpg"))
  {
    dataType = "image/jpeg";
  }
  else if (path.endsWith(".ico"))
  {
    dataType = "image/x-icon";
  }
  else if (path.endsWith(".xml"))
  {
    dataType = "text/xml";
  }
  else if (path.endsWith(".pdf"))
  {
    dataType = "application/pdf";
  }
  else if (path.endsWith(".zip"))
  {
    dataType = "application/zip";
  }
  else if (path.endsWith(".gz"))
  {
    dataType = "application/x-gzip";
  }
  else
  {
    dataType = "text/plain";
  }

  if (server.hasArg("download"))
  {
    dataType = "application/octet-stream";
  }

  File dataFile = SD.open(path.c_str());
  if (dataFile.isDirectory())
  {
    path += "/index.html";
    dataType = "text/html";
  }
  dataFile.close();

  String pathWithGz = path + ".gz";
  if (SD.exists(pathWithGz))
  {
    path = pathWithGz;
  }
  else if (!SD.exists(path))
  {
    return false;
  }

  dataFile = SD.open(path.c_str());
  if (!dataFile)
  {
    return false;
  }

  if (dataFile.isDirectory())
  {
    dataFile.close();
    return false;
  }

  DEBUG_PRINTLN(String("\tSent file: ") + path);
  if (server.streamFile(dataFile, dataType) != dataFile.size())
  {
    DEBUG_PRINTLN("Sent less data than expected!");
  }
  dataFile.close();
  return true;
}

void handleFileUpload()
{
  if (server.uri() != "/system/resources/edit")
  {
    return;
  }
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START)
  {
    if (upload.filename.startsWith("/profiles/") && !SD.exists("/profiles"))
    {
      SD.mkdir("/profiles");
    }
    if (SD.exists((char *)upload.filename.c_str()))
    {
      SD.remove((char *)upload.filename.c_str());
    }
    uploadFile = SD.open(upload.filename.c_str(), FILE_WRITE);
    DEBUG_PRINT("Upload: START, filename: ");
    DEBUG_PRINTLN(upload.filename);
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    if (uploadFile)
    {
      uploadFile.write(upload.buf, upload.currentSize);
    }
    DEBUG_PRINT("Upload: WRITE, Bytes: ");
    DEBUG_PRINTLN(upload.currentSize);
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (uploadFile)
    {
      uploadFile.close();
    }
    DEBUG_PRINT("Upload: END, Size: ");
    DEBUG_PRINTLN(upload.totalSize);
  }
}

void deleteRecursive(String path)
{
  File file = SD.open((char *)path.c_str());
  if (!file.isDirectory())
  {
    file.close();
    SD.remove((char *)path.c_str());
    return;
  }

  file.rewindDirectory();
  while (true)
  {
    File entry = file.openNextFile();
    if (!entry)
    {
      break;
    }
    String entryPath = path + "/" + entry.name();
    if (entry.isDirectory())
    {
      entry.close();
      deleteRecursive(entryPath);
    }
    else
    {
      entry.close();
      SD.remove((char *)entryPath.c_str());
    }
    delay(1);
  }

  SD.rmdir((char *)path.c_str());
  file.close();
}

void handleDelete()
{
  if (server.args() == 0)
  {
    return returnFail("BAD ARGS");
  }
  String path = server.arg(0);
  if (path == "/" || !SD.exists((char *)path.c_str()))
  {
    returnFail("BAD PATH");
    return;
  }
  deleteRecursive(path);
  returnOK();
}

void handleCreate()
{
  if (server.args() == 0)
  {
    return returnFail("BAD ARGS");
  }
  String path = server.arg(0);
  if (path == "/" || SD.exists((char *)path.c_str()))
  {
    returnFail("BAD PATH");
    return;
  }

  if (path.indexOf('.') > 0)
  {
    File file = SD.open((char *)path.c_str(), FILE_WRITE);
    if (file)
    {
      file.write(0);
      file.close();
    }
  }
  else
  {
    SD.mkdir((char *)path.c_str());
  }
  returnOK();
}

void printDirectory()
{
  if (!server.hasArg("dir"))
  {
    return returnFail("BAD ARGS");
  }
  String path = server.arg("dir");
  if (path != "/" && !SD.exists((char *)path.c_str()))
  {
    return returnFail("BAD PATH");
  }
  File dir = SD.open((char *)path.c_str());
  path = String();
  if (!dir.isDirectory())
  {
    dir.close();
    return returnFail("NOT DIR");
  }
  dir.rewindDirectory();
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/json", "");
  bool firstEntry = true;

  server.sendContent("[");
  while (true)
  {
    File entry = dir.openNextFile();
    if (!entry)
    {
      break;
    }

    if (strcmp(entry.path(), "/resources/css") != 0)
    {

      String output;
      if (!firstEntry)
      {
        output = ',';
      }
      firstEntry = false;

      output += "{\"type\":\"";
      output += (entry.isDirectory()) ? "dir" : "file";
      output += "\",\"name\":\"";
      output += jsonEscape(String(entry.path()));
      output += "\"";
      output += "}";
      server.sendContent(output);
    }
    entry.close();
  }
  server.sendContent("]");
  dir.close();
}

void handleNotFound()
{
  if (loadFromSdCard(server.uri()))
  {
    return;
  }
  String message = "SDCARD Not Detected\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " NAME:" + server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  DEBUG_PRINT(message);
}

void handleReboot()
{
  server.send(200, "text/html", "<h3>Reboot now.</h3><br><button onClick='javascript:history.back()'>Back</button>");
  ESP.restart();
}

void handleDownloadSdFiles()
{
  bool success = downloadSdFilesTar();
  server.send(success ? 200 : 500, "text/plain", success ? "OK" : "FAIL");
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
  server.send(200, "text/html", "<h3>Value Set.</h3><br><button onClick='javascript:history.back()'>Back</button>");
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
  server.send(200, "text/html", "<h3>Value Set.</h3><br><button onClick='javascript:history.back()'>Back</button>");
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
    message += "\"";
    message += String(i);
    message += "\":\"";
    message += jsonEscape(String(profileListBuff[i]));
    message += "\"";
    if (i < profileListCount - 1)
      message += ",";
  }
  message += "}";

  server.send(200, "text/json", message);
}

void handleStart()
{
  startTrickler();
  server.send(200, "text/html", "<h3>Running...</h3><br><button onClick='javascript:history.back()'>Back</button>");
}
void handleStop()
{
  stopTrickler();
  server.send(200, "text/html", "<h3>Stopped...</h3><br><button onClick='javascript:history.back()'>Back</button>");
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

String firmwareCheckUrl()
{
  String serverPath = DEFAULT_FW_UPDATE_URL;
  if (serverPath == DEFAULT_FW_UPDATE_URL)
  {
    serverPath += "?mac=" + String(WiFi.macAddress()) + "&version=" + String(FW_VERSION);
  }
  return serverPath;
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

  uint8_t reason = info.wifi_sta_disconnected.reason;
  DEBUG_PRINT("WiFi disconnected, reason ");
  DEBUG_PRINT(reason);
  DEBUG_PRINT(" (");
  DEBUG_PRINT(WiFi.disconnectReasonName((wifi_err_reason_t)reason));
  DEBUG_PRINTLN(")");
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

String normalizeFirmwareVersion(String version)
{
  version.trim();
  if ((version.length() > 0) && ((version.charAt(0) == 'v') || (version.charAt(0) == 'V')))
  {
    version.remove(0, 1);
    version.trim();
  }
  return version;
}

bool readFirmwareVersionSegment(const String &version, int &index, unsigned long &segment, bool &hasSegment)
{
  segment = 0;
  hasSegment = false;
  if (index >= version.length())
  {
    return true;
  }

  int digitCount = 0;
  while (index < version.length())
  {
    char c = version.charAt(index);
    if (!isdigit((unsigned char)c))
    {
      break;
    }

    unsigned long digit = (unsigned long)(c - '0');
    if ((segment > 429496729UL) || ((segment == 429496729UL) && (digit > 5)))
    {
      return false;
    }
    segment = (segment * 10UL) + digit;
    digitCount++;
    index++;
  }

  if (digitCount == 0)
  {
    return false;
  }

  hasSegment = true;
  if (index >= version.length())
  {
    return true;
  }

  if (version.charAt(index) != '.')
  {
    return false;
  }

  index++;
  return index < version.length();
}

int compareFirmwareVersions(const String &leftVersion, const String &rightVersion, bool &valid)
{
  String left = normalizeFirmwareVersion(leftVersion);
  String right = normalizeFirmwareVersion(rightVersion);
  valid = (left.length() > 0) && (right.length() > 0);
  if (!valid)
  {
    return 0;
  }

  int leftIndex = 0;
  int rightIndex = 0;
  while ((leftIndex < left.length()) || (rightIndex < right.length()))
  {
    unsigned long leftSegment = 0;
    unsigned long rightSegment = 0;
    bool leftHasSegment = false;
    bool rightHasSegment = false;

    if (!readFirmwareVersionSegment(left, leftIndex, leftSegment, leftHasSegment) ||
        !readFirmwareVersionSegment(right, rightIndex, rightSegment, rightHasSegment))
    {
      valid = false;
      return 0;
    }

    if (leftSegment > rightSegment)
    {
      return 1;
    }
    if (leftSegment < rightSegment)
    {
      return -1;
    }
  }

  return 0;
}

bool isRemoteFirmwareNewer(const String &remoteVersion)
{
  bool valid = false;
  int comparison = compareFirmwareVersions(FW_VERSION, remoteVersion, valid);
  if (!valid)
  {
    DEBUG_PRINT("Invalid firmware version payload: ");
    DEBUG_PRINTLN(remoteVersion);
    return false;
  }
  return comparison < 0;
}

void makeHttpGetRequest(String serverPath)
{
  HTTPClient http;
  http.setTimeout(5000);

  if (http.begin(serverPath))
  {
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0)
    {
      DEBUG_PRINT("HTTP Response code: ");
      DEBUG_PRINTLN(httpResponseCode);
      String payload = http.getString();
      DEBUG_PRINTLN(payload);
      if (isRemoteFirmwareNewer(payload))
      {
        messageBox((String(langText("msg_new_firmware")) + payload + langText("msg_check_url")).c_str(), &lv_font_montserrat_14, lv_color_hex(0x00FF00), true);
      }
    }
    else
    {
      DEBUG_PRINT("Error code: ");
      DEBUG_PRINTLN(httpResponseCode);
      DEBUG_PRINT("HTTP error: ");
      DEBUG_PRINTLN(HTTPClient::errorToString(httpResponseCode));
    }
    http.end();
  }
  else
  {
    DEBUG_PRINTLN("Unable to connect");
  }
}

void registerWebServerRoutes()
{
  if (WEB_SERVER_ROUTES_REGISTERED)
  {
    return;
  }

  server.on("/list", HTTP_GET, printDirectory);
  server.on("/system/resources/edit", HTTP_DELETE, handleDelete);
  server.on("/system/resources/edit", HTTP_PUT, handleCreate);
  server.on("/system/resources/edit", HTTP_POST, []()
            { returnOK(); }, handleFileUpload);
  server.onNotFound(handleNotFound);
  server.on("/generate_204", handleNotFound);
  server.on("/favicon.ico", handleNotFound);
  server.on("/fwlink", handleNotFound);
  server.on("/reboot", handleReboot);
  server.on("/downloadSdFiles", HTTP_GET, handleDownloadSdFiles);
  server.on("/getWeight", handleGetWeight);
  server.on("/setProfile", handleSetProfile);
  server.on("/getProfile", handleGetProfile);
  server.on("/getLanguage", handleGetLanguage);
  server.on("/getProfileList", handleGetProfileList);
  server.on("/getTarget", handleGetTarget);
  server.on("/setTarget", handleSetTarget);
  server.on("/system/start", handleStart);
  server.on("/system/stop", handleStop);
  server.on("/screenshot", HTTP_GET, handleScreenshot);
  server.on("/fwupdate", HTTP_GET, []()
            {
            server.sendHeader("Connection", "close");
            String updatePage = "<form method='POST' action='/update' enctype='multipart/form-data'><p>FW-Version: ";
                  updatePage += FW_VERSION;
                  updatePage += "</p><br><input type='file' name='update'><input type='submit' value='Update'></form><br><button onClick='javascript:history.back()'>Back</button>";
            server.send(200, "text/html", updatePage); });

  server.on(
      "/update", HTTP_POST, []()
      {
        bool updateOk = webUpdateSucceeded && !Update.hasError();
        webUpdateStarted = false;
        webUpdateSucceeded = false;
        Serial.setDebugOutput(false);
        server.sendHeader("Connection", "close");
        server.send(updateOk ? 200 : 500, "text/html", updateOk ? "<h3>OK</h3><br><br><input type='button' value='Back' onClick='javascript:history.back()'>" : "<h3>FAIL</h3><br><br><input type='button' value='Back' onClick='javascript:history.back()'>");
        if (updateOk)
        {
          delay(500);
          ESP.restart();
        } },
      []()
      {
        HTTPUpload &upload = server.upload();
        if (upload.status == UPLOAD_FILE_START)
        {
          webUpdateStarted = false;
          webUpdateSucceeded = false;
          Update.clearError();
          Serial.setDebugOutput(true);
          Serial.printf("Update: %s\n", upload.filename.c_str());
          String infoText = String(langText("status_update_upload")) + String(upload.filename);
          updateDisplayLog(infoText);
          if (Update.begin(UPDATE_SIZE_UNKNOWN))
          {
            webUpdateStarted = true;
          }
          else
          { // start with max available size
            Update.printError(Serial);
            updateDisplayLog(String(langText("status_update_failed")) + Update.errorString());
            Serial.setDebugOutput(false);
          }
        }
        else if (upload.status == UPLOAD_FILE_WRITE)
        {
          if (!webUpdateStarted || Update.hasError())
          {
            return;
          }
          if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
          {
            Update.printError(Serial);
            updateDisplayLog(String(langText("status_update_write_failed")) + Update.errorString());
            Update.abort();
            webUpdateStarted = false;
            Serial.setDebugOutput(false);
          }
        }
        else if (upload.status == UPLOAD_FILE_END)
        {
          if (!webUpdateStarted || Update.hasError())
          {
            if (Update.isRunning())
            {
              Update.abort();
            }
            updateDisplayLog(String(langText("status_update_end_failed")) + Update.errorString());
          }
          else if (Update.end(true))
          { // true to set the size to the current progress
            Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
            String infoText = String(langText("status_update_success")) + String(upload.totalSize);
            updateDisplayLog(infoText);
            webUpdateSucceeded = true;
          }
          else
          {
            Update.printError(Serial);
            updateDisplayLog(String(langText("status_update_end_failed")) + Update.errorString());
          }
          webUpdateStarted = false;
          Serial.setDebugOutput(false);
        }
        else if (upload.status == UPLOAD_FILE_ABORTED)
        {
          Update.abort();
          webUpdateStarted = false;
          webUpdateSucceeded = false;
          Serial.setDebugOutput(false);
          Serial.printf("Update Failed Unexpectedly (likely broken connection): status=%d\n", upload.status);
          updateDisplayLog(langText("status_update_unexpected"));
        }
        else
        {
          Update.abort();
          webUpdateStarted = false;
          webUpdateSucceeded = false;
          Serial.setDebugOutput(false);
          Serial.printf("Update Failed Unexpectedly (likely broken connection): status=%d\n", upload.status);
          updateDisplayLog(langText("status_update_unexpected"));
        }
      });

  WEB_SERVER_ROUTES_REGISTERED = true;
}

bool startWebServerServices()
{
  if (WEB_SERVER_ACTIVE)
  {
    return true;
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    return false;
  }

  applyWifiDnsIfNeeded();
  updateDisplayLog(langText("status_wifi_connected"));
  bool mdnsStarted = MDNS.begin(host);
  if (!mdnsStarted)
  {
    DEBUG_PRINTLN("mDNS responder failed to start.");
  }

  registerWebServerRoutes();
  server.begin();
  WEB_SERVER_ACTIVE = true;

  if (mdnsStarted)
  {
    MDNS.addService("http", "tcp", 80);
    updateDisplayLog(String(langText("status_open_browser_prefix")) + host + langText("status_open_browser_suffix"));
  }

  updateDisplayLog("IP:" + WiFi.localIP().toString());


  if (config.fwCheck)
  {
    makeHttpGetRequest(firmwareCheckUrl());
  }

  if (lvglLock())
  {
    lv_obj_add_flag(ui_PanelMessages, LV_OBJ_FLAG_HIDDEN);
    lvglUnlock();
  }

  return true;
}

void maintainWifiConnection()
{
  if (!WIFI_AKTIVE || running)
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
    messageBox(langText("status_no_wifi"), &lv_font_montserrat_14, lv_color_hex(0xFFFF00), false);
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
  WIFI_AKTIVE = false;
  WEB_SERVER_ACTIVE = false;
  if (String(config.wifi_ssid).length() > 0)
  {
    updateDisplayLog(langText("status_connect_wifi"));
    updateDisplayLog(config.wifi_ssid);

    registerWifiDebugLogging();
    WiFi.disconnect();           // added to start with the wifi off, avoid crashing
    WiFi.softAPdisconnect(true); // Function will set currently configured SSID and password of the soft-AP to null values. The parameter  is optional. If set to true it will switch the soft-AP mode off.
    WiFi.mode(WIFI_OFF);         // added to start with the wifi off, avoid crashing

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
        // labelInfo.drawButton(false, "Static IP Failed to configure!");
        updateDisplayLog(langText("status_static_ip_failed"));
        delay(3000);
      }
    }
    if (!useStaticIp)
    {
      if (!WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE))
      {
        // labelInfo.drawButton(false, "DNS Failed to configure!");
        updateDisplayLog(langText("status_dns_failed"));
        delay(3000);
      }
    }

    wifiConfiguredDNS = ipDNS;
    wifiUsesStaticIp = useStaticIp;
    WiFi.begin(config.wifi_ssid, config.wifi_psk);
    WiFi.setSleep(false);
    WIFI_AKTIVE = true;
    wifiPreviousMillis = millis();
    wifiConnectStartedMillis = wifiPreviousMillis;
    wifiConnectTimeoutReported = false;
    wifiLastLoggedStatus = WL_NO_SHIELD;

    messageBox(String(langText("status_connect_wifi")) + String(config.wifi_ssid) + langText("msg_connect_wifi_wait"), &lv_font_montserrat_14, lv_color_hex(0xFFFFFF), false);
  }
}
