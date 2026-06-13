const char *host = "robo-trickler";

static bool webUpdateStarted = false;
static bool webUpdateSucceeded = false;
static bool webUpdateFilesystem = false;
static bool webUpdateFilesystemUnmounted = false;

void restoreFilesystemAfterFailedUpdate()
{
  if (!webUpdateFilesystemUnmounted)
  {
    return;
  }

  webUpdateFilesystemUnmounted = false;
  if (activeFSIsSD)
  {
    // SD is active, LittleFS was never the primary filesystem for runtime.
    return;
  }

  FILESYSTEM_ACTIVE = LittleFS.begin(false);
  if (FILESYSTEM_ACTIVE)
  {
    activeFS = &LittleFS;
  }
  else
  {
    DEBUG_PRINTLN("LittleFS remount failed after update error.");
  }
}

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

void registerWebServerRoutes()
{
  if (WEB_SERVER_ROUTES_REGISTERED)
  {
    return;
  }

  server.on("/list", HTTP_GET, printDirectory);
  server.on("/", HTTP_GET, handleHomePage);
  server.on("/system/ap", HTTP_GET, handleWifiSetupPortal);
  server.on("/system/ap/", HTTP_GET, handleWifiSetupPortal);
  server.on("/api/wifi/scan", HTTP_GET, handleWifiScan);
  server.on("/api/wifi/save", HTTP_POST, handleWifiSave);
  server.on("/system/resources/edit", HTTP_DELETE, handleDelete);
  server.on("/system/resources/edit", HTTP_PUT, handleCreate);
  // The web editor uses this endpoint for multipart uploads. The upload
  // handler writes directly to the LittleFS path supplied as the filename.
  server.on("/system/resources/edit", HTTP_POST, []()
            { returnOK(); }, handleFileUpload);
  server.onNotFound(handleNotFound);
  server.on("/generate_204", handleNotFound);
  server.on("/favicon.ico", handleNotFound);
  server.on("/fwlink", handleNotFound);
  server.on("/reboot", handleReboot);
  server.on("/setProfile", handleSetProfile);
  server.on("/getProfile", handleGetProfile);
  server.on("/getLanguage", handleGetLanguage);
  server.on("/getProfileList", handleGetProfileList);
  server.on("/getTarget", handleGetTarget);
  server.on("/getTricklerState", handleGetTricklerState);
  server.on("/setTarget", handleSetTarget);
  server.on("/system/start", handleStart);
  server.on("/system/stop", handleStop);
#if ENABLE_SCREENSHOT
  server.on("/screenshot", HTTP_GET, handleScreenshot);
#endif
  server.on("/fwupdate", HTTP_GET, []()
            {
            server.sendHeader("Connection", "close");
            String updatePage = "<p>";
                  updatePage += langText("web_fw_version");
                  updatePage += ": ";
                  updatePage += FW_VERSION;
                  updatePage += "</p><h3>";
                  updatePage += langText("web_firmware_image");
                  updatePage += "</h3>";
                  updatePage += "<form method='POST' action='/update' enctype='multipart/form-data'>";
                  updatePage += "<input type='file' name='firmware' accept='.bin,application/octet-stream' required>";
                  updatePage += "<input type='submit' value='";
                  updatePage += langText("web_update_firmware");
                  updatePage += "'></form><br><h3>";
                  updatePage += langText("web_littlefs_image");
                  updatePage += "</h3><p>";
                  updatePage += langText("web_update_filesystem_warning");
                  updatePage += "</p>";
                  updatePage += "<form method='POST' action='/update' enctype='multipart/form-data'>";
                  updatePage += "<input type='file' name='filesystem' accept='.bin,application/octet-stream' required>";
                  updatePage += "<input type='submit' value='";
                  updatePage += langText("web_update_littlefs");
                  updatePage += "'></form><br>";
                  updatePage += webBackButtonHtml();
            server.send(200, "text/html", updatePage); });

  server.on(
      "/update", HTTP_POST, []()
      {
        bool updateOk = webUpdateSucceeded && !Update.hasError();
        if (!updateOk)
        {
          restoreFilesystemAfterFailedUpdate();
        }
        webUpdateStarted = false;
        webUpdateSucceeded = false;
        webUpdateFilesystem = false;
        webUpdateFilesystemUnmounted = false;
        Serial.setDebugOutput(false);
        server.sendHeader("Connection", "close");
        server.send(updateOk ? 200 : 500, "text/html", webStatusPage(updateOk ? "web_update_ok" : "web_update_fail"));
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
          Update.clearError();
          Serial.setDebugOutput(true);
          Serial.printf("%s update: %s\n", webUpdateFilesystem ? "LittleFS" : "Firmware", upload.filename.c_str());
          String infoText = String(langText("status_update_upload")) + String(upload.filename);
          updateDisplayLog(infoText);

          if (webUpdateFilesystem && FILESYSTEM_ACTIVE && !activeFSIsSD)
          {
            LittleFS.end();
            FILESYSTEM_ACTIVE = false;
            activeFS = NULL;
            webUpdateFilesystemUnmounted = true;
          }

          int updateTarget = webUpdateFilesystem ? U_FLASHFS : U_FLASH;
          if (Update.begin(UPDATE_SIZE_UNKNOWN, updateTarget))
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
          restoreFilesystemAfterFailedUpdate();
          Serial.setDebugOutput(false);
          Serial.printf("Update Failed Unexpectedly (likely broken connection): status=%d\n", upload.status);
          updateDisplayLog(langText("status_update_unexpected"));
        }
        else
        {
          Update.abort();
          webUpdateStarted = false;
          webUpdateSucceeded = false;
          restoreFilesystemAfterFailedUpdate();
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

  closeDialog(&ui_PanelMessages, false);

  return true;
}

