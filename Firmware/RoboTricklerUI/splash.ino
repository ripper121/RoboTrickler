static uint16_t readLE16(File &file)
{
  uint16_t value = file.read();
  value |= (uint16_t)file.read() << 8;
  return value;
}

static uint32_t readLE32(File &file)
{
  uint32_t value = file.read();
  value |= (uint32_t)file.read() << 8;
  value |= (uint32_t)file.read() << 16;
  value |= (uint32_t)file.read() << 24;
  return value;
}

bool showSplashLogo()
{
  if (!SD.exists(SPLASH_LOGO_PATH))
  {
    DEBUG_PRINT("Splash logo not found: ");
    DEBUG_PRINTLN(SPLASH_LOGO_PATH);
    return false;
  }

  File file = SD.open(SPLASH_LOGO_PATH, FILE_READ);
  if (!file)
  {
    DEBUG_PRINTLN("Failed to open splash logo");
    return false;
  }

  if (readLE16(file) != 0x4D42)
  {
    DEBUG_PRINTLN("Splash logo is not a BMP file");
    file.close();
    return false;
  }

  (void)readLE32(file); // file size
  (void)readLE32(file); // creator bytes
  uint32_t imageOffset = readLE32(file);
  uint32_t headerSize = readLE32(file);
  int32_t width = (int32_t)readLE32(file);
  int32_t height = (int32_t)readLE32(file);
  uint16_t planes = readLE16(file);
  uint16_t bitDepth = readLE16(file);
  uint32_t compression = readLE32(file);

  if ((headerSize < 40) || (planes != 1) || (compression != 0) || ((bitDepth != 24) && (bitDepth != 32)) ||
      (width <= 0) || (height == 0) || (width > LV_HOR_RES_MAX) || (abs(height) > LV_VER_RES_MAX))
  {
    DEBUG_PRINTLN("Unsupported splash BMP format");
    file.close();
    return false;
  }

  static uint8_t rowBuffer[LV_HOR_RES_MAX * 4];
  static uint16_t lineBuffer[LV_HOR_RES_MAX];
  int32_t bmpHeight = abs(height);
  uint32_t bytesPerPixel = bitDepth / 8;
  uint32_t rowSize = ((uint32_t)width * bytesPerPixel + 3) & ~3U;
  int16_t xpos = (LV_HOR_RES_MAX - width) / 2;
  int16_t ypos = (LV_VER_RES_MAX - bmpHeight) / 2;

  tft.fillScreen(TFT_BLACK);
  file.seek(imageOffset);
  tft.startWrite();
  for (int32_t row = 0; row < bmpHeight; row++)
  {
    if (file.read(rowBuffer, rowSize) != rowSize)
    {
      DEBUG_PRINTLN("Failed to read splash BMP row");
      break;
    }

    uint8_t *pixel = rowBuffer;
    for (int32_t x = 0; x < width; x++)
    {
      uint8_t b = pixel[0];
      uint8_t g = pixel[1];
      uint8_t r = pixel[2];
      lineBuffer[x] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
      pixel += bytesPerPixel;
    }

    int32_t y = (height > 0) ? (ypos + bmpHeight - 1 - row) : (ypos + row);
    tft.pushImage(xpos, y, width, 1, lineBuffer);
  }
  tft.endWrite();
  file.close();

  delay(SPLASH_DISPLAY_MS);
  return true;
}
