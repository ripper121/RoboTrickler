// Uniform height for all dialog UI elements (buttons and value boxes). The
// factories below apply it, so no dialog element can end up with its own height.
#define DIALOG_ELEMENT_HEIGHT 50

volatile bool messageBoxOpen = false;
volatile bool confirmBoxOpen = false;
volatile bool confirmBoxResult = false;
lv_obj_t *ui_ButtonMessageNo = NULL;
lv_obj_t *ui_LabelMessageNo = NULL;

// Delete the shared message/confirm panel and dispatch the pending confirm
// actions. `confirmed` is the user's answer (true = OK/Yes, false = No/cancel);
// finishProfileDeleteConfirm/finishFilesystemSyncConfirm early-out when nothing
// is pending, so this is safe for plain message boxes too. The panel is built
// fresh on every show (see createMessageDialog) and freed here so it costs no
// LVGL pool memory while closed.
static void dismissMessageDialog(bool confirmed)
{
  confirmBoxResult = confirmed;
  closeDialog(&ui_PanelMessages, true);
  ui_LabelMessages = NULL;
  ui_ButtonMessageOk = NULL;
  ui_LabelMessageOk = NULL;
  ui_ButtonMessageNo = NULL;
  ui_LabelMessageNo = NULL;
  messageBoxOpen = false;
  confirmBoxOpen = false;
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
static lv_obj_t *createDialogButton(lv_obj_t *parent, int x, int y, int width,
                                    const char *text, const lv_font_t *font, lv_event_cb_t eventCb)
{
  lv_obj_t *button = lv_btn_create(parent);
  lv_obj_set_width(button, width);
  lv_obj_set_height(button, DIALOG_ELEMENT_HEIGHT);
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

// Shared factory for the dialog panels so every dialog has the same screen
// size (90% x 90%, centered, hidden until shown, not scrollable).
static lv_obj_t *createDialogPanel()
{
  lv_obj_t *panel = lv_obj_create(ui_Screen1);
  lv_obj_set_width(panel, lv_pct(90));
  lv_obj_set_height(panel, lv_pct(90));
  lv_obj_set_align(panel, LV_ALIGN_CENTER);
  lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  return panel;
}

// Shared factory for the centered title row at the top of a dialog.
static lv_obj_t *createDialogTitle(lv_obj_t *parent, int y, const char *text)
{
  lv_obj_t *label = lv_label_create(parent);
  lv_obj_set_width(label, lv_pct(100));
  lv_obj_set_height(label, LV_SIZE_CONTENT);
  lv_obj_set_y(label, y);
  lv_obj_set_align(label, LV_ALIGN_CENTER);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_font(label, UI_FONT_LARGE, LV_PART_MAIN);
  return label;
}

// Shared factory for the white bordered value box used by the tune dialogs.
static lv_obj_t *createDialogValueLabel(lv_obj_t *parent, int y)
{
  lv_obj_t *label = lv_label_create(parent);
  lv_obj_set_width(label, 140);
  lv_obj_set_height(label, DIALOG_ELEMENT_HEIGHT);
  lv_obj_set_y(label, y);
  lv_obj_set_align(label, LV_ALIGN_CENTER);
  lv_label_set_text_static(label, "");
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_font(label, UI_FONT_LARGE, LV_PART_MAIN);
  lv_obj_set_style_bg_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(label, 255, LV_PART_MAIN);
  lv_obj_set_style_text_color(label, lv_color_hex(0x000000), LV_PART_MAIN);
  lv_obj_set_style_border_color(label, lv_color_hex(0x808080), LV_PART_MAIN);
  lv_obj_set_style_border_width(label, 2, LV_PART_MAIN);
  lv_obj_set_style_pad_top(label, 5, LV_PART_MAIN);
  lv_obj_clear_flag(label, LV_OBJ_FLAG_SCROLLABLE);
  return label;
}

// Lazily build the shared "No" button the first time a confirm box is shown.
static void ensureNoButton()
{
  if (ui_ButtonMessageNo != NULL)
  {
    return;
  }
  ui_ButtonMessageNo = createDialogButton(ui_PanelMessages, 70, 100, 100,
                                          UI_SYMBOL_NO, UI_FONT_LARGE, messageNo_event_cb);
  ui_LabelMessageNo = lv_obj_get_child(ui_ButtonMessageNo, 0);
}

// Lazily build the shared message/confirm panel. Mirrors the tune dialogs'
// lifecycle: created when a box is shown, deleted in dismissMessageDialog(),
// so it occupies LVGL pool memory only while visible. (It used to be a
// persistent panel created by ui_Screen1.c at boot.)
static void createMessageDialog()
{
  if (ui_PanelMessages != NULL)
  {
    return;
  }

  ui_PanelMessages = createDialogPanel();

  ui_ButtonMessageOk = createDialogButton(ui_PanelMessages, 0, 100, 100,
                                          UI_SYMBOL_OK, UI_FONT_LARGE, messageOk_event_cb);
  ui_LabelMessageOk = lv_obj_get_child(ui_ButtonMessageOk, 0);

  ui_LabelMessages = lv_label_create(ui_PanelMessages);
  lv_obj_set_width(ui_LabelMessages, lv_pct(100));
  lv_obj_set_height(ui_LabelMessages, lv_pct(70));
  lv_obj_set_align(ui_LabelMessages, LV_ALIGN_TOP_MID);
  lv_label_set_long_mode(ui_LabelMessages, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_style_text_align(ui_LabelMessages, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
}

// Shared renderer for both message and confirm boxes. Callers set the
// messageBoxOpen/confirmBoxOpen state flags before calling so the dialog is
// already armed by the time the panel becomes visible.
static void presentDialog(const char *message, const lv_font_t *font,
                          lv_color_t color, bool showNo)
{
  if (!lvglLock())
  {
    return;
  }
  createMessageDialog();
  if (showNo)
  {
    ensureNoButton();
    lv_obj_set_x(ui_ButtonMessageOk, -70);
    lv_label_set_text_static(ui_LabelMessageOk, UI_SYMBOL_YES);
    lv_obj_clear_flag(ui_ButtonMessageNo, LV_OBJ_FLAG_HIDDEN);
  }
  else
  {
    // The panel can be reused when one message box replaces another, so undo
    // any confirm-box layout it may still carry.
    if (ui_ButtonMessageNo != NULL)
    {
      lv_obj_add_flag(ui_ButtonMessageNo, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_x(ui_ButtonMessageOk, 0);
    lv_label_set_text_static(ui_LabelMessageOk, UI_SYMBOL_OK);
  }
  lv_obj_set_style_text_font(ui_LabelMessages, font, LV_PART_MAIN);
  lv_obj_set_style_text_color(ui_LabelMessages, color, LV_PART_MAIN);
  lv_label_set_text(ui_LabelMessages, message);
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

void messageBox(const char *message, const lv_font_t *font, lv_color_t color, bool wait)
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

void messageBox(const String &message, const lv_font_t *font, lv_color_t color, bool wait)
{
  messageBox(message.c_str(), font, color, wait);
}

// Semantic message-box colors, defined once instead of repeating raw hex at
// every call site.
#define COLOR_MSG_ERROR 0xFF0000
#define COLOR_MSG_SUCCESS 0x00FF00

// Convenience wrappers for the two common message styles. They both use
// UI_FONT_LARGE; the rare large-font message (over-trickle safety warning)
// still calls messageBox() directly. `wait` blocks until the user dismisses it.
void errorBox(const char *message, bool wait)
{
  messageBox(message, UI_FONT_LARGE, lv_color_hex(COLOR_MSG_ERROR), wait);
}

void errorBox(const String &message, bool wait)
{
  errorBox(message.c_str(), wait);
}

void successBox(const char *message, bool wait)
{
  messageBox(message, UI_FONT_LARGE, lv_color_hex(COLOR_MSG_SUCCESS), wait);
}

void successBox(const String &message, bool wait)
{
  successBox(message.c_str(), wait);
}

bool confirmBox(const String &message, const lv_font_t *font, lv_color_t color)
{
  confirmBoxResult = false;
  showConfirmBox(message, font, color);
  pumpUntil(messageBoxOpen);
  return confirmBoxResult;
}

void showConfirmBox(const String &message, const lv_font_t *font, lv_color_t color)
{
  cancelInteractiveDialogs();
  messageBoxOpen = true;
  confirmBoxOpen = true;
  presentDialog(message.c_str(), font, color, true);
}

// ---------------------------------------------------------------------------
// One lazily allocated editor shared by all profile tuning modes.
// ---------------------------------------------------------------------------
String profileTuneName = "";
float profileTuneWeightPerRev = 0.0;
byte profileTuneStepIndex = 0;
enum ProfileTuneMode
{
    PROFILE_TUNE_WEIGHT,
    PROFILE_TUNE_MEASUREMENTS,
    PROFILE_TUNE_STEPS,
    PROFILE_TUNE_MODE_COUNT
};
byte profileTuneMode = PROFILE_TUNE_WEIGHT;
int profileTuneEntryCount = 0;
int profileTuneSelectedEntry = 0;
int profileTuneMeasurements[PROFILE_MAX_ENTRIES];
long profileTuneSteps[PROFILE_MAX_ENTRIES];
lv_obj_t *ui_PanelProfileTune = NULL;
lv_obj_t *profileTuneTitleLabel = NULL;
lv_obj_t *profileTuneValueLabel = NULL;
lv_obj_t *profileTuneEntryLabel = NULL;

bool isProfileTuneDialogOpen()
{
    bool open = false;
    if (lvglLock())
    {
        open = ui_PanelProfileTune != NULL &&
               !lv_obj_has_flag(ui_PanelProfileTune, LV_OBJ_FLAG_HIDDEN);
        lvglUnlock();
    }
    return open;
}

void clearProfileTuneState()
{
    profileTuneName = "";
    profileTuneWeightPerRev = 0.0;
    profileTuneEntryCount = 0;
    profileTuneSelectedEntry = 0;
}

void closeProfileTuneDialog()
{
    closeDialog(&ui_PanelProfileTune, true);
    profileTuneTitleLabel = NULL;
    profileTuneValueLabel = NULL;
    profileTuneEntryLabel = NULL;
}

static bool canTuneProfile(const String &profileName, bool valid)
{
    if (isTricklerRunning())
    {
        errorBox(langText("msg_stop_trickler_before_tune_profile"), false);
        return false;
    }

    if ((profileName.length() <= 0) || (profileName == CALIBRATE_PROFILE_NAME) || !valid)
    {
        errorBox(langText("msg_cannot_tune_profile"), false);
        return false;
    }

    return true;
}

static void reportProfileTuneError()
{
    String errorText = getSdReadError();
    if (errorText.length() <= 0)
    {
        errorText = langText("msg_could_not_tune_profile");
    }
    errorBox(errorText, false);
}

// Reload the tuned profile so config reflects the new file, and confirm.
static void finishProfileTune(const String &profileName)
{
    if (!loadSelectedProfile(false))
    {
        return;
    }

    setLabelText(ui_LabelProfile, config.profileName);
    updateProfileActionButtonVisibility();
    updateTargetWeightLabel();
    successBox(String(langText("msg_profile_tuned")) + profileName, false);
}


static void updateProfileTuneLabels()
{
    const char *key = profileTuneMode == PROFILE_TUNE_WEIGHT ? "msg_tune_profile_title" :
                      profileTuneMode == PROFILE_TUNE_MEASUREMENTS ? "msg_tune_measurements_title" :
                      "msg_tune_steps_title";
    lv_label_set_text(profileTuneTitleLabel, langText(key));
    char text[16];
    if (profileTuneMode == PROFILE_TUNE_WEIGHT)
    {
        formatWeight(text, sizeof(text), profileTuneWeightPerRev);
    }
    else if (profileTuneMode == PROFILE_TUNE_MEASUREMENTS)
    {
        snprintf(text, sizeof(text), "%d", profileTuneMeasurements[profileTuneSelectedEntry]);
    }
    else
    {
        snprintf(text, sizeof(text), "%ld", profileTuneSteps[profileTuneSelectedEntry]);
    }
    lv_label_set_text(profileTuneValueLabel, text);
    lv_obj_set_style_text_font(profileTuneValueLabel,
                              strlen(text) > 6 ? UI_FONT_NORMAL : UI_FONT_LARGE, LV_PART_MAIN);
    formatWeight(text, sizeof(text), profileTuneMode == PROFILE_TUNE_WEIGHT ?
                 WEIGHT_STEP_SIZES[profileTuneStepIndex] :
                 config.profileDiffWeight[profileTuneSelectedEntry]);
    lv_label_set_text(profileTuneEntryLabel, text);
}

void selectPreviousTuneMode_event_cb(lv_event_t *e)
{
    profileTuneMode = (profileTuneMode + PROFILE_TUNE_MODE_COUNT - 1) % PROFILE_TUNE_MODE_COUNT;
    updateProfileTuneLabels();
}

void selectNextTuneMode_event_cb(lv_event_t *e)
{
    profileTuneMode = (profileTuneMode + 1) % PROFILE_TUNE_MODE_COUNT;
    updateProfileTuneLabels();
}

static void adjustProfileTuneValue(int direction)
{
    if (profileTuneMode == PROFILE_TUNE_WEIGHT)
    {
        profileTuneWeightPerRev += direction * WEIGHT_STEP_SIZES[profileTuneStepIndex];
        profileTuneWeightPerRev = constrain(profileTuneWeightPerRev, WEIGHT_RESOLUTION, 99.999f);
    }
    else if (profileTuneMode == PROFILE_TUNE_MEASUREMENTS)
    {
        profileTuneMeasurements[profileTuneSelectedEntry] =
            constrain(profileTuneMeasurements[profileTuneSelectedEntry] + direction, 0, 99);
    }
    else
    {
        long &value = profileTuneSteps[profileTuneSelectedEntry];
        if ((direction < 0 && value > 1) || (direction > 0 && value < LONG_MAX))
        {
            value += direction;
        }
    }
    updateProfileTuneLabels();
}

void decreaseTuneValue_event_cb(lv_event_t *e)
{
    adjustProfileTuneValue(-1);
}

void increaseTuneValue_event_cb(lv_event_t *e)
{
    adjustProfileTuneValue(1);
}

void selectTuneEntry_event_cb(lv_event_t *e)
{
    if (profileTuneMode == PROFILE_TUNE_WEIGHT)
    {
        profileTuneStepIndex = (profileTuneStepIndex + 1) % WEIGHT_STEP_COUNT;
    }
    else
    {
        profileTuneSelectedEntry = (profileTuneSelectedEntry + 1) % profileTuneEntryCount;
    }
    updateProfileTuneLabels();
}

void cancelProfileTune_event_cb(lv_event_t *e)
{
    closeProfileTuneDialog();
    clearProfileTuneState();
}

void saveProfileTune_event_cb(lv_event_t *e)
{
    String profileName = profileTuneName;
    if (!canTuneProfile(profileName, profileTuneEntryCount > 0 && profileTuneWeightPerRev > 0.0))
    {
        return;
    }
    // Commit every mode together, regardless of which mode is visible.
    closeProfileTuneDialog();
    updateDisplayLog(String(langText("status_tuning_profile")) + profileName, true);
    bool saved = tuneProfileValues(profileName.c_str(), profileTuneWeightPerRev,
                                   profileTuneMeasurements, profileTuneSteps, profileTuneEntryCount);
    clearProfileTuneState();
    if (!saved)
    {
        reportProfileTuneError();
        return;
    }
    finishProfileTune(profileName);
}

static void createProfileTuneDialog()
{
    ui_PanelProfileTune = createDialogPanel();
    profileTuneTitleLabel = createDialogTitle(ui_PanelProfileTune, -95, "");
    lv_obj_set_width(profileTuneTitleLabel, 290);
    createDialogButton(ui_PanelProfileTune, -180, -95, 50, LV_SYMBOL_LEFT, UI_FONT_LARGE, selectPreviousTuneMode_event_cb);
    createDialogButton(ui_PanelProfileTune, 180, -95, 50, LV_SYMBOL_RIGHT, UI_FONT_LARGE, selectNextTuneMode_event_cb);
    profileTuneValueLabel = createDialogValueLabel(ui_PanelProfileTune, -42);
    createDialogButton(ui_PanelProfileTune, 115, -42, 60, "-", UI_FONT_LARGE, decreaseTuneValue_event_cb);
    createDialogButton(ui_PanelProfileTune, -115, -42, 60, "+", UI_FONT_LARGE, increaseTuneValue_event_cb);
    lv_obj_t *entryButton = createDialogButton(ui_PanelProfileTune, 0, 22, 290, "", UI_FONT_LARGE, selectTuneEntry_event_cb);
    profileTuneEntryLabel = lv_obj_get_child(entryButton, 0);
    createDialogButton(ui_PanelProfileTune, 70, 88, 110, UI_SYMBOL_CANCEL, UI_FONT_LARGE, cancelProfileTune_event_cb);
    createDialogButton(ui_PanelProfileTune, -70, 88, 110, UI_SYMBOL_SAVE, UI_FONT_LARGE, saveProfileTune_event_cb);
}

bool tuneSelectedProfile()
{
    if (messageBoxOpen || isProfileTuneDialogOpen())
    {
        return false;
    }
    String profileName = config.profileName;
    if (!canTuneProfile(profileName, config.profileEntryCount > 0))
    {
        return false;
    }
    String filename = profileFilename(profileName.c_str());
    if (!ACTIVE_FS.exists(filename.c_str()))
    {
        errorBox(String(langText("msg_delete_profile_file_not_found")) + filename, false);
        refreshProfileList();
        return false;
    }
    if (!lvglLock())
    {
        return false;
    }
    profileTuneName = profileName;
    profileTuneMode = PROFILE_TUNE_WEIGHT;
    profileTuneWeightPerRev = config.profileStepperWeightPerRev[1] > 0.0 ?
                              config.profileStepperWeightPerRev[1] : WEIGHT_RESOLUTION;
    profileTuneEntryCount = min(config.profileEntryCount, PROFILE_MAX_ENTRIES);
    profileTuneSelectedEntry = 0;
    for (int i = 0; i < profileTuneEntryCount; i++)
    {
        profileTuneMeasurements[i] = config.profileMeasurements[i];
        profileTuneSteps[i] = config.profileSteps[i];
    }
    createProfileTuneDialog();
    updateProfileTuneLabels();
    showDialog(ui_PanelProfileTune);
    lvglUnlock();
    return true;
}
