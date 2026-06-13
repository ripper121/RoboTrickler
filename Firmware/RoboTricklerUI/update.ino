static const char *SD_FIRMWARE_UPDATE_PATH = "/firmware.bin";
static const char *SD_LITTLEFS_UPDATE_PATH = "/littleFS.bin";

static const int SD_UPDATE_NOT_FOUND = 0;
static const int SD_UPDATE_FAILED = 1;
static const int SD_UPDATE_SUCCEEDED = 2;

static bool performSdUpdate(File &updateFile, size_t updateSize, int updateTarget, const char *label)
{
  Update.clearError();
  if (!Update.begin(updateSize, updateTarget))
  {
    updateDisplayLog(String(label) + langText("status_update_begin_failed_suffix") + Update.errorString());
    return false;
  }

  size_t written = Update.writeStream(updateFile);
  if (written != updateSize)
  {
    updateDisplayLog(String(label) + langText("status_update_write_failed_suffix") + String(written) + "/" + String(updateSize));
    Update.abort();
    return false;
  }

  if (!Update.end())
  {
    updateDisplayLog(String(label) + langText("status_update_failed_suffix") + Update.errorString());
    return false;
  }

  if (!Update.isFinished())
  {
    updateDisplayLog(String(label) + langText("status_update_not_finished_suffix"));
    return false;
  }

  updateDisplayLog(String(label) + langText("status_update_completed_suffix"));
  return true;
}

static int updateFromSd(const char *path, int updateTarget, const char *label)
{
  File updateFile = SD.open(path, FILE_READ);
  if (!updateFile)
  {
    return SD_UPDATE_NOT_FOUND;
  }

  if (updateFile.isDirectory())
  {
    updateDisplayLog(String(label) + langText("status_update_not_file_suffix") + path);
    updateFile.close();
    return SD_UPDATE_FAILED;
  }

  size_t updateSize = updateFile.size();
  if (updateSize == 0)
  {
    updateDisplayLog(String(label) + langText("status_update_empty_suffix") + path);
    updateFile.close();
    return SD_UPDATE_FAILED;
  }

  updateDisplayLog(String(langText("status_update_start_prefix")) + label + langText("status_update_start_suffix") + path);
  bool updateSucceeded = performSdUpdate(updateFile, updateSize, updateTarget, label);
  updateFile.close();

  if (!updateSucceeded)
  {
    return SD_UPDATE_FAILED;
  }

  if (!SD.remove(path))
  {
    updateDisplayLog(String(langText("status_update_delete_failed")) + path);
  }

  return SD_UPDATE_SUCCEEDED;
}

void initUpdate()
{
  if (!activeFSIsSD)
  {
    return;
  }

  bool restartRequired = false;
  updateDisplayLog(langText("status_check_sd_updates"));

  int firmwareResult = updateFromSd(SD_FIRMWARE_UPDATE_PATH, U_FLASH, "Firmware");
  if (firmwareResult == SD_UPDATE_FAILED)
  {
    return;
  }
  restartRequired = firmwareResult == SD_UPDATE_SUCCEEDED;

  int littleFsResult = updateFromSd(SD_LITTLEFS_UPDATE_PATH, U_FLASHFS, "LittleFS");
  restartRequired = restartRequired || (littleFsResult == SD_UPDATE_SUCCEEDED);

  if (restartRequired)
  {
    updateDisplayLog(langText("status_sd_update_complete"));
    delay(1000);
    ESP.restart();
  }
}
