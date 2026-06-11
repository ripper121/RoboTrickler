String infoMessageBuffer[14];

void insertLine(String newLine)
{
    // Shift all lines up by one position
    for (int i = 0; i < (sizeof(infoMessageBuffer) / sizeof(infoMessageBuffer[0])) - 1; i++)
    {
        infoMessageBuffer[i] = infoMessageBuffer[i + 1];
    }
    // Add new line at the bottom
    infoMessageBuffer[(sizeof(infoMessageBuffer) / sizeof(infoMessageBuffer[0])) - 1] = newLine;
}

void setLabelTextColor(lv_obj_t *label, uint32_t colorHex)
{
    if (lvglLock())
    {
        lv_obj_set_style_text_color(label, lv_color_hex(colorHex), LV_PART_MAIN);
        lvglUnlock();
    }
}

void disableTouchGestures()
{
    if (lvglLock())
    {
        lv_obj_t *tabViewContent = lv_tabview_get_content(ui_TabView);
        lv_obj_clear_flag(tabViewContent, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(tabViewContent, LV_DIR_NONE);
        lv_obj_set_scrollbar_mode(tabViewContent, LV_SCROLLBAR_MODE_OFF);
        lvglUnlock();
    }
}

void updateProfileActionButtonVisibility()
{
    if ((ui_ButtonProfileDelete == NULL) || (ui_ButtonProfileTune == NULL))
    {
        return;
    }

    if (lvglLock())
    {
        if (strcmp(config.profile, "calibrate") == 0)
        {
            lv_obj_add_flag(ui_ButtonProfileDelete, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_ButtonProfileTune, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_clear_flag(ui_ButtonProfileDelete, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_ButtonProfileTune, LV_OBJ_FLAG_HIDDEN);
        }
        lvglUnlock();
    }
}

void setLabelText(lv_obj_t *label, const char *text)
{
  if (lvglLock())
  {
    lv_label_set_text(label, text);
    lvglUnlock();
  }
}

void setWeightLabel(lv_obj_t *label)
{
  char text[32];
  int decimals = decPlaces;
  if (decimals < 0)
  {
    decimals = 0;
  }
  else if (decimals > 6)
  {
    decimals = 6;
  }
  snprintf(text, sizeof(text), "%.*f%s", decimals, weight, unit.c_str());
  setLabelText(label, text);
}

void setObjBgColor(lv_obj_t *obj, uint32_t colorHex)
{
  if (lvglLock())
  {
    lv_obj_set_style_bg_color(obj, lv_color_hex(colorHex), LV_PART_MAIN);
    lvglUnlock();
  }
}

void updateDisplayLog(String logOutput, bool noLog = false)
{
  if (!noLog)
  {
    logOutput += "\n";
    insertLine(logOutput);
    refreshLogLabel();
  }
  else
  {
    logOutput.trim();
    setLabelText(ui_LabelInfo, logOutput.c_str());
  }
  DEBUG_PRINT(logOutput);
}

void refreshLogLabel()
{
  String logText = "";
  for (int i = 0; i < (sizeof(infoMessageBuffer) / sizeof(infoMessageBuffer[0])); i++)
  {
    logText += infoMessageBuffer[i];
  }
  setLabelText(ui_LabelLog, logText.c_str());
}

