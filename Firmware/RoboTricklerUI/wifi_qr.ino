static lv_obj_t *wifiQrOverlay = NULL;

static void closeWifiQrCode(lv_event_t *event)
{
  (void)event;
  if (wifiQrOverlay != NULL)
  {
    lv_obj_delete_async(wifiQrOverlay);
    wifiQrOverlay = NULL;
  }
}

void showWifiQrCode(lv_event_t *event)
{
  (void)event;
  if (!WIFI_SETUP_AP_ACTIVE || (wifiSetupApPassword.length() <= 0) || (wifiQrOverlay != NULL))
  {
    return;
  }

  String payload = "WIFI:T:WPA;S:";
  payload += WIFI_SETUP_AP_SSID;
  payload += ";P:";
  payload += wifiSetupApPassword;
  payload += ";;";

  wifiQrOverlay = lv_obj_create(ui_Screen1);
  lv_obj_set_size(wifiQrOverlay, lv_pct(90), lv_pct(90));
  lv_obj_center(wifiQrOverlay);
  lv_obj_clear_flag(wifiQrOverlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(wifiQrOverlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(wifiQrOverlay, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(wifiQrOverlay, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(wifiQrOverlay, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(wifiQrOverlay, 8, LV_PART_MAIN);
  lv_obj_add_event_cb(wifiQrOverlay, closeWifiQrCode, LV_EVENT_CLICKED, NULL);

  lv_obj_t *qrCode = lv_qrcode_create(wifiQrOverlay);
  lv_qrcode_set_size(qrCode, 170);
  lv_qrcode_set_dark_color(qrCode, lv_color_black());
  lv_qrcode_set_light_color(qrCode, lv_color_white());
  lv_qrcode_set_quiet_zone(qrCode, true);
  lv_qrcode_set_data(qrCode, payload.c_str());
  lv_obj_align(qrCode, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_clear_flag(qrCode, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *hint = lv_label_create(wifiQrOverlay);
  lv_label_set_text(hint, "Scan to connect - tap to close");
  lv_obj_set_style_text_color(hint, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, 0);

  lv_obj_move_to_index(wifiQrOverlay, -1);
}

static void showWifiQrCodeOnInfoTab(lv_event_t *event)
{
  (void)event;
  if (WIFI_SETUP_AP_ACTIVE && (lv_tabview_get_tab_act(ui_TabView) == 2))
  {
    showWifiQrCode(NULL);
  }
}

void initWifiQrCode()
{
  if ((ui_LabelLog == NULL) || (ui_TabView == NULL))
  {
    return;
  }

  lv_obj_add_flag(ui_LabelLog, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(ui_LabelLog, showWifiQrCode, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(ui_TabView, showWifiQrCodeOnInfoTab, LV_EVENT_VALUE_CHANGED, NULL);
}
