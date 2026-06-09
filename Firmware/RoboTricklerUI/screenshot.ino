static String screenshotLastError = "";
static File screenshotCaptureFile;
static bool screenshotCaptureActive = false;
static bool screenshotCaptureOk = false;
static int32_t screenshotCaptureNextY = 0;
static uint8_t screenshotCaptureBuffer[LV_HOR_RES_MAX * LV_DRAW_BUF_ROWS * 3];

String getScreenshotError()
{
  return screenshotLastError;
}

void screenshotCaptureFlush(const lv_area_t *area, uint8_t *px_map)
{
  if (!screenshotCaptureActive || !screenshotCaptureFile)
  {
    return;
  }

  if ((area == NULL) || (px_map == NULL) ||
      (area->x1 < 0) || (area->y1 < 0) ||
      (area->x2 >= LV_HOR_RES_MAX) || (area->y2 >= LV_VER_RES_MAX) ||
      (area->x2 < area->x1) || (area->y2 < area->y1))
  {
    screenshotLastError = "LVGL flushed an invalid screenshot area";
    screenshotCaptureOk = false;
    screenshotCaptureActive = false;
    return;
  }

  if ((area->x1 != 0) || (area->x2 != (LV_HOR_RES_MAX - 1)) || (area->y1 != screenshotCaptureNextY))
  {
    screenshotLastError = String("Unexpected LVGL screenshot area x") + String(area->x1) + "-" + String(area->x2) +
                          " y" + String(area->y1) + "-" + String(area->y2) +
                          ", expected full-width row " + String(screenshotCaptureNextY);
    screenshotCaptureOk = false;
    screenshotCaptureActive = false;
    return;
  }

  const uint32_t width = (uint32_t)LV_HOR_RES_MAX;
  const uint32_t height = (uint32_t)(area->y2 - area->y1 + 1);
  if (height > LV_DRAW_BUF_ROWS)
  {
    screenshotLastError = String("LVGL screenshot area too tall: ") + String(height);
    screenshotCaptureOk = false;
    screenshotCaptureActive = false;
    return;
  }

  const uint16_t *pixels = (const uint16_t *)px_map;

  for (uint32_t row = 0; row < height; row++)
  {
    for (uint32_t x = 0; x < width; x++)
    {
      uint16_t color = pixels[(row * width) + x];
      uint8_t r = (uint8_t)(((color >> 11) & 0x1F) << 3);
      uint8_t g = (uint8_t)(((color >> 5) & 0x3F) << 2);
      uint8_t b = (uint8_t)((color & 0x1F) << 3);
      uint32_t offset = ((row * width) + x) * 3U;
      screenshotCaptureBuffer[offset] = b;
      screenshotCaptureBuffer[offset + 1] = g;
      screenshotCaptureBuffer[offset + 2] = r;
    }
  }

  size_t bytesToWrite = width * height * 3U;
  size_t bytesWritten = screenshotCaptureFile.write(screenshotCaptureBuffer, bytesToWrite);
  if (bytesWritten != bytesToWrite)
  {
    screenshotLastError = String("Failed to write LVGL BMP rows ") + String(area->y1) + "-" + String(area->y2) +
                          " (" + String(bytesWritten) + "/" + String(bytesToWrite) + " bytes)";
    screenshotCaptureOk = false;
    screenshotCaptureActive = false;
    return;
  }

  screenshotCaptureNextY += (int32_t)height;
}

static void writeLE16(File &file, uint16_t value)
{
  file.write((uint8_t)(value & 0xFF));
  file.write((uint8_t)((value >> 8) & 0xFF));
}

static void writeLE32(File &file, uint32_t value)
{
  file.write((uint8_t)(value & 0xFF));
  file.write((uint8_t)((value >> 8) & 0xFF));
  file.write((uint8_t)((value >> 16) & 0xFF));
  file.write((uint8_t)((value >> 24) & 0xFF));
}

static String nextScreenshotFilename()
{
  if (!SD.exists("/screenshots"))
  {
    if (!SD.mkdir("/screenshots"))
    {
      screenshotLastError = "Failed to create /screenshots directory";
      return String();
    }
  }
  else
  {
    File dir = SD.open("/screenshots");
    if (!dir || !dir.isDirectory())
    {
      if (dir)
      {
        dir.close();
      }
      screenshotLastError = "/screenshots exists but is not a directory";
      return String();
    }
    dir.close();
  }

  char filename[32];
  for (uint16_t i = 0; i < 10000; i++)
  {
    snprintf(filename, sizeof(filename), "/screenshots/screen%04u.bmp", i);
    if (!SD.exists(filename))
    {
      return String(filename);
    }
  }

  screenshotLastError = "No free screenshot filename found";
  return String();
}

bool saveScreenshotToSd(const char *filename)
{
  screenshotLastError = "";
  if ((filename == NULL) || (filename[0] != '/'))
  {
    screenshotLastError = "Screenshot filename must start with /";
    return false;
  }

  const int32_t width = LV_HOR_RES_MAX;
  const int32_t height = LV_VER_RES_MAX;
  const uint32_t rowSize = ((uint32_t)width * 3U + 3U) & ~3U;
  const uint32_t pixelDataSize = rowSize * (uint32_t)height;
  const uint32_t fileSize = 54U + pixelDataSize;

  if (SD.exists(filename))
  {
    SD.remove(filename);
  }

  screenshotCaptureFile = SD.open(filename, FILE_WRITE);
  if (!screenshotCaptureFile)
  {
    screenshotLastError = String("Failed to open ") + filename + " for writing";
    return false;
  }

  writeLE16(screenshotCaptureFile, 0x4D42);       // BMP signature
  writeLE32(screenshotCaptureFile, fileSize);     // file size
  writeLE32(screenshotCaptureFile, 0);            // reserved
  writeLE32(screenshotCaptureFile, 54);           // pixel data offset
  writeLE32(screenshotCaptureFile, 40);           // DIB header size
  writeLE32(screenshotCaptureFile, (uint32_t)width);
  writeLE32(screenshotCaptureFile, (uint32_t)(-height)); // negative height stores rows top-down
  writeLE16(screenshotCaptureFile, 1);            // planes
  writeLE16(screenshotCaptureFile, 24);           // bits per pixel
  writeLE32(screenshotCaptureFile, 0);            // BI_RGB, no compression
  writeLE32(screenshotCaptureFile, pixelDataSize);
  writeLE32(screenshotCaptureFile, 2835);         // 72 DPI horizontal
  writeLE32(screenshotCaptureFile, 2835);         // 72 DPI vertical
  writeLE32(screenshotCaptureFile, 0);            // colors used
  writeLE32(screenshotCaptureFile, 0);            // important colors

  screenshotCaptureNextY = 0;
  screenshotCaptureOk = true;
  screenshotCaptureActive = true;

  if (lvglLock())
  {
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(NULL);
    lvglUnlock();
  }
  else
  {
    screenshotLastError = "Failed to lock LVGL for screenshot";
    screenshotCaptureOk = false;
  }

  screenshotCaptureActive = false;
  if (screenshotCaptureOk && (screenshotCaptureNextY != height))
  {
    screenshotLastError = String("LVGL rendered ") + String(screenshotCaptureNextY) + " screenshot rows, expected " + String(height);
    screenshotCaptureOk = false;
  }

  screenshotCaptureFile.flush();
  screenshotCaptureFile.close();
  if (!screenshotCaptureOk)
  {
    SD.remove(filename);
  }
  return screenshotCaptureOk;
}

String saveScreenshotToSd()
{
  String filename = nextScreenshotFilename();
  if (filename.length() == 0)
  {
    return String();
  }

  if (!saveScreenshotToSd(filename.c_str()))
  {
    return String();
  }

  return filename;
}
