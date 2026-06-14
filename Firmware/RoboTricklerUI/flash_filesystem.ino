bool initFilesystem()
{
  FILESYSTEM_ACTIVE = false;
  activeFS = NULL;
  activeFSIsSD = false;
  sdMounted = false;
  littleFSMounted = false;

  SDspi = new SPIClass(HSPI);
  SDspi->begin(GRBL_SPI_SCK, GRBL_SPI_MISO, GRBL_SPI_MOSI, GRBL_SPI_SS);
  if (SD.begin(GRBL_SPI_SS, *SDspi, SD_SPI_FREQ, "/sd", 10) && (SD.cardType() != CARD_NONE))
  {
    sdMounted = true;
    activeFS = &SD;
    activeFSIsSD = true;
    FILESYSTEM_ACTIVE = true;
    DEBUG_PRINTLN("SD card mounted.");
    initUpdate();
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
