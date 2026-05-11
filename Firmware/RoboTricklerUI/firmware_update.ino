// perform the actual update from a given stream
bool performFirmwareUpdate(Stream &updateSource, size_t updateSize)
{
  if (Update.begin(updateSize))
  {
    size_t written = Update.writeStream(updateSource);
    if (written == updateSize)
    {
      DEBUG_PRINTLN("Written : " + String(written) + " successfully");
    }
    else
    {
      DEBUG_PRINTLN("Written only : " + String(written) + "/" + String(updateSize));
    }
    if (Update.end())
    {
      updateDisplayLog(languageText("status_fw_update_done"));
      if (Update.isFinished())
      {
        updateDisplayLog(languageText("status_update_completed"));
        return true;
      }
      else
      {
        updateDisplayLog(languageText("status_update_not_finished"));
      }
    }
    else
    {
      DEBUG_PRINTLN("Error Occurred. Error #: " + String(Update.getError()));
      updateDisplayLog(String(languageText("status_update_failed")) + Update.errorString());
    }
  }
  else
  {
    updateDisplayLog(String(languageText("status_fw_update_begin_failed")) + Update.errorString());
  }
  return false;
}

// check given FS for valid update.bin and perform update if available
void updateFirmwareFromFileSystem(fs::FS &fs)
{
  bool successfully = false;
  File updateBin = fs.open("/update.bin");
  if (updateBin)
  {
    if (updateBin.isDirectory())
    {
      updateDisplayLog(languageText("status_update_not_file"));
      updateBin.close();
      return;
    }

    size_t updateSize = updateBin.size();

    if (updateSize > 0)
    {
      updateDisplayLog(languageText("status_update_start"));
      successfully = performFirmwareUpdate(updateBin, updateSize);
    }
    else
    {
      updateDisplayLog(languageText("status_update_empty"));
    }

    updateBin.close();

    if (successfully)
    {
      // whe finished remove the binary from sd card to indicate end of the process
      fs.remove("/update.bin");
      delay(RESTART_DELAY_MS);
      ESP.restart();
    }
  }
  else
  {
    updateDisplayLog(languageText("status_no_new_firmware"));
  }
}

void initFirmwareUpdate()
{
  updateDisplayLog(languageText("status_check_fw_update"));
  updateFirmwareFromFileSystem(SD);
}

String firmwareCheckUrl()
{
  String serverPath = String(config.fwUpdateUrl);
  if (serverPath.length() == 0)
  {
    serverPath = DEFAULT_FW_UPDATE_URL;
  }
  if (serverPath == DEFAULT_FW_UPDATE_URL)
  {
    serverPath += "?mac=" + String(WiFi.macAddress());
  }
  return serverPath;
}

void checkFirmwareUpdate()
{
  String serverPath = firmwareCheckUrl();
  HTTPClient http;
  http.setTimeout(FIRMWARE_HTTP_TIMEOUT_MS);

  NetworkClientSecure secureClient;
  bool connected = false;
  if (serverPath.startsWith("https://"))
  {
    secureClient.setInsecure();
    connected = http.begin(secureClient, serverPath);
  }
  else
  {
    connected = http.begin(serverPath);
  }

  if (connected)
  {
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0)
    {
      DEBUG_PRINT("HTTP Response code: ");
      DEBUG_PRINTLN(httpResponseCode);
      String payload = http.getString();
      DEBUG_PRINTLN(payload);
      if (FW_VERSION < payload.toFloat())
      {
        String message = String(languageText("msg_new_firmware")) + payload + languageText("msg_check_url");
        messageBox(message.c_str(), &lv_font_montserrat_14, lv_color_hex(UI_COLOR_OK), true);
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
