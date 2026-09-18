const char *const MDNS_HOST = "robo-trickler";
static const char *WEB_REQUEST_HEADERS[] = {"Origin"};

bool webMutationAllowed()
{
  String origin = server.header("Origin");
  if (origin.length() == 0)
  {
    return true; // Non-browser clients need no Origin header.
  }
  String host = server.hostHeader();
  String expected = String("http://") + host;
  String hostName = host;
  int portSeparator = hostName.indexOf(':');
  if (portSeparator >= 0) hostName.remove(portSeparator);
  bool knownHost = wifiSetupApActive ||
                   (hostName == WiFi.localIP().toString()) ||
                   (hostName == "robo-trickler.local") ||
                   (hostName == "robo-trickler");
  if ((host.length() == 0) || !knownHost || (origin != expected))
  {
    server.send(403, "text/plain", "Cross-origin request rejected");
    return false;
  }
  return true;
}

static bool webUpdateStarted = false;
static bool webUpdateSucceeded = false;
static bool webUpdateFilesystem = false;
static bool webUpdateFilesystemUnmounted = false;
static bool webUpdateFilesystemLocked = false;
static size_t webUpdateMaxSize = 0;

static size_t webUpdateLimit(bool filesystemImage)
{
  const esp_partition_t *partition = filesystemImage
      ? esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL)
      : esp_ota_get_next_update_partition(NULL);
  if (partition == NULL) return 0;
  const size_t productLimit = filesystemImage ? 1536 * 1024 : 3300 * 1024;
  return (partition->size < productLimit) ? partition->size : productLimit;
}

static void releaseWebUpdateFilesystemLock()
{
  if (webUpdateFilesystemLocked)
  {
    webUpdateFilesystemLocked = false;
    filesystemUnlock();
  }
}

void restoreFilesystemAfterFailedUpdate()
{
  if (!webUpdateFilesystemUnmounted)
  {
    return;
  }

  webUpdateFilesystemUnmounted = false;
  littleFsMounted = LittleFS.begin(false);
  if (!littleFsMounted)
  {
    DEBUG_PRINTLN("LittleFS remount failed after update error.");
    return;
  }

  if (!activeFsIsSd)
  {
    activeFs = &LittleFS;
  }
  filesystemActive = sdMounted || littleFsMounted;
}

void returnOk()
{
  server.send(200, "text/plain", "");
}

void returnFail(String message)
{
  server.send(500, "text/plain", message + "\r\n");
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

void registerWebServerRoutes()
{
  if (webServerRoutesRegistered)
  {
    return;
  }

  server.collectHeaders(WEB_REQUEST_HEADERS, 1);

  server.on("/list", HTTP_GET, printDirectory);
  server.on("/", HTTP_GET, handleHomePage);
  server.on("/system/ap", HTTP_GET, handleWifiSetupPortal);
  server.on("/api/wifi/scan", HTTP_GET, handleWifiScan);
  server.on("/api/wifi/save", HTTP_POST, handleWifiSave);
  server.on("/system/resources/edit", HTTP_DELETE, handleDelete);
  server.on("/system/resources/edit", HTTP_PUT, handleCreate);
  // The web editor uses this endpoint for multipart uploads. The upload
  // handler writes directly to the LittleFS path supplied as the filename.
  server.on("/system/resources/edit", HTTP_POST, []()
            { finishFileUploadRequest(); }, handleFileUpload);
  server.onNotFound(handleNotFound);
  server.on("/generate_204", HTTP_GET, handleNotFound);
  server.on("/favicon.ico", HTTP_GET, handleNotFound);
  server.on("/fwlink", HTTP_GET, handleNotFound);
  server.on("/reboot", HTTP_POST, handleReboot);
  server.on("/setProfile", HTTP_POST, handleSetProfile);
  server.on("/getProfile", HTTP_GET, handleGetProfile);
  server.on("/getLanguage", HTTP_GET, handleGetLanguage);
  server.on("/getProfileList", HTTP_GET, handleGetProfileList);
  server.on("/getTarget", HTTP_GET, handleGetTarget);
  server.on("/getTricklerState", HTTP_GET, handleGetTricklerState);
  server.on("/setTarget", HTTP_POST, handleSetTarget);
  server.on("/system/start", HTTP_POST, handleStart);
  server.on("/system/stop", HTTP_POST, handleStop);
#if ENABLE_SCREENSHOT
  server.on("/screenshot", HTTP_GET, handleScreenshot);
#endif
  server.on("/fwupdate", HTTP_GET, []()
            {
            // Stream the page in fragments so we never hold the whole HTML body in heap.
            // Web text is parsed once from the web language store for this request.
            JsonDocument langDoc;
            loadWebLang(langDoc);
            server.sendHeader("Connection", "close");
            server.setContentLength(CONTENT_LENGTH_UNKNOWN);
            server.send(200, "text/html", "");
            server.sendContent(webPageHead(webFwText(langDoc, "updateTitle", "Firmware Update")));
            server.sendContent("<p>");
            server.sendContent(webFwText(langDoc, "fwVersion", "FW-Version"));
            server.sendContent(": " FW_VERSION "</p><h3>");
            server.sendContent(webFwText(langDoc, "firmwareImage", "Firmware image"));
            server.sendContent("</h3><form method='POST' action='/update' enctype='multipart/form-data'>"
                               "<input type='file' name='firmware' accept='.bin,application/octet-stream' required>"
                               "<input class='button' type='submit' value='");
            server.sendContent(webFwText(langDoc, "updateFirmware", "Update Firmware"));
            server.sendContent("'></form><br>");
            server.sendContent("<h3>");
            server.sendContent(webFwText(langDoc, "littlefsImage", "LittleFS image"));
            server.sendContent("</h3><p>");
            server.sendContent(webFwText(langDoc, "filesystemWarning", "Uploading replaces all files in the internal filesystem."));
            server.sendContent("</p><form method='POST' action='/update' enctype='multipart/form-data'>"
                               "<input type='file' name='filesystem' accept='.bin,application/octet-stream' required>"
                               "<input class='button' type='submit' value='");
            server.sendContent(webFwText(langDoc, "updateLittlefs", "Update LittleFS"));
            server.sendContent("'></form><br>");
            server.sendContent(webBackButtonHtml(langDoc));
            server.sendContent(webPageFoot()); });

  server.on(
      "/update", HTTP_POST, []()
      {
        if (!webMutationAllowed() || isTricklerRunning())
        {
          if (Update.isRunning()) Update.abort();
          restoreFilesystemAfterFailedUpdate();
          releaseWebUpdateFilesystemLock();
          server.send(409, "text/plain", "Update rejected");
          return;
        }
        bool updateOk = webUpdateSucceeded && !Update.hasError();
        if (!updateOk)
        {
          restoreFilesystemAfterFailedUpdate();
        }
        webUpdateStarted = false;
        webUpdateSucceeded = false;
        webUpdateFilesystem = false;
        webUpdateFilesystemUnmounted = false;
        releaseWebUpdateFilesystemLock();
        Serial.setDebugOutput(false);
        server.sendHeader("Connection", "close");
        server.send(updateOk ? 200 : 500, "text/html",
                    webStatusPage(updateOk ? "ok" : "fail", updateOk ? "OK" : "FAIL"));
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
          webUpdateFilesystem = upload.name == "filesystem";
          if (!webMutationAllowed() || isTricklerRunning() ||
              (upload.name != "filesystem" && upload.name != "firmware") ||
              isWebFileUploadActive() || !filesystemLock())
          {
            return;
          }
          webUpdateFilesystemLocked = true;
          webUpdateMaxSize = webUpdateLimit(webUpdateFilesystem);
          int contentLength = server.clientContentLength();
          if ((webUpdateMaxSize == 0) || (contentLength <= 0) ||
              ((size_t)contentLength > webUpdateMaxSize + 4096))
          {
            releaseWebUpdateFilesystemLock();
            return;
          }
          Update.clearError();
          Serial.setDebugOutput(true);
          Serial.printf("%s update: %s\n", webUpdateFilesystem ? "LittleFS" : "Firmware", upload.filename.c_str());
          String infoText = String(langText("status_update_upload")) + String(upload.filename);
          updateDisplayLog(infoText);

          if (webUpdateFilesystem && littleFsMounted)
          {
            LittleFS.end();
            littleFsMounted = false;
            if (!activeFsIsSd)
            {
              filesystemActive = false;
              activeFs = NULL;
            }
            webUpdateFilesystemUnmounted = true;
          }

          int updateTarget = webUpdateFilesystem ? U_FLASHFS : U_FLASH;
          if (Update.begin(webUpdateMaxSize, updateTarget))
          {
            webUpdateStarted = true;
          }
          else
          {
            Update.printError(Serial);
            updateDisplayLog(String(langText("status_update_failed")) + Update.errorString());
            restoreFilesystemAfterFailedUpdate();
            Serial.setDebugOutput(false);
          }
        }
        else if (upload.status == UPLOAD_FILE_WRITE)
        {
          if (!webUpdateStarted || Update.hasError())
          {
            return;
          }
          if ((upload.totalSize > webUpdateMaxSize) ||
              (upload.currentSize > webUpdateMaxSize - upload.totalSize))
          {
            Update.abort();
            webUpdateStarted = false;
            restoreFilesystemAfterFailedUpdate();
            releaseWebUpdateFilesystemLock();
            return;
          }
          if ((upload.totalSize == 0) && !webUpdateFilesystem &&
              ((upload.currentSize == 0) || (upload.buf[0] != 0xE9)))
          {
            Update.abort();
            webUpdateStarted = false;
            releaseWebUpdateFilesystemLock();
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
          if ((upload.totalSize == 0) ||
              (webUpdateFilesystem && (upload.totalSize != webUpdateMaxSize)))
          {
            if (Update.isRunning()) Update.abort();
            webUpdateStarted = false;
          }
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
          if (!webUpdateSucceeded) restoreFilesystemAfterFailedUpdate();
          releaseWebUpdateFilesystemLock();
          Serial.setDebugOutput(false);
        }
        else
        {
          Update.abort();
          webUpdateStarted = false;
          webUpdateSucceeded = false;
          restoreFilesystemAfterFailedUpdate();
          releaseWebUpdateFilesystemLock();
          Serial.setDebugOutput(false);
          Serial.printf("Update Failed Unexpectedly (likely broken connection): status=%d\n", upload.status);
          updateDisplayLog(langText("status_update_unexpected"));
        }
      });

  webServerRoutesRegistered = true;
}

bool startWebServerServices()
{
  if (webServerActive)
  {
    return true;
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    return false;
  }

  applyWifiDnsIfNeeded();
  updateDisplayLog(langText("status_wifi_connected"));
  bool mdnsStarted = MDNS.begin(MDNS_HOST);
  if (!mdnsStarted)
  {
    DEBUG_PRINTLN("mDNS responder failed to start.");
  }

  registerWebServerRoutes();
  server.begin();
  webServerActive = true;

  if (mdnsStarted)
  {
    MDNS.addService("http", "tcp", 80);
    updateDisplayLog(String(langText("status_open_browser_prefix")) + MDNS_HOST + langText("status_open_browser_suffix"));
  }

  updateDisplayLog("IP:" + WiFi.localIP().toString());


  if (config.fwUpdateCheck)
  {
    makeHttpGetRequest(firmwareCheckUrl());
  }

  return true;
}
