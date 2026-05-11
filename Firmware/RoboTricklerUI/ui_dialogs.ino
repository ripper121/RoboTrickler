volatile bool messageBoxOpen = false;
volatile bool confirmBoxOpen = false;
volatile bool confirmBoxResult = false;
lv_obj_t *ui_ButtonMessageNo = NULL;
lv_obj_t *ui_LabelButtonMessageNo = NULL;

static void closeMessagePanel();
static void waitForMessageBoxClose();
static void showMessagePanel(const String &message, const lv_font_t *font, lv_color_t color, bool confirmMode);
static void resetMessageButtons();

void message_event_cb(lv_event_t *e)
{
  if (confirmBoxOpen)
  {
    confirmBoxResult = true;
  }
  closeMessagePanel();
  if (restart_now)
  {
    delay(RESTART_DELAY_MS);
    ESP.restart();
  }
}

void confirm_no_event_cb(lv_event_t *e)
{
  confirmBoxResult = false;
  closeMessagePanel();
}

static void closeMessagePanel()
{
  if (!lvglLock())
  {
    return;
  }

  lv_obj_add_flag(ui_PanelMessages, LV_OBJ_FLAG_HIDDEN);
  messageBoxOpen = false;
  confirmBoxOpen = false;
  lvglUnlock();
}

static void waitForMessageBoxClose()
{
  while (messageBoxOpen)
  {
    if (lvglLock())
    {
      lv_timer_handler();
      lvglUnlock();
    }
    delay(UI_MESSAGE_POLL_DELAY_MS);
  }
}

static void ensureConfirmNoButton()
{
  if (ui_ButtonMessageNo != NULL)
  {
    return;
  }

  ui_ButtonMessageNo = lv_btn_create(ui_PanelMessages);
  lv_obj_set_width(ui_ButtonMessageNo, UI_MESSAGE_BUTTON_WIDTH);
  lv_obj_set_height(ui_ButtonMessageNo, UI_MESSAGE_BUTTON_HEIGHT);
  lv_obj_set_x(ui_ButtonMessageNo, UI_CONFIRM_BUTTON_OFFSET);
  lv_obj_set_y(ui_ButtonMessageNo, UI_MESSAGE_BUTTON_Y);
  lv_obj_set_align(ui_ButtonMessageNo, LV_ALIGN_CENTER);
  lv_obj_add_flag(ui_ButtonMessageNo, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
  lv_obj_clear_flag(ui_ButtonMessageNo, LV_OBJ_FLAG_SCROLLABLE);

  ui_LabelButtonMessageNo = lv_label_create(ui_ButtonMessageNo);
  lv_obj_set_width(ui_LabelButtonMessageNo, LV_SIZE_CONTENT);
  lv_obj_set_height(ui_LabelButtonMessageNo, LV_SIZE_CONTENT);
  lv_obj_set_align(ui_LabelButtonMessageNo, LV_ALIGN_CENTER);
  lv_label_set_text(ui_LabelButtonMessageNo, languageText("button_no"));
  lv_obj_add_event_cb(ui_ButtonMessageNo, confirm_no_event_cb, LV_EVENT_CLICKED, NULL);
}

static void configureMessageButtons(bool confirmMode)
{
  if (confirmMode)
  {
    ensureConfirmNoButton();
    lv_obj_set_x(ui_ButtonMessage, -UI_CONFIRM_BUTTON_OFFSET);
    lv_label_set_text(ui_LabelButtonMessage, languageText("button_yes"));
    lv_obj_clear_flag(ui_ButtonMessageNo, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  if (ui_ButtonMessageNo != NULL)
  {
    lv_obj_add_flag(ui_ButtonMessageNo, LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_set_x(ui_ButtonMessage, 0);
  lv_label_set_text(ui_LabelButtonMessage, languageText("button_ok"));
}

static void showMessagePanel(const String &message, const lv_font_t *font, lv_color_t color, bool confirmMode)
{
  configureMessageButtons(confirmMode);
  messageBoxOpen = true;
  confirmBoxOpen = confirmMode;
  lv_obj_set_style_text_font(ui_LabelMessages, font, LV_PART_MAIN);
  lv_obj_set_style_text_color(ui_LabelMessages, color, LV_PART_MAIN);
  lv_label_set_text(ui_LabelMessages, message.c_str());
  lv_obj_clear_flag(ui_PanelMessages, LV_OBJ_FLAG_HIDDEN);
}

static void resetMessageButtons()
{
  if (lvglLock())
  {
    configureMessageButtons(false);
    lvglUnlock();
  }
}

void messageBox(String message, const lv_font_t *font, lv_color_t color, bool wait)
{
  if (lvglLock())
  {
    showMessagePanel(message, font, color, false);
    lvglUnlock();
  }
  if (wait)
  {
    waitForMessageBoxClose();
  }
}

bool confirmBox(String message, const lv_font_t *font, lv_color_t color)
{
  confirmBoxResult = false;
  if (lvglLock())
  {
    showMessagePanel(message, font, color, true);
    lvglUnlock();
  }

  waitForMessageBoxClose();
  resetMessageButtons();
  return confirmBoxResult;
}
