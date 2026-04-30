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
      updateDisplayLog("FW Update done!");
      if (Update.isFinished())
      {
        updateDisplayLog("Update successfully completed. Rebooting.");
        return true;
      }
      else
      {
        updateDisplayLog("Update not finished? Something went wrong!");
      }
    }
    else
    {
      DEBUG_PRINTLN("Error Occurred. Error #: " + String(Update.getError()));
      updateDisplayLog(String("Update failed: ") + Update.errorString());
    }
  }
  else
  {
    updateDisplayLog(String("FW Update begin failed: ") + Update.errorString());
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
      updateDisplayLog("Error, update.bin is not a file");
      updateBin.close();
      return;
    }

    size_t updateSize = updateBin.size();

    if (updateSize > 0)
    {
      updateDisplayLog("Try to start update");
      successfully = performUpdate(updateBin, updateSize);
    }
    else
    {
      updateDisplayLog("Error, file is empty");
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
    updateDisplayLog("No new firmware found");
  }
}

void initUpdate()
{
  updateDisplayLog("Check for Firmware Update...");
  updateFromFS(SD);
}
