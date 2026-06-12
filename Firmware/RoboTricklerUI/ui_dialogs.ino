volatile bool messageBoxOpen = false;
volatile bool confirmBoxOpen = false;
volatile bool confirmBoxResult = false;
lv_obj_t *ui_ButtonMessageNo = NULL;
lv_obj_t *ui_LabelMessageNo = NULL;

static lv_obj_t *ui_ActiveMessageBox = NULL;

static void closeActiveMessageBox()
{
  if (ui_ActiveMessageBox != NULL)
  {
    lv_msgbox_close(ui_ActiveMessageBox);
    ui_ActiveMessageBox = NULL;
  }
  messageBoxOpen = false;
  confirmBoxOpen = false;
}

static void styleMessageBox(lv_obj_t *box, const lv_font_t *font, lv_color_t color)
{
  lv_obj_set_width(box, lv_pct(86));
  lv_obj_set_style_text_font(box, font, LV_PART_MAIN);
  lv_obj_set_style_text_color(box, color, LV_PART_MAIN);
  lv_obj_set_style_radius(box, 4, LV_PART_MAIN);

  lv_obj_t *content = lv_msgbox_get_content(box);
  if (content != NULL)
  {
    lv_obj_set_style_text_align(content, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  }

  lv_obj_t *footer = lv_msgbox_get_footer(box);
  if (footer != NULL)
  {
    lv_obj_set_height(footer, 52);
    lv_obj_set_style_text_color(footer, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  }
}

static lv_obj_t *createMessageButton(lv_obj_t *box, const char *text, lv_event_cb_t eventCb)
{
  lv_obj_t *button = lv_msgbox_add_footer_button(box, text);
  lv_obj_set_width(button, 110);
  lv_obj_set_style_text_font(button, UI_FONT_NORMAL, LV_PART_MAIN);
  lv_obj_add_event_cb(button, eventCb, LV_EVENT_CLICKED, NULL);
  return button;
}

void messageOk_event_cb(lv_event_t *e)
{
  bool confirmed = false;
  if (confirmBoxOpen)
  {
    confirmBoxResult = true;
    confirmed = true;
  }

  if (lvglLock())
  {
    closeActiveMessageBox();
    lvglUnlock();
  }

  if (confirmed)
  {
    resetConfirmBoxButtons();
    finishProfileDeleteConfirm(true);
  }

  if (restart_now)
  {
    delay(1000);
    ESP.restart();
  }
}

void messageNo_event_cb(lv_event_t *e)
{
  confirmBoxResult = false;
  if (lvglLock())
  {
    closeActiveMessageBox();
    lvglUnlock();
  }
  resetConfirmBoxButtons();
  finishProfileDeleteConfirm(false);
}

void messageBox(String message, const lv_font_t *font, lv_color_t color, bool wait)
{
  if (lvglLock())
  {
    closeActiveMessageBox();
    ui_ActiveMessageBox = lv_msgbox_create(NULL);
    lv_msgbox_add_text(ui_ActiveMessageBox, message.c_str());
    createMessageButton(ui_ActiveMessageBox, langText("button_ok"), messageOk_event_cb);
    styleMessageBox(ui_ActiveMessageBox, font, color);
    messageBoxOpen = true;
    confirmBoxOpen = false;
    lvglUnlock();
  }

  if (wait)
  {
    // Some startup and safety errors intentionally block until the user sees them.
    while (messageBoxOpen)
    {
      if (lvglLock())
      {
        lv_timer_handler();
        lvglUnlock();
      }
      delay(10);
    }
  }
}

bool confirmBox(String message, const lv_font_t *font, lv_color_t color)
{
  confirmBoxResult = false;
  showConfirmBox(message, font, color);

  while (messageBoxOpen)
  {
    if (lvglLock())
    {
      lv_timer_handler();
      lvglUnlock();
    }
    delay(10);
  }

  resetConfirmBoxButtons();
  return confirmBoxResult;
}

void showConfirmBox(String message, const lv_font_t *font, lv_color_t color)
{
  if (lvglLock())
  {
    closeActiveMessageBox();
    ui_ActiveMessageBox = lv_msgbox_create(NULL);
    lv_msgbox_add_text(ui_ActiveMessageBox, message.c_str());
    createMessageButton(ui_ActiveMessageBox, langText("button_yes"), messageOk_event_cb);
    createMessageButton(ui_ActiveMessageBox, langText("button_no"), messageNo_event_cb);
    styleMessageBox(ui_ActiveMessageBox, font, color);
    messageBoxOpen = true;
    confirmBoxOpen = true;
    confirmBoxResult = false;
    lvglUnlock();
  }
}

void resetConfirmBoxButtons()
{
}

bool isRuntimeMessageBoxOpen()
{
  return ui_ActiveMessageBox != NULL;
}
