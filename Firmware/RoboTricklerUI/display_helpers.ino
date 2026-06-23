// Fixed-size log storage. Kept in BSS instead of the heap so logging cannot
// fragment the heap (the previous String[] + concatenation churned malloc on
// every one of the ~70 log call sites). Lines are truncated to LOG_LINE_LEN.
#define LOG_LINE_COUNT 8
#define LOG_LINE_LEN 48
static char infoMessageBuffer[LOG_LINE_COUNT][LOG_LINE_LEN];
static lv_obj_t *dialogBackdrop = NULL;
static lv_obj_t *activeDialog = NULL;

// Trim leading/trailing whitespace in place (no heap, replaces String::trim()).
static void trimInPlace(char *s)
{
    size_t start = strspn(s, " \t\r\n");
    if (start > 0)
    {
        memmove(s, s + start, strlen(s + start) + 1);
    }
    size_t len = strlen(s);
    while (len > 0 && strchr(" \t\r\n", s[len - 1]) != NULL)
    {
        s[--len] = '\0';
    }
}

void showDialog(lv_obj_t *dialog)
{
    if (dialog == NULL)
    {
        return;
    }

    if ((activeDialog != NULL) && (activeDialog != dialog))
    {
        lv_obj_add_flag(activeDialog, LV_OBJ_FLAG_HIDDEN);
    }

    if (dialogBackdrop == NULL)
    {
        dialogBackdrop = lv_obj_create(ui_Screen1);
        lv_obj_set_size(dialogBackdrop, lv_pct(100), lv_pct(100));
        lv_obj_set_align(dialogBackdrop, LV_ALIGN_CENTER);
        lv_obj_clear_flag(dialogBackdrop, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(dialogBackdrop, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(dialogBackdrop, LV_OPA_40, LV_PART_MAIN);
        lv_obj_set_style_border_width(dialogBackdrop, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(dialogBackdrop, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(dialogBackdrop, 0, LV_PART_MAIN);
    }

    activeDialog = dialog;
    lv_obj_move_to_index(dialogBackdrop, -1);
    lv_obj_clear_flag(dialogBackdrop, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_to_index(dialog, -1);
    lv_obj_clear_flag(dialog, LV_OBJ_FLAG_HIDDEN);
}

void closeDialog(lv_obj_t **dialog, bool deleteAfterClose)
{
    if ((dialog == NULL) || (*dialog == NULL) || !lvglLock())
    {
        return;
    }

    if ((dialogBackdrop != NULL) && (*dialog == activeDialog))
    {
        lv_obj_add_flag(dialogBackdrop, LV_OBJ_FLAG_HIDDEN);
        activeDialog = NULL;
    }

    if (deleteAfterClose)
    {
        lv_obj_delete_async(*dialog);
        *dialog = NULL;
    }
    else
    {
        lv_obj_add_flag(*dialog, LV_OBJ_FLAG_HIDDEN);
    }
    lvglUnlock();
}

void insertLine(const char *newLine)
{
    // Shift all lines up by one position (contiguous 2D array -> single memmove).
    memmove(infoMessageBuffer[0], infoMessageBuffer[1],
            (LOG_LINE_COUNT - 1) * LOG_LINE_LEN);
    // Add new line at the bottom
    strncpy(infoMessageBuffer[LOG_LINE_COUNT - 1], newLine, LOG_LINE_LEN - 1);
    infoMessageBuffer[LOG_LINE_COUNT - 1][LOG_LINE_LEN - 1] = '\0';
}

void setLabelTextColor(lv_obj_t *label, uint32_t colorHex)
{
    if (lvglLock())
    {
        lv_color_t color = lv_color_hex(colorHex);
        if (!lv_color_eq(lv_obj_get_style_text_color(label, LV_PART_MAIN), color))
        {
            lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
        }
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

void setProfileTabEnabled(bool enabled)
{
    if ((ui_TabView == NULL) || !lvglLock())
    {
        return;
    }

    lv_obj_t *profileTabButton = lv_tabview_get_tab_button(ui_TabView, 1);
    lv_obj_t *infoTabButton = lv_tabview_get_tab_button(ui_TabView, 2);
    if (!enabled)
    {
        lv_tabview_set_active(ui_TabView, 0, LV_ANIM_OFF);
    }

    lv_obj_t *blockedTabButtons[] = {profileTabButton, infoTabButton};
    for (lv_obj_t *tabButton : blockedTabButtons)
    {
        if (tabButton == NULL)
        {
            continue;
        }

        if (enabled)
        {
            lv_obj_remove_state(tabButton, LV_STATE_DISABLED);
        }
        else
        {
            lv_obj_add_state(tabButton, LV_STATE_DISABLED);
        }
    }
    lvglUnlock();
}

void updateWifiTabIndicator(bool forceUpdate)
{
    static bool indicatorInitialized = false;
    static bool wasConnected = false;
    bool isConnected = WiFi.status() == WL_CONNECTED;

    if ((ui_TabView == NULL) ||
        (!forceUpdate && indicatorInitialized && (isConnected == wasConnected)))
    {
        return;
    }

    char tabText[48];
    strlcpy(tabText, langText("tab_info"), sizeof(tabText));
    if (isConnected)
    {
        strlcat(tabText, " " UI_SYMBOL_WIFI, sizeof(tabText));
    }
    if (activeFSIsSD)
    {
        strlcat(tabText, " " UI_SYMBOL_SD_CARD, sizeof(tabText));
    }
    else
    {
        strlcat(tabText, " " UI_SYMBOL_FLASH, sizeof(tabText));
    }

    if (lvglLock())
    {
        lv_tabview_set_tab_text(ui_TabView, 2, tabText);
        lvglUnlock();
        indicatorInitialized = true;
        wasConnected = isConnected;
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
        if (strcmp(config.profileName, "calibrate") == 0)
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
  if ((label == NULL) || (text == NULL) || !lvglLock())
  {
    return;
  }

  const char *currentText = lv_label_get_text(label);
  if ((currentText == NULL) || (strcmp(currentText, text) != 0))
  {
    lv_label_set_text(label, text);
  }
  lvglUnlock();
}

// The target weight label is rewritten from config.targetWeight in several
// places (boot, +/- buttons, profile load/tune); keep that formatting in one spot.
void updateTargetWeightLabel()
{
  char text[16];
  snprintf(text, sizeof(text), "%.3f", config.targetWeight);
  setLabelText(ui_LabelTarget, text);
}

void setWeightLabel(lv_obj_t *label)
{
  char text[32];
  if (!isfinite(weight))
  {
    setLabelText(label, "NaN...");
    return;
  }

  int decimals = decimalPlaces;
  if (decimals < 0)
  {
    decimals = 0;
  }
  else if (decimals > 6)
  {
    decimals = 6;
  }
  snprintf(text, sizeof(text), "%.*f%s", decimals, weight, weightUnit.c_str());
  setLabelText(label, text);
}

void setObjBgColor(lv_obj_t *obj, uint32_t colorHex)
{
  if (lvglLock())
  {
    lv_color_t color = lv_color_hex(colorHex);
    if (!lv_color_eq(lv_obj_get_style_bg_color(obj, LV_PART_MAIN), color))
    {
      lv_obj_set_style_bg_color(obj, color, LV_PART_MAIN);
    }
    lvglUnlock();
  }
}

void updateDisplayLog(const String &logOutput, bool noLog = false)
{
  if (!noLog)
  {
    if (lvglLock())
    {
      // Copy the message, reserving the last two slots for the trailing
      // newline and null terminator. The label (LV_LABEL_LONG_CLIP) clips
      // overflow, so we only need to ensure each entry ends with exactly one
      // "\n" to keep it on its own row once refreshLogLabel() concatenates them.
      char line[LOG_LINE_LEN];
      snprintf(line, sizeof(line) - 1, "%s", logOutput.c_str());
      size_t len = strlen(line);
      // Strip any newlines already in the message so we end with exactly one.
      while (len > 0 && line[len - 1] == '\n')
      {
        len--;
      }
      line[len] = '\n';
      line[len + 1] = '\0';
      insertLine(line);
      refreshLogLabel();
      lvglUnlock();
    }
  }
  else
  {
    char text[LOG_LINE_LEN];
    snprintf(text, sizeof(text), "%s", logOutput.c_str());
    trimInPlace(text);
    setLabelText(ui_LabelInfo, text);
  }
  DEBUG_PRINTLN(logOutput);
}

void refreshLogLabel()
{
  static char logText[LOG_LINE_COUNT * LOG_LINE_LEN];
  logText[0] = '\0';
  for (int i = 0; i < LOG_LINE_COUNT; i++)
  {
    strncat(logText, infoMessageBuffer[i], sizeof(logText) - strlen(logText) - 1);
  }
  setLabelText(ui_LabelLog, logText);
}

