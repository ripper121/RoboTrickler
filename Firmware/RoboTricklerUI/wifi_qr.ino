#define WIFI_QR_MAX_VERSION 5
#define WIFI_QR_MAX_MODULES (17 + (WIFI_QR_MAX_VERSION * 4))
#define WIFI_QR_MODULE_BYTES ((WIFI_QR_MAX_MODULES * WIFI_QR_MAX_MODULES + 7) / 8)
#define WIFI_QR_BORDER_MODULES 4
#define WIFI_QR_OBJECT_SIZE 150

static lv_obj_t *wifiQrObject = NULL;
static lv_obj_t *wifiQrHintLabel = NULL;
static uint8_t wifiQrModules[WIFI_QR_MODULE_BYTES];
static uint8_t wifiQrPendingModules[WIFI_QR_MODULE_BYTES];
static uint8_t wifiQrModuleCount = 0;
static uint8_t wifiQrPendingModuleCount = 0;
static bool wifiQrDismissed = false;

static bool wifiQrModuleIsSet(const uint8_t *modules, uint8_t moduleCount, int x, int y)
{
  if ((x < 0) || (y < 0) || (x >= moduleCount) || (y >= moduleCount))
  {
    return false;
  }

  size_t bitIndex = ((size_t)y * moduleCount) + x;
  return (modules[bitIndex >> 3] & (1U << (bitIndex & 7))) != 0;
}

static void storeWifiQrModules(esp_qrcode_handle_t qrcode)
{
  int size = esp_qrcode_get_size(qrcode);
  if ((size <= 0) || (size > WIFI_QR_MAX_MODULES))
  {
    wifiQrPendingModuleCount = 0;
    return;
  }

  memset(wifiQrPendingModules, 0, sizeof(wifiQrPendingModules));
  wifiQrPendingModuleCount = (uint8_t)size;
  for (int y = 0; y < size; y++)
  {
    for (int x = 0; x < size; x++)
    {
      if (esp_qrcode_get_module(qrcode, x, y))
      {
        size_t bitIndex = ((size_t)y * size) + x;
        wifiQrPendingModules[bitIndex >> 3] |= 1U << (bitIndex & 7);
      }
    }
  }
}

static void drawWifiQr_event_cb(lv_event_t *e)
{
  if ((wifiQrModuleCount == 0) || (lv_event_get_code(e) != LV_EVENT_DRAW_MAIN_END))
  {
    return;
  }

  lv_obj_t *obj = lv_event_get_target_obj(e);
  lv_layer_t *layer = lv_event_get_layer(e);
  lv_area_t coords;
  lv_obj_get_coords(obj, &coords);

  int totalModules = wifiQrModuleCount + (WIFI_QR_BORDER_MODULES * 2);
  int modulePixels = lv_obj_get_width(obj) / totalModules;
  if (modulePixels < 1)
  {
    return;
  }

  int qrPixels = totalModules * modulePixels;
  int originX = coords.x1 + (lv_obj_get_width(obj) - qrPixels) / 2 +
                (WIFI_QR_BORDER_MODULES * modulePixels);
  int originY = coords.y1 + (lv_obj_get_height(obj) - qrPixels) / 2 +
                (WIFI_QR_BORDER_MODULES * modulePixels);

  lv_draw_rect_dsc_t drawDsc;
  lv_draw_rect_dsc_init(&drawDsc);
  drawDsc.bg_color = lv_color_black();
  drawDsc.bg_opa = LV_OPA_COVER;
  drawDsc.border_width = 0;
  drawDsc.radius = 0;

  for (int y = 0; y < wifiQrModuleCount; y++)
  {
    int x = 0;
    while (x < wifiQrModuleCount)
    {
      while ((x < wifiQrModuleCount) &&
             !wifiQrModuleIsSet(wifiQrModules, wifiQrModuleCount, x, y))
      {
        x++;
      }
      int runStart = x;
      while ((x < wifiQrModuleCount) &&
             wifiQrModuleIsSet(wifiQrModules, wifiQrModuleCount, x, y))
      {
        x++;
      }
      if (runStart < x)
      {
        lv_area_t area = {
          originX + (runStart * modulePixels),
          originY + (y * modulePixels),
          originX + (x * modulePixels) - 1,
          originY + ((y + 1) * modulePixels) - 1
        };
        lv_draw_rect(layer, &drawDsc, &area);
      }
    }
  }
}

static void setWifiQrHidden(bool hidden)
{
  if (wifiQrObject != NULL)
  {
    if (hidden)
    {
      lv_obj_add_flag(wifiQrObject, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
      lv_obj_clear_flag(wifiQrObject, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_foreground(wifiQrObject);
    }
  }
  if (wifiQrHintLabel != NULL)
  {
    if (hidden)
    {
      lv_obj_add_flag(wifiQrHintLabel, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
      lv_obj_clear_flag(wifiQrHintLabel, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_foreground(wifiQrHintLabel);
    }
  }
}

static void hideWifiQr_event_cb(lv_event_t *e)
{
  (void)e;
  wifiQrDismissed = true;
  setWifiQrHidden(true);
}

static void showWifiQrFromLog_event_cb(lv_event_t *e)
{
  (void)e;
  if (wifiSetupApActive && (wifiQrObject != NULL) && (wifiQrModuleCount > 0))
  {
    wifiQrDismissed = false;
    setWifiQrHidden(false);
  }
}

static void ensureWifiQrObject()
{
  if ((wifiQrObject != NULL) || (ui_PanelPageInfo == NULL) || (ui_LabelLog == NULL))
  {
    return;
  }

  wifiQrObject = lv_obj_create(ui_PanelPageInfo);
  lv_obj_set_size(wifiQrObject, WIFI_QR_OBJECT_SIZE, WIFI_QR_OBJECT_SIZE);
  lv_obj_align(wifiQrObject, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(wifiQrObject, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(wifiQrObject, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(wifiQrObject, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(wifiQrObject, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(wifiQrObject, 0, LV_PART_MAIN);
  lv_obj_add_event_cb(wifiQrObject, drawWifiQr_event_cb, LV_EVENT_DRAW_MAIN_END, NULL);
  lv_obj_add_event_cb(wifiQrObject, hideWifiQr_event_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(ui_PanelPageInfo, showWifiQrFromLog_event_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_flag(wifiQrObject, LV_OBJ_FLAG_HIDDEN);

  wifiQrHintLabel = lv_label_create(ui_PanelPageInfo);
  lv_label_set_text(wifiQrHintLabel, langText("wifi_qr_click_to_hide"));
  lv_obj_set_style_text_color(wifiQrHintLabel, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_bg_color(wifiQrHintLabel, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(wifiQrHintLabel, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_pad_hor(wifiQrHintLabel, 6, LV_PART_MAIN);
  lv_obj_set_style_pad_ver(wifiQrHintLabel, 2, LV_PART_MAIN);
  lv_obj_set_style_radius(wifiQrHintLabel, 4, LV_PART_MAIN);
  lv_obj_align_to(wifiQrHintLabel, wifiQrObject, LV_ALIGN_OUT_BOTTOM_MID, 0, 6);
  lv_obj_add_flag(wifiQrHintLabel, LV_OBJ_FLAG_HIDDEN);
}

void updateWifiSetupQrCode()
{
  if (!lvglLock())
  {
    return;
  }

  ensureWifiQrObject();
  if ((wifiQrObject == NULL) || !wifiSetupApActive || (wifiQrModuleCount == 0))
  {
    setWifiQrHidden(true);
    lvglUnlock();
    return;
  }

  if (!wifiQrDismissed)
  {
    setWifiQrHidden(false);
    lv_obj_invalidate(wifiQrObject);
  }
  lvglUnlock();
}

bool generateWifiSetupQrCode(const char *ssid, const char *password)
{
  if ((ssid == NULL) || (password == NULL))
  {
    return false;
  }

  char payload[160];
  int payloadLength = snprintf(payload, sizeof(payload), "WIFI:T:WPA;S:%s;P:%s;;", ssid, password);
  if ((payloadLength <= 0) || ((size_t)payloadLength >= sizeof(payload)))
  {
    return false;
  }

  wifiQrPendingModuleCount = 0;
  esp_qrcode_config_t qrConfig = ESP_QRCODE_CONFIG_DEFAULT();
  qrConfig.display_func = storeWifiQrModules;
  qrConfig.max_qrcode_version = WIFI_QR_MAX_VERSION;
  qrConfig.qrcode_ecc_level = ESP_QRCODE_ECC_LOW;
  if ((esp_qrcode_generate(&qrConfig, payload) != ESP_OK) || (wifiQrPendingModuleCount == 0))
  {
    return false;
  }

  if (!lvglLock())
  {
    return false;
  }
  memcpy(wifiQrModules, wifiQrPendingModules, sizeof(wifiQrModules));
  wifiQrModuleCount = wifiQrPendingModuleCount;
  wifiQrDismissed = false;
  lvglUnlock();
  updateWifiSetupQrCode();
  return true;
}
