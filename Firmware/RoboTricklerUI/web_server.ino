const char *host = "robo-trickler";

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

void registerWebServerRoutes()
{
  if (WEB_SERVER_ROUTES_REGISTERED)
  {
    return;
  }

  server.on("/list", HTTP_GET, printDirectory);
  server.on("/system/resources/edit", HTTP_DELETE, handleDelete);
  server.on("/system/resources/edit", HTTP_PUT, handleCreate);
  // The SD web editor uses this endpoint for multipart uploads. The upload
  // handler writes directly to the SD path supplied as the multipart filename.
  server.on("/system/resources/edit", HTTP_POST, []()
            { returnOK(); }, handleFileUpload);
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
  server.on("/getTricklerRunning", handleGetTricklerRunning);
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

