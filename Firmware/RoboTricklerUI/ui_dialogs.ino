volatile bool messageBoxOpen = false;
volatile bool confirmBoxOpen = false;
volatile bool confirmBoxResult = false;
lv_obj_t *ui_ButtonMessageNo = NULL;
lv_obj_t *ui_LabelMessageNo = NULL;

void messageOk_event_cb(lv_event_t *e)
{
  // The OK button is shared by message and confirm boxes. Confirm actions are
  // dispatched after the panel is hidden so follow-up dialogs can open cleanly.
  bool confirmed = false;
  if (confirmBoxOpen)
  {
    confirmBoxResult = true;
    confirmed = true;
  }
  if (lvglLock())
  {
    lv_obj_add_flag(ui_PanelMessages, LV_OBJ_FLAG_HIDDEN);
    messageBoxOpen = false;
    confirmBoxOpen = false;
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
    lv_obj_add_flag(ui_PanelMessages, LV_OBJ_FLAG_HIDDEN);
    messageBoxOpen = false;
    confirmBoxOpen = false;
    lvglUnlock();
  }
  resetConfirmBoxButtons();
  finishProfileDeleteConfirm(false);
}

void messageBox(String message, const lv_font_t *font, lv_color_t color, bool wait)
{
  if (lvglLock())
  {
    if (ui_ButtonMessageNo != NULL)
    {
      lv_obj_add_flag(ui_ButtonMessageNo, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_x(ui_ButtonMessageOk, 0);
    lv_label_set_text(ui_LabelMessageOk, langText("button_ok"));
    messageBoxOpen = true;
    lv_obj_set_style_text_font(ui_LabelMessages, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(ui_LabelMessages, color, LV_PART_MAIN);
    lv_label_set_text(ui_LabelMessages, message.c_str());
    lv_obj_clear_flag(ui_PanelMessages, LV_OBJ_FLAG_HIDDEN);
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
    if (ui_ButtonMessageNo == NULL)
    {
      ui_ButtonMessageNo = lv_btn_create(ui_PanelMessages);
      lv_obj_set_width(ui_ButtonMessageNo, 100);
      lv_obj_set_height(ui_ButtonMessageNo, 50);
      lv_obj_set_x(ui_ButtonMessageNo, 70);
      lv_obj_set_y(ui_ButtonMessageNo, 45);
      lv_obj_set_align(ui_ButtonMessageNo, LV_ALIGN_CENTER);
      lv_obj_add_flag(ui_ButtonMessageNo, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
      lv_obj_clear_flag(ui_ButtonMessageNo, LV_OBJ_FLAG_SCROLLABLE);

      ui_LabelMessageNo = lv_label_create(ui_ButtonMessageNo);
      lv_obj_set_width(ui_LabelMessageNo, LV_SIZE_CONTENT);
      lv_obj_set_height(ui_LabelMessageNo, LV_SIZE_CONTENT);
      lv_obj_set_align(ui_LabelMessageNo, LV_ALIGN_CENTER);
      lv_label_set_text(ui_LabelMessageNo, langText("button_no"));
      lv_obj_add_event_cb(ui_ButtonMessageNo, messageNo_event_cb, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_set_x(ui_ButtonMessageOk, -70);
    lv_label_set_text(ui_LabelMessageOk, langText("button_yes"));
    lv_obj_clear_flag(ui_ButtonMessageNo, LV_OBJ_FLAG_HIDDEN);
    messageBoxOpen = true;
    confirmBoxOpen = true;
    lv_obj_set_style_text_font(ui_LabelMessages, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(ui_LabelMessages, color, LV_PART_MAIN);
    lv_label_set_text(ui_LabelMessages, message.c_str());
    lv_obj_clear_flag(ui_PanelMessages, LV_OBJ_FLAG_HIDDEN);
    lvglUnlock();
  }
}

void resetConfirmBoxButtons()
{
  if (lvglLock())
  {
    if (ui_ButtonMessageNo != NULL)
    {
      lv_obj_add_flag(ui_ButtonMessageNo, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_x(ui_ButtonMessageOk, 0);
    lv_label_set_text(ui_LabelMessageOk, langText("button_ok"));
    lvglUnlock();
  }
}
