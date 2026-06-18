bool initFilesystem()
{
  FILESYSTEM_ACTIVE = false;
  activeFS = NULL;
  activeFSIsSD = false;
  sdMounted = false;
  littleFSMounted = false;

  SDspi = new SPIClass(HSPI);
  SDspi->begin(GRBL_SPI_SCK, GRBL_SPI_MISO, GRBL_SPI_MOSI, GRBL_SPI_SS);
  delay(100); // give the SD card time to initialize before querying it
  bool sdBegan = SD.begin(GRBL_SPI_SS, *SDspi, SD_SPI_FREQ, "/sd", 5);
  delay(100); // give the SD card time to initialize before querying it
  if (sdBegan && (SD.cardType() != CARD_NONE))
  {
    sdMounted = true;
    activeFS = &SD;
    activeFSIsSD = true;
    FILESYSTEM_ACTIVE = true;
    DEBUG_PRINTLN("SD card mounted.");
  }
  else
  {
    DEBUG_PRINTLN("SD card mount failed or card not present.");
    SD.end();
    delete SDspi;
    SDspi = NULL;
  }

#if ENABLE_LITTLEFS
  if (LittleFS.begin(false))
  {
    littleFSMounted = true;
    if (!FILESYSTEM_ACTIVE)
    {
      activeFS = &LittleFS;
      activeFSIsSD = false;
      FILESYSTEM_ACTIVE = true;
    }
    DEBUG_PRINTLN("LittleFS mounted.");
  }
  else
  {
    DEBUG_PRINTLN("LittleFS mount failed.");
  }
#endif

  if (sdMounted)
  {
    removeLegacyRootFiles();
  }

  if (sdMounted)
  {
    DEBUG_PRINT("SD total bytes: ");
    DEBUG_PRINTLN(SD.totalBytes());
    DEBUG_PRINT("SD used bytes: ");
    DEBUG_PRINTLN(SD.usedBytes());
  }
#if ENABLE_LITTLEFS
  if (littleFSMounted)
  {
    DEBUG_PRINT("LittleFS total bytes: ");
    DEBUG_PRINTLN(LittleFS.totalBytes());
    DEBUG_PRINT("LittleFS used bytes: ");
    DEBUG_PRINTLN(LittleFS.usedBytes());
  }
#endif
  return FILESYSTEM_ACTIVE;
}

// Pre-2.13 firmware stored the active profile/calibration at the SD-card root as
// /avg.txt and /calibrate.txt. They are unused now (profiles live under
// /profiles/) but linger on cards upgraded from old releases. Delete them on
// boot so they do not confuse the file editor or waste space.
void removeLegacyRootFiles()
{
  static const char *legacyFiles[] = {"/avg.txt", "/calibrate.txt"};
  for (size_t i = 0; i < sizeof(legacyFiles) / sizeof(legacyFiles[0]); i++)
  {
    if (SD.exists(legacyFiles[i]) && SD.remove(legacyFiles[i]))
    {
      DEBUG_PRINT("Removed legacy file: ");
      DEBUG_PRINTLN(legacyFiles[i]);
    }
  }
}
