static bool screenshotCaptureInProgress = false;
static bool screenshotCaptureActive = false;
static bool screenshotCaptureOk = false;
static String screenshotCaptureError = "";
static int32_t screenshotCaptureNextY = 0;
static lv_area_t screenshotCaptureArea;
static uint8_t screenshotCaptureBuffer[LV_HOR_RES_MAX * LV_DRAW_BUF_ROWS * 3];
static const uint8_t screenshotBmpPadding[3] = {0, 0, 0};

static void screenshotPutLE16(uint8_t *buffer, size_t &offset, uint16_t value)
{
  buffer[offset++] = (uint8_t)(value & 0xFF);
  buffer[offset++] = (uint8_t)((value >> 8) & 0xFF);
}

static void screenshotPutLE32(uint8_t *buffer, size_t &offset, uint32_t value)
{
  buffer[offset++] = (uint8_t)(value & 0xFF);
  buffer[offset++] = (uint8_t)((value >> 8) & 0xFF);
  buffer[offset++] = (uint8_t)((value >> 16) & 0xFF);
  buffer[offset++] = (uint8_t)((value >> 24) & 0xFF);
}

static void screenshotSendBytes(const uint8_t *data, size_t length)
{
  server.sendContent((const char *)data, length);
}

static void screenshotSendBmpHeader(uint32_t width, uint32_t height, uint32_t rowSize, uint32_t pixelDataSize)
{
  uint8_t header[54];
  size_t offset = 0;
  screenshotPutLE16(header, offset, 0x4D42);                         // BMP signature
  screenshotPutLE32(header, offset, 54U + pixelDataSize);             // file size
  screenshotPutLE32(header, offset, 0);                               // reserved
  screenshotPutLE32(header, offset, 54);                              // pixel data offset
  screenshotPutLE32(header, offset, 40);                              // DIB header size
  screenshotPutLE32(header, offset, width);
  screenshotPutLE32(header, offset, (uint32_t)(int32_t)(-((int32_t)height)));
  screenshotPutLE16(header, offset, 1);                               // planes
  screenshotPutLE16(header, offset, 24);                              // bits per pixel
  screenshotPutLE32(header, offset, 0);                               // BI_RGB, no compression
  screenshotPutLE32(header, offset, pixelDataSize);
  screenshotPutLE32(header, offset, 2835);                            // 72 DPI horizontal
  screenshotPutLE32(header, offset, 2835);                            // 72 DPI vertical
  screenshotPutLE32(header, offset, 0);                               // colors used
  screenshotPutLE32(header, offset, 0);                               // important colors
  screenshotSendBytes(header, sizeof(header));
}

static void screenshotSetError(const String &message)
{
  screenshotCaptureError = message;
  screenshotCaptureOk = false;
  screenshotCaptureActive = false;
  DEBUG_PRINTLN(message);
}

void screenshotExpandInvalidatedArea(lv_event_t *event)
{
  if (!screenshotCaptureActive)
  {
    return;
  }

  lv_area_t *area = lv_event_get_invalidated_area(event);
  if (area == NULL)
  {
    return;
  }

  area->x1 = screenshotCaptureArea.x1;
  area->y1 = screenshotCaptureArea.y1;
  area->x2 = screenshotCaptureArea.x2;
  area->y2 = screenshotCaptureArea.y2;
}

void screenshotCaptureFlush(const lv_area_t *area, uint8_t *px_map)
{
  if (!screenshotCaptureActive)
  {
    return;
  }

  if ((area == NULL) || (px_map == NULL) ||
      (area->x1 < 0) || (area->y1 < 0) ||
      (area->x2 >= LV_HOR_RES_MAX) || (area->y2 >= LV_VER_RES_MAX) ||
      (area->x2 < area->x1) || (area->y2 < area->y1))
  {
    screenshotSetError("LVGL flushed an invalid screenshot area");
    return;
  }

  if ((area->x1 != 0) || (area->x2 != (LV_HOR_RES_MAX - 1)) || (area->y1 != screenshotCaptureNextY))
  {
    screenshotSetError(String("Unexpected LVGL screenshot area x") + String(area->x1) + "-" + String(area->x2) +
                       " y" + String(area->y1) + "-" + String(area->y2) +
                       ", expected full-width row " + String(screenshotCaptureNextY));
    return;
  }

  const uint32_t width = (uint32_t)LV_HOR_RES_MAX;
  const uint32_t height = (uint32_t)(area->y2 - area->y1 + 1);
  const uint32_t rowSize = (width * 3U + 3U) & ~3U;
  const uint32_t rowPadding = rowSize - (width * 3U);
  if (height > LV_DRAW_BUF_ROWS)
  {
    screenshotSetError(String("LVGL screenshot area too tall: ") + String(height));
    return;
  }

  const uint16_t *pixels = (const uint16_t *)px_map;

  for (uint32_t row = 0; row < height; row++)
  {
    uint8_t *output = screenshotCaptureBuffer + (row * width * 3U);
    for (uint32_t x = 0; x < width; x++)
    {
      uint16_t color = pixels[(row * width) + x];
      uint8_t r = (uint8_t)((((color >> 11) & 0x1F) * 255U) / 31U);
      uint8_t g = (uint8_t)((((color >> 5) & 0x3F) * 255U) / 63U);
      uint8_t b = (uint8_t)(((color & 0x1F) * 255U) / 31U);
      output[(x * 3U) + 0] = b;
      output[(x * 3U) + 1] = g;
      output[(x * 3U) + 2] = r;
    }
  }

  if (rowPadding == 0)
  {
    screenshotSendBytes(screenshotCaptureBuffer, width * height * 3U);
  }
  else
  {
    for (uint32_t row = 0; row < height; row++)
    {
      screenshotSendBytes(screenshotCaptureBuffer + (row * width * 3U), width * 3U);
      screenshotSendBytes(screenshotBmpPadding, rowPadding);
    }
  }

  screenshotCaptureNextY += (int32_t)height;
}

void handleScreenshot()
{
  if (screenshotCaptureInProgress)
  {
    server.send(503, "text/plain", "Capture in progress, try again");
    return;
  }

  screenshotCaptureInProgress = true;
  screenshotCaptureActive = false;
  screenshotCaptureOk = true;
  screenshotCaptureError = "";
  screenshotCaptureNextY = 0;

  if (!lvglLock())
  {
    screenshotCaptureInProgress = false;
    server.send(500, "text/plain", "Failed to lock LVGL for screenshot");
    return;
  }

  const uint32_t width = (uint32_t)LV_HOR_RES_MAX;
  const uint32_t height = (uint32_t)LV_VER_RES_MAX;
  const uint32_t rowSize = (width * 3U + 3U) & ~3U;
  const uint32_t pixelDataSize = rowSize * height;

  server.sendHeader("Content-Disposition", "inline; filename=\"screenshot.bmp\"");
  server.sendHeader("Cache-Control", "no-cache, no-store");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "image/bmp", "");
  screenshotSendBmpHeader(width, height, rowSize, pixelDataSize);

  screenshotCaptureActive = true;
  for (uint32_t y = 0; screenshotCaptureOk && (y < height); y += LV_DRAW_BUF_ROWS)
  {
    screenshotCaptureArea.x1 = 0;
    screenshotCaptureArea.y1 = (lv_coord_t)y;
    screenshotCaptureArea.x2 = (lv_coord_t)(width - 1U);
    screenshotCaptureArea.y2 = (lv_coord_t)(min(y + (uint32_t)LV_DRAW_BUF_ROWS, height) - 1U);

    lv_obj_invalidate_area(lv_screen_active(), &screenshotCaptureArea);
    lv_refr_now(NULL);
  }
  screenshotCaptureActive = false;
  lvglUnlock();

  if (screenshotCaptureOk && (screenshotCaptureNextY != (int32_t)height))
  {
    screenshotSetError(String("LVGL rendered ") + String(screenshotCaptureNextY) + " screenshot rows, expected " + String(height));
  }

  server.sendContent("");
  server.setContentLength(CONTENT_LENGTH_NOT_SET);

  screenshotCaptureInProgress = false;
}
