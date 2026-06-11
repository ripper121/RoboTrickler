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

void initUpdate()
{
  updateDisplayLog(langText("status_check_fw_update"));
  updateFromFS(SD);
}
