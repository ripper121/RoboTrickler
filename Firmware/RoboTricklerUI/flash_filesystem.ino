bool initFilesystem()
{
  FILESYSTEM_ACTIVE = LittleFS.begin(false);
  if (!FILESYSTEM_ACTIVE)
  {
    DEBUG_PRINTLN("LittleFS mount failed.");
    return false;
  }

  DEBUG_PRINT("LittleFS total bytes: ");
  DEBUG_PRINTLN(LittleFS.totalBytes());
  DEBUG_PRINT("LittleFS used bytes: ");
  DEBUG_PRINTLN(LittleFS.usedBytes());
  return true;
}
