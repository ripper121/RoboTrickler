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

#define SD_FILES_TAR_EXTRACT_DIR "/tar"

String tarHeaderString(const uint8_t *header, size_t offset, size_t length)
{
  String value;
  value.reserve(length);
  for (size_t i = 0; i < length; i++)
  {
    char c = (char)header[offset + i];
    if (c == '\0')
    {
      break;
    }
    value += c;
  }
  return value;
}

uint32_t tarHeaderOctal(const uint8_t *header, size_t offset, size_t length)
{
  uint32_t value = 0;
  for (size_t i = 0; i < length; i++)
  {
    char c = (char)header[offset + i];
    if ((c >= '0') && (c <= '7'))
    {
      value = (value << 3) + (uint32_t)(c - '0');
    }
  }
  return value;
}

bool tarHeaderIsEmpty(const uint8_t *header)
{
  for (size_t i = 0; i < 512; i++)
  {
    if (header[i] != 0)
    {
      return false;
    }
  }
  return true;
}

String cleanTarEntryName(String entryName)
{
  entryName.replace("\\", "/");
  while (entryName.startsWith("./"))
  {
    entryName = entryName.substring(2);
  }
  while (entryName.startsWith("/"))
  {
    entryName = entryName.substring(1);
  }

  if ((entryName.length() == 0) || (entryName == ".") || (entryName.indexOf("..") >= 0))
  {
    return String();
  }

  return entryName;
}

bool ensureSdDir(String path)
{
  if (path.length() == 0)
  {
    return false;
  }
  if (path == "/")
  {
    return true;
  }

  String current;
  int start = 1;
  while (true)
  {
    int slash = path.indexOf('/', start);
    if (slash < 0)
    {
      current = path;
    }
    else
    {
      current = path.substring(0, slash);
    }

    if ((current.length() > 1) && !SD.exists(current.c_str()) && !SD.mkdir(current.c_str()))
    {
      DEBUG_PRINT("Failed to create directory: ");
      DEBUG_PRINTLN(current);
      return false;
    }

    if (slash < 0)
    {
      break;
    }
    start = slash + 1;
  }

  return true;
}

bool ensureSdParentDir(const String &path)
{
  int slash = path.lastIndexOf('/');
  if (slash <= 0)
  {
    return true;
  }
  return ensureSdDir(path.substring(0, slash));
}

bool skipTarBytes(File &tarFile, uint32_t byteCount, uint8_t *buffer, size_t bufferSize)
{
  while (byteCount > 0)
  {
    size_t toRead = (byteCount > bufferSize) ? bufferSize : byteCount;
    size_t bytesRead = tarFile.read(buffer, toRead);
    if (bytesRead != toRead)
    {
      return false;
    }
    byteCount -= bytesRead;
    delay(1);
  }
  return true;
}

bool extractTarToSdFolder(const char *tarPath, const char *extractDir)
{
  File tarFile = SD.open(tarPath, FILE_READ);
  if (!tarFile || tarFile.isDirectory())
  {
    DEBUG_PRINT("Failed to open tar file: ");
    DEBUG_PRINTLN(tarPath);
    if (tarFile)
    {
      tarFile.close();
    }
    return false;
  }

  if (SD.exists(extractDir))
  {
    deleteRecursive(String(extractDir));
  }
  if (!ensureSdDir(String(extractDir)))
  {
    tarFile.close();
    return false;
  }

  uint8_t buffer[1024];
  bool success = true;

  while (tarFile.available())
  {
    size_t headerRead = tarFile.read(buffer, 512);
    if (headerRead == 0)
    {
      break;
    }
    if (headerRead != 512)
    {
      success = false;
      break;
    }
    if (tarHeaderIsEmpty(buffer))
    {
      break;
    }
    delay(1);

    String entryName = tarHeaderString(buffer, 0, 100);
    String entryPrefix = tarHeaderString(buffer, 345, 155);
    if (entryPrefix.length() > 0)
    {
      entryName = entryPrefix + "/" + entryName;
    }
    entryName = cleanTarEntryName(entryName);

    uint32_t entrySize = tarHeaderOctal(buffer, 124, 12);
    uint32_t paddedSize = (entrySize + 511) & ~((uint32_t)511);
    char typeFlag = (char)buffer[156];

    if (entryName.length() == 0)
    {
      success = skipTarBytes(tarFile, paddedSize, buffer, sizeof(buffer));
      if (!success)
      {
        break;
      }
      continue;
    }

    String targetPath = String(extractDir) + "/" + entryName;

    if ((typeFlag == '5') || targetPath.endsWith("/"))
    {
      targetPath.trim();
      if (targetPath.endsWith("/"))
      {
        targetPath.remove(targetPath.length() - 1);
      }
      success = ensureSdDir(targetPath) && skipTarBytes(tarFile, paddedSize, buffer, sizeof(buffer));
      if (!success)
      {
        break;
      }
      continue;
    }

    if ((typeFlag != '\0') && (typeFlag != '0'))
    {
      success = skipTarBytes(tarFile, paddedSize, buffer, sizeof(buffer));
      if (!success)
      {
        break;
      }
      continue;
    }

    if (!ensureSdParentDir(targetPath))
    {
      success = false;
      break;
    }

    if (SD.exists(targetPath.c_str()))
    {
      SD.remove(targetPath.c_str());
    }

    File outFile = SD.open(targetPath.c_str(), FILE_WRITE);
    if (!outFile)
    {
      DEBUG_PRINT("Failed to create extracted file: ");
      DEBUG_PRINTLN(targetPath);
      success = false;
      break;
    }

    uint32_t remaining = entrySize;
    while (remaining > 0)
    {
      size_t toRead = (remaining > sizeof(buffer)) ? sizeof(buffer) : remaining;
      size_t bytesRead = tarFile.read(buffer, toRead);
      if (bytesRead != toRead)
      {
        success = false;
        break;
      }
      if (outFile.write(buffer, bytesRead) != bytesRead)
      {
        success = false;
        break;
      }
      remaining -= bytesRead;
      delay(1);
    }
    outFile.close();

    if (!success)
    {
      break;
    }

    success = skipTarBytes(tarFile, paddedSize - entrySize, buffer, sizeof(buffer));
    if (!success)
    {
      break;
    }
  }

  tarFile.close();
  return success;
}

bool downloadSdFilesTar()
{
  updateDisplayLog("Downloading SD-Files.tar...");
  bool success = downloadUrlToSdFile(DEFAULT_SD_FILES_TAR_URL, SD_FILES_TAR_PATH);
  if (!success)
  {
    updateDisplayLog("SD-Files.tar download failed");
    return false;
  }

  updateDisplayLog("SD-Files.tar downloaded");
  updateDisplayLog("Extracting SD-Files.tar to /tar...");
  success = extractTarToSdFolder(SD_FILES_TAR_PATH, SD_FILES_TAR_EXTRACT_DIR);
  updateDisplayLog(success ? "SD-Files.tar extracted to /tar" : "SD-Files.tar extract failed");
  return success;
}

void initUpdate()
{
  updateDisplayLog(langText("status_check_fw_update"));
  updateFromFS(SD);
}
