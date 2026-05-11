bool mountSdCard()
{
  SDspi = new SPIClass(HSPI);
  SDspi->begin(GRBL_SPI_SCK, GRBL_SPI_MISO, GRBL_SPI_MOSI, GRBL_SPI_SS);
  if (!SD.begin(GRBL_SPI_SS, *SDspi, SD_SPI_FREQ, "/sd", 10))
  {
    restart_now = true;
    messageBox(languageText("msg_card_mount_failed"), &lv_font_montserrat_14, lv_color_hex(0xFF0000), true);
    return false;
  }

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE)
  {
    restart_now = true;
    messageBox(languageText("msg_no_sd_card"), &lv_font_montserrat_14, lv_color_hex(0xFF0000), true);
    return false;
  }

  DEBUG_PRINT("SD Card Type: ");
  if (cardType == CARD_MMC)
  {
    DEBUG_PRINTLN("MMC");
  }
  else if (cardType == CARD_SD)
  {
    DEBUG_PRINTLN("SDSC");
  }
  else if (cardType == CARD_SDHC)
  {
    DEBUG_PRINTLN("SDHC");
  }
  else
  {
    DEBUG_PRINTLN("UNKNOWN");
    updateDisplayLog(languageText("status_card_unknown"));
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  DEBUG_PRINT("SD Card Size: ");
  DEBUG_PRINT(cardSize);
  DEBUG_PRINTLN("MB");
  (void)cardSize;
  return true;
}

// Prints the content of a file to the Serial
void printFile(const char *filename)
{
#if DEBUG
  // Open file for reading
  File file = SD.open(filename);

  DEBUG_PRINTLN(filename);

  if (!file)
  {
    DEBUG_PRINTLN("Failed to read file");
    return;
  }

  // Extract each characters by one by one
  while (file.available())
  {
    DEBUG_PRINT((char)file.read());
  }
  DEBUG_PRINTLN();

  // Close the file
  file.close();
#else
  (void)filename;
#endif
}
