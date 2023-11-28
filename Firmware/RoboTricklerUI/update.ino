// perform the actual update from a given stream
bool performUpdate(Stream &updateSource, size_t updateSize) {
  if (Update.begin(updateSize)) {
    size_t written = Update.writeStream(updateSource);
    if (written == updateSize) {
      DEBUG_PRINTLN("Written : " + String(written) + " successfully");
    }
    else {
      DEBUG_PRINTLN("Written only : " + String(written) + "/" + String(updateSize) + ". Retry?");
    }
    if (Update.end()) {
      DEBUG_PRINTLN("OTA done!");
      if (Update.isFinished()) {
        DEBUG_PRINTLN("Update successfully completed. Rebooting.");
        return true;
      }
      else {
        DEBUG_PRINTLN("Update not finished? Something went wrong!");
      }
    }
    else {
      DEBUG_PRINTLN("Error Occurred. Error #: " + String(Update.getError()));
    }

  }
  else
  {
    DEBUG_PRINTLN("Not enough space to begin OTA");
  }
  return false;
}

// check given FS for valid update.bin and perform update if available
void updateFromFS(fs::FS &fs) {
  bool successfully = false;
  File updateBin = fs.open("/update.bin");
  if (updateBin) {
    if (updateBin.isDirectory()) {
      DEBUG_PRINTLN("Error, update.bin is not a file");
      updateBin.close();
      return;
    }

    size_t updateSize = updateBin.size();

    if (updateSize > 0) {
      DEBUG_PRINTLN("Try to start update");
      successfully = performUpdate(updateBin, updateSize);
    }
    else {
      DEBUG_PRINTLN("Error, file is empty");
    }

    updateBin.close();

    if (successfully) {
      // whe finished remove the binary from sd card to indicate end of the process
      fs.remove("/update.bin");
      delay(1000);
      ESP.restart();
    }
  }
  else {
    DEBUG_PRINTLN("Could not load update.bin from sd root");
  }
}

void initUpdate() {
  DEBUG_PRINTLN("initUpdate()");
  updateFromFS(SD);
}
