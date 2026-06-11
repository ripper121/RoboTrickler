// perform the actual update from a given stream
bool performUpdate(Stream &updateSource, size_t updateSize)
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
      updateDisplayLog(langText("status_fw_update_done"));
      if (Update.isFinished())
      {
        updateDisplayLog(langText("status_update_completed"));
        return true;
      }
      else
      {
        updateDisplayLog(langText("status_update_not_finished"));
      }
    }
    else
    {
      DEBUG_PRINTLN("Error Occurred. Error #: " + String(Update.getError()));
      updateDisplayLog(String(langText("status_update_failed")) + Update.errorString());
    }
  }
  else
  {
    updateDisplayLog(String(langText("status_fw_update_begin_failed")) + Update.errorString());
  }
  return false;
}

// check given FS for valid update.bin and perform update if available
void updateFromFS(fs::FS &fs)
{
  bool successfully = false;
  File updateBin = fs.open("/update.bin");
  if (updateBin)
  {
    if (updateBin.isDirectory())
    {
      updateDisplayLog(langText("status_update_not_file"));
      updateBin.close();
      return;
    }

    size_t updateSize = updateBin.size();

    if (updateSize > 0)
    {
      updateDisplayLog(langText("status_update_start"));
      successfully = performUpdate(updateBin, updateSize);
    }
    else
    {
      updateDisplayLog(langText("status_update_empty"));
    }

    updateBin.close();

    if (successfully)
    {
      // whe finished remove the binary from sd card to indicate end of the process
      fs.remove("/update.bin");
      delay(1000);
      ESP.restart();
    }
  }
  else
  {
    updateDisplayLog(langText("status_no_new_firmware"));
  }
}

bool downloadUrlToSdFile(const String &url, const char *sdPath)
{
  HTTPClient http;
  http.setTimeout(10000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  if (!http.begin(url))
  {
    DEBUG_PRINTLN("Unable to connect");
    return false;
  }

  http.addHeader("Accept-Encoding", "identity");
  int httpResponseCode = http.GET();
  if (httpResponseCode != HTTP_CODE_OK)
  {
    DEBUG_PRINT("HTTP download failed: ");
    DEBUG_PRINTLN(httpResponseCode);
    DEBUG_PRINTLN(HTTPClient::errorToString(httpResponseCode));
    http.end();
    return false;
  }

  int remaining = http.getSize();
  int expectedSize = remaining;
  NetworkClient *stream = http.getStreamPtr();

  if (SD.exists(sdPath))
  {
    SD.remove(sdPath);
  }

  File downloadFile = SD.open(sdPath, FILE_WRITE);
  if (!downloadFile)
  {
    DEBUG_PRINT("Failed to open SD file for download: ");
    DEBUG_PRINTLN(sdPath);
    http.end();
    return false;
  }

  uint8_t buffer[1024];
  size_t totalWritten = 0;
  bool success = true;
  unsigned long lastDataMillis = millis();

  while (http.connected() && (remaining > 0 || remaining == -1))
  {
    size_t available = stream->available();
    if (available)
    {
      int bytesRead = stream->readBytes(buffer, (available > sizeof(buffer)) ? sizeof(buffer) : available);
      if (bytesRead <= 0)
      {
        success = false;
        break;
      }

      size_t bytesWritten = downloadFile.write(buffer, bytesRead);
      if (bytesWritten != (size_t)bytesRead)
      {
        success = false;
        break;
      }

      totalWritten += bytesWritten;
      if (remaining > 0)
      {
        remaining -= bytesRead;
      }
      lastDataMillis = millis();
    }
    else if ((millis() - lastDataMillis) > 10000)
    {
      success = false;
      break;
    }

    delay(1);
  }

  downloadFile.close();
  http.end();

  if (success && expectedSize > 0 && totalWritten != (size_t)expectedSize)
  {
    success = false;
  }

  if (!success)
  {
    SD.remove(sdPath);
    DEBUG_PRINT("Download failed for ");
    DEBUG_PRINTLN(sdPath);
    return false;
  }

  DEBUG_PRINT("Downloaded ");
  DEBUG_PRINT(totalWritten);
  DEBUG_PRINT(" bytes to ");
  DEBUG_PRINTLN(sdPath);
  return true;
}

bool downloadSdFilesTar()
{
  updateDisplayLog("Downloading SD-Files.tar...");
  bool success = downloadUrlToSdFile(DEFAULT_SD_FILES_TAR_URL, SD_FILES_TAR_PATH);
  updateDisplayLog(success ? "SD-Files.tar downloaded" : "SD-Files.tar download failed");
  return success;
}

void initUpdate()
{
  updateDisplayLog(langText("status_check_fw_update"));
  updateFromFS(SD);
}
