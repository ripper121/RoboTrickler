volatile bool messageBoxOpen = false;
volatile bool confirmBoxOpen = false;
volatile bool confirmBoxResult = false;
lv_obj_t *ui_ButtonMessageNo = NULL;
lv_obj_t *ui_LabelMessageNo = NULL;

// Hide the shared message/confirm panel and dispatch the pending confirm
// actions. `confirmed` is the user's answer (true = OK/Yes, false = No/cancel);
// finishProfileDeleteConfirm/finishFilesystemSyncConfirm early-out when nothing
// is pending, so this is safe for plain message boxes too.
static void dismissMessageDialog(bool confirmed)
{
  confirmBoxResult = confirmed;
  closeDialog(&ui_PanelMessages, false);
  messageBoxOpen = false;
  confirmBoxOpen = false;
  resetConfirmBoxButtons();
  finishProfileDeleteConfirm(confirmed);
  finishFilesystemSyncConfirm(confirmed);
}

void cancelInteractiveDialogs()
{
  if (confirmBoxOpen)
  {
    dismissMessageDialog(false);
  }

  if (isProfileTuneDialogOpen())
  {
    closeProfileTuneDialog();
    clearProfileTuneState();
  }
}

// Block the caller until a dialog button clears the flag. The Core 0 display
// task normally drives LVGL, but it can stall while servicing the web server,
// so pump the handler here too to keep the blocking dialog responsive.
static void pumpUntil(volatile bool &flag)
{
  while (flag)
  {
    if (lvglLock())
    {
      lv_timer_handler();
      lvglUnlock();
    }
    delay(10);
  }
}

// Shared factory for the "button with a centered label" used throughout the
// dialogs. Returns the button; its label is child 0 (lv_obj_get_child(btn, 0)).
static lv_obj_t *createDialogButton(lv_obj_t *parent, int x, int y, int width, int height,
                                    const char *text, const lv_font_t *font, lv_event_cb_t eventCb)
{
  lv_obj_t *button = lv_btn_create(parent);
  lv_obj_set_width(button, width);
  lv_obj_set_height(button, height);
  lv_obj_set_x(button, x);
  lv_obj_set_y(button, y);
  lv_obj_set_align(button, LV_ALIGN_CENTER);
  lv_obj_add_flag(button, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
  lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(button, eventCb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *label = lv_label_create(button);
  lv_obj_set_align(label, LV_ALIGN_CENTER);
  lv_label_set_text_static(label, text);
  lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
  return button;
}

// Lazily build the shared "No" button the first time a confirm box is shown.
static void ensureNoButton()
{
  if (ui_ButtonMessageNo != NULL)
  {
    return;
  }
  ui_ButtonMessageNo = createDialogButton(ui_PanelMessages, 70, 100, 100, 50,
                                          UI_SYMBOL_NO, UI_FONT_LARGE, messageNo_event_cb);
  ui_LabelMessageNo = lv_obj_get_child(ui_ButtonMessageNo, 0);
}

// Shared renderer for both message and confirm boxes. Callers set the
// messageBoxOpen/confirmBoxOpen state flags before calling so the dialog is
// already armed by the time the panel becomes visible.
static void presentDialog(const String &message, const lv_font_t *font,
                          lv_color_t color, bool showNo)
{
  if (!lvglLock())
  {
    return;
  }
  ensureNoButton();
  if (showNo)
  {
    lv_obj_set_x(ui_ButtonMessageOk, -70);
    lv_label_set_text_static(ui_LabelMessageOk, UI_SYMBOL_YES);
    lv_obj_clear_flag(ui_ButtonMessageNo, LV_OBJ_FLAG_HIDDEN);
  }
  else
  {
    lv_obj_add_flag(ui_ButtonMessageNo, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_x(ui_ButtonMessageOk, 0);
    lv_label_set_text_static(ui_LabelMessageOk, UI_SYMBOL_OK);
  }
  lv_obj_set_style_text_font(ui_LabelMessages, font, LV_PART_MAIN);
  lv_obj_set_style_text_color(ui_LabelMessages, color, LV_PART_MAIN);
  lv_label_set_text(ui_LabelMessages, message.c_str());
  showDialog(ui_PanelMessages);
  lvglUnlock();
}

void messageOk_event_cb(lv_event_t *e)
{
  // The OK button is shared by message and confirm boxes. Confirm actions are
  // dispatched after the panel is hidden so follow-up dialogs can open cleanly.
  bool confirmed = confirmBoxOpen;
  dismissMessageDialog(confirmed);
  if (restartNow && !messageBoxOpen)
  {
    delay(1000);
    ESP.restart();
  }
}

void messageNo_event_cb(lv_event_t *e)
{
  dismissMessageDialog(false);
}

void messageBox(const String &message, const lv_font_t *font, lv_color_t color, bool wait)
{
  cancelInteractiveDialogs();
  messageBoxOpen = true;
  presentDialog(message, font, color, false);
  if (wait)
  {
    // Some startup and safety errors intentionally block until the user sees them.
    pumpUntil(messageBoxOpen);
  }
}

// Semantic message-box colors, defined once instead of repeating raw hex at
// every call site.
#define COLOR_MSG_ERROR 0xFF0000
#define COLOR_MSG_SUCCESS 0x00FF00

// Convenience wrappers for the two common message styles. They both use
// UI_FONT_NORMAL; the rare large-font message (over-trickle safety warning)
// still calls messageBox() directly. `wait` blocks until the user dismisses it.
void errorBox(const String &message, bool wait)
{
  messageBox(message, UI_FONT_NORMAL, lv_color_hex(COLOR_MSG_ERROR), wait);
}

void successBox(const String &message, bool wait)
{
  messageBox(message, UI_FONT_NORMAL, lv_color_hex(COLOR_MSG_SUCCESS), wait);
}

bool confirmBox(const String &message, const lv_font_t *font, lv_color_t color)
{
  confirmBoxResult = false;
  showConfirmBox(message, font, color);
  pumpUntil(messageBoxOpen);
  resetConfirmBoxButtons();
  return confirmBoxResult;
}

void showConfirmBox(const String &message, const lv_font_t *font, lv_color_t color)
{
  cancelInteractiveDialogs();
  messageBoxOpen = true;
  confirmBoxOpen = true;
  presentDialog(message, font, color, true);
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
    lv_label_set_text_static(ui_LabelMessageOk, UI_SYMBOL_OK);
    lvglUnlock();
  }
}

// ---------------------------------------------------------------------------
// Profile tune dialog
//
// A custom modal (built lazily) for adjusting a profile's weight-per-rev.
// Moved here from profile_actions.ino so it lives next to the other dialogs;
// the profile data side (load/save/delete) stays in profile_actions.ino.
// isProfileTuneDialogOpen/closeProfileTuneDialog/clearProfileTuneState are
// non-static because deleteSelectedProfile() (profile_actions.ino) calls them.
// ---------------------------------------------------------------------------
String profileTuneName = "";
float profileTuneWeightPerRev = 0.0;
float profileTuneStepSize = 0.001;
byte profileTuneStepIndex = 0;
lv_obj_t *ui_PanelProfileTune = NULL;
lv_obj_t *ui_LabelProfileTuneTitle = NULL;
lv_obj_t *ui_LabelProfileTuneValue = NULL;
lv_obj_t *ui_ButtonProfileTuneStep = NULL;
lv_obj_t *ui_LabelProfileTuneStep = NULL;
lv_obj_t *ui_ButtonProfileTuneMinus = NULL;
lv_obj_t *ui_ButtonProfileTunePlus = NULL;
lv_obj_t *ui_ButtonProfileTuneCancel = NULL;
lv_obj_t *ui_ButtonProfileTuneSave = NULL;

bool isProfileTuneDialogOpen()
{
    bool open = false;
    if ((ui_PanelProfileTune != NULL) && lvglLock())
    {
        open = !lv_obj_has_flag(ui_PanelProfileTune, LV_OBJ_FLAG_HIDDEN);
        lvglUnlock();
    }
    return open;
}

void clearProfileTuneState()
{
    profileTuneName = "";
    profileTuneWeightPerRev = 0.0;
}

static void updateProfileTuneValueLabel()
{
    if (ui_LabelProfileTuneValue != NULL)
    {
        char text[16];
        formatWeight(text, sizeof(text), profileTuneWeightPerRev);
        lv_label_set_text(ui_LabelProfileTuneValue, text);
    }
}

static void updateProfileTuneStepLabel()
{
    if (profileTuneStepIndex >= WEIGHT_STEP_COUNT)
    {
        profileTuneStepIndex = 0;
    }
    profileTuneStepSize = WEIGHT_STEP_SIZES[profileTuneStepIndex];
    if (ui_LabelProfileTuneStep != NULL)
    {
        char text[16];
        formatWeight(text, sizeof(text), profileTuneStepSize);
        lv_label_set_text(ui_LabelProfileTuneStep, text);
    }
}

static float currentProfileTuneWeightPerRev()
{
    if (config.profileStepperWeightPerRev[1] > 0.0)
    {
        return config.profileStepperWeightPerRev[1];
    }
    return WEIGHT_RESOLUTION;
}

void closeProfileTuneDialog()
{
    closeDialog(&ui_PanelProfileTune, true);
    ui_LabelProfileTuneTitle = NULL;
    ui_LabelProfileTuneValue = NULL;
    ui_ButtonProfileTuneStep = NULL;
    ui_LabelProfileTuneStep = NULL;
    ui_ButtonProfileTuneMinus = NULL;
    ui_ButtonProfileTunePlus = NULL;
    ui_ButtonProfileTuneCancel = NULL;
    ui_ButtonProfileTuneSave = NULL;
}

static void saveProfileTune()
{
    String profileName = profileTuneName;
    if (ui_LabelProfileTuneValue != NULL)
    {
        profileTuneWeightPerRev = String(lv_label_get_text(ui_LabelProfileTuneValue)).toFloat();
    }
    float weightPerRev = profileTuneWeightPerRev;
    closeProfileTuneDialog();
    profileTuneName = "";
    profileTuneWeightPerRev = 0.0;

    if (isTricklerRunning())
    {
        errorBox(langText("msg_stop_trickler_before_tune_profile"), false);
        return;
    }

    if ((profileName.length() <= 0) || (profileName == CALIBRATE_PROFILE_NAME) || (weightPerRev <= 0.0))
    {
        errorBox(langText("msg_cannot_tune_profile"), false);
        return;
    }

    updateDisplayLog(String(langText("status_tuning_profile")) + profileName, true);
    if (!tuneProfileWeightPerRev(profileName.c_str(), weightPerRev))
    {
        String errorText = getSdReadError();
        if (errorText.length() <= 0)
        {
            errorText = langText("msg_could_not_tune_profile");
        }
        errorBox(errorText, false);
        return;
    }

    if (!loadSelectedProfile(false))
    {
        return;
    }

    setLabelText(ui_LabelProfile, config.profileName);
    updateProfileActionButtonVisibility();
    updateTargetWeightLabel();
    successBox(String(langText("msg_profile_tuned")) + profileName, false);
}

void profileTuneMinus_event_cb(lv_event_t *e)
{
    profileTuneWeightPerRev -= profileTuneStepSize;
    if (profileTuneWeightPerRev < WEIGHT_RESOLUTION)
    {
        profileTuneWeightPerRev = WEIGHT_RESOLUTION;
    }
    updateProfileTuneValueLabel();
}

void profileTunePlus_event_cb(lv_event_t *e)
{
    profileTuneWeightPerRev += profileTuneStepSize;
    if (profileTuneWeightPerRev > 99.9999)
    {
        profileTuneWeightPerRev = 99.9999;
    }
    updateProfileTuneValueLabel();
}

void profileTuneStep_event_cb(lv_event_t *e)
{
    profileTuneStepIndex++;
    if (profileTuneStepIndex >= WEIGHT_STEP_COUNT)
    {
        profileTuneStepIndex = 0;
    }
    updateProfileTuneStepLabel();
}

void profileTuneCancel_event_cb(lv_event_t *e)
{
    closeProfileTuneDialog();
    clearProfileTuneState();
}

void profileTuneSave_event_cb(lv_event_t *e)
{
    saveProfileTune();
}

static void createProfileTuneDialog()
{
    if (ui_PanelProfileTune != NULL)
    {
        return;
    }

    ui_PanelProfileTune = lv_obj_create(ui_Screen1);
    lv_obj_set_width(ui_PanelProfileTune, lv_pct(86));
    lv_obj_set_height(ui_PanelProfileTune, lv_pct(84));
    lv_obj_set_align(ui_PanelProfileTune, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_PanelProfileTune, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_PanelProfileTune, LV_OBJ_FLAG_SCROLLABLE);

    ui_LabelProfileTuneTitle = lv_label_create(ui_PanelProfileTune);
    lv_obj_set_width(ui_LabelProfileTuneTitle, lv_pct(100));
    lv_obj_set_height(ui_LabelProfileTuneTitle, LV_SIZE_CONTENT);
    lv_obj_set_y(ui_LabelProfileTuneTitle, -95);
    lv_obj_set_align(ui_LabelProfileTuneTitle, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelProfileTuneTitle, langText("msg_tune_profile_title"));
    lv_obj_set_style_text_align(ui_LabelProfileTuneTitle, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_LabelProfileTuneTitle, UI_FONT_NORMAL, LV_PART_MAIN);

    ui_LabelProfileTuneValue = lv_label_create(ui_PanelProfileTune);
    lv_obj_set_width(ui_LabelProfileTuneValue, 140);
    lv_obj_set_height(ui_LabelProfileTuneValue, 50);
    lv_obj_set_y(ui_LabelProfileTuneValue, -42);
    lv_obj_set_align(ui_LabelProfileTuneValue, LV_ALIGN_CENTER);
    lv_label_set_text_static(ui_LabelProfileTuneValue, "0.0000");
    lv_obj_set_style_text_align(ui_LabelProfileTuneValue, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_LabelProfileTuneValue, UI_FONT_LARGE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_LabelProfileTuneValue, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_LabelProfileTuneValue, 255, LV_PART_MAIN);
    lv_obj_set_style_text_color(ui_LabelProfileTuneValue, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_border_color(ui_LabelProfileTuneValue, lv_color_hex(0x808080), LV_PART_MAIN);
    lv_obj_set_style_border_width(ui_LabelProfileTuneValue, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_top(ui_LabelProfileTuneValue, 5, LV_PART_MAIN);
    lv_obj_clear_flag(ui_LabelProfileTuneValue, LV_OBJ_FLAG_SCROLLABLE);

    ui_ButtonProfileTuneMinus = createDialogButton(ui_PanelProfileTune, 115, -42, 60, 45, "-", UI_FONT_NORMAL, profileTuneMinus_event_cb);
    ui_ButtonProfileTunePlus = createDialogButton(ui_PanelProfileTune, -115, -42, 60, 45, "+", UI_FONT_NORMAL, profileTunePlus_event_cb);
    ui_ButtonProfileTuneStep = createDialogButton(ui_PanelProfileTune, 0, 22, 110, 45, "0.0001", UI_FONT_NORMAL, profileTuneStep_event_cb);
    ui_LabelProfileTuneStep = lv_obj_get_child(ui_ButtonProfileTuneStep, 0);
    ui_ButtonProfileTuneCancel = createDialogButton(ui_PanelProfileTune, 70, 88, 110, 45, UI_SYMBOL_CANCEL, UI_FONT_NORMAL, profileTuneCancel_event_cb);
    ui_ButtonProfileTuneSave = createDialogButton(ui_PanelProfileTune, -70, 88, 110, 45, UI_SYMBOL_SAVE, UI_FONT_NORMAL, profileTuneSave_event_cb);
}

static void showProfileTuneDialog()
{
    if (lvglLock())
    {
        createProfileTuneDialog();
        profileTuneWeightPerRev = currentProfileTuneWeightPerRev();
        updateProfileTuneStepLabel();
        const char *title = langText("msg_tune_profile_title");
        if (strcmp(lv_label_get_text(ui_LabelProfileTuneTitle), title) != 0)
        {
            lv_label_set_text(ui_LabelProfileTuneTitle, title);
        }
        updateProfileTuneValueLabel();
        showDialog(ui_PanelProfileTune);
        lvglUnlock();
    }
}

bool tuneSelectedProfile()
{
    if (messageBoxOpen)
    {
        return false;
    }

    if (isTricklerRunning())
    {
        errorBox(langText("msg_stop_trickler_before_tune_profile"), false);
        return false;
    }

    String profileName = config.profileName;
    String filename = profileFilename(profileName.c_str());
    if (!ACTIVE_FS.exists(filename.c_str()))
    {
        errorBox(String(langText("msg_delete_profile_file_not_found")) + filename, false);
        refreshProfileList();
        return false;
    }

    profileTuneName = profileName;
    showProfileTuneDialog();
    return true;
}
