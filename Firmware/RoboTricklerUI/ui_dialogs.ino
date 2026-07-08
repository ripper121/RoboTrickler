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
  lv_obj_set_style_text_font(label, UI_FONT_NORMAL, LV_PART_MAIN);
  return label;
}

// Shared factory for the white bordered value box used by the tune dialogs.
static lv_obj_t *createDialogValueLabel(lv_obj_t *parent, int y)
{
  lv_obj_t *label = lv_label_create(parent);
  lv_obj_set_width(label, 140);
  lv_obj_set_height(label, 50);
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
  ui_ButtonMessageNo = createDialogButton(ui_PanelMessages, 70, 100, 100, 50,
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

  ui_ButtonMessageOk = createDialogButton(ui_PanelMessages, 0, 100, 100, 50,
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
// UI_FONT_NORMAL; the rare large-font message (over-trickle safety warning)
// still calls messageBox() directly. `wait` blocks until the user dismisses it.
void errorBox(const char *message, bool wait)
{
  messageBox(message, UI_FONT_NORMAL, lv_color_hex(COLOR_MSG_ERROR), wait);
}

void errorBox(const String &message, bool wait)
{
  errorBox(message.c_str(), wait);
}

void successBox(const char *message, bool wait)
{
  messageBox(message, UI_FONT_NORMAL, lv_color_hex(COLOR_MSG_SUCCESS), wait);
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
// Profile tune dialogs
//
// Custom modals (built lazily, deleted on close) for tuning a profile. The
// Tune button opens a chooser between two editors: weight-per-rev and the
// per-entry trickleMap measurements. Moved here from profile_actions.ino so
// they live next to the other dialogs; the profile data side (load/save/
// delete) stays in profile_actions.ino.
// isProfileTuneDialogOpen/closeProfileTuneDialog/clearProfileTuneState are
// non-static because deleteSelectedProfile() (profile_actions.ino) calls them;
// they cover all three tune dialogs.
// ---------------------------------------------------------------------------
String profileTuneName = "";
float profileTuneWeightPerRev = 0.0;
float profileTuneStepSize = 0.001;
byte profileTuneStepIndex = 0;
lv_obj_t *ui_PanelProfileTune = NULL;
lv_obj_t *ui_PanelProfileTuneChoice = NULL;
// Working copy of the trickleMap measurements while the editor is open.
int profileMeasTuneValues[PROFILE_MAX_ENTRIES];
int profileMeasTuneCount = 0;
int profileMeasTuneSelected = 0;
lv_obj_t *ui_PanelProfileTuneMeas = NULL;
lv_obj_t *ui_LabelProfileTuneMeasValue = NULL;
lv_obj_t *ui_ButtonProfileTuneMeasEntry = NULL;
lv_obj_t *ui_LabelProfileTuneMeasEntry = NULL;
lv_obj_t *ui_ButtonProfileTuneMeasMinus = NULL;
lv_obj_t *ui_ButtonProfileTuneMeasPlus = NULL;
lv_obj_t *ui_ButtonProfileTuneMeasCancel = NULL;
lv_obj_t *ui_ButtonProfileTuneMeasSave = NULL;
lv_obj_t *ui_LabelProfileTuneValue = NULL;
lv_obj_t *ui_ButtonProfileTuneStep = NULL;
lv_obj_t *ui_LabelProfileTuneStep = NULL;
lv_obj_t *ui_ButtonProfileTuneMinus = NULL;
lv_obj_t *ui_ButtonProfileTunePlus = NULL;
lv_obj_t *ui_ButtonProfileTuneCancel = NULL;
lv_obj_t *ui_ButtonProfileTuneSave = NULL;

bool isProfileTuneDialogOpen()
{
    if ((ui_PanelProfileTune == NULL) && (ui_PanelProfileTuneChoice == NULL) &&
        (ui_PanelProfileTuneMeas == NULL))
    {
        return false;
    }

    bool open = false;
    if (lvglLock())
    {
        lv_obj_t *panels[] = {ui_PanelProfileTune, ui_PanelProfileTuneChoice, ui_PanelProfileTuneMeas};
        for (lv_obj_t *panel : panels)
        {
            if ((panel != NULL) && !lv_obj_has_flag(panel, LV_OBJ_FLAG_HIDDEN))
            {
                open = true;
                break;
            }
        }
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
    ui_LabelProfileTuneValue = NULL;
    ui_ButtonProfileTuneStep = NULL;
    ui_LabelProfileTuneStep = NULL;
    ui_ButtonProfileTuneMinus = NULL;
    ui_ButtonProfileTunePlus = NULL;
    ui_ButtonProfileTuneCancel = NULL;
    ui_ButtonProfileTuneSave = NULL;
    closeDialog(&ui_PanelProfileTuneChoice, true);
    closeDialog(&ui_PanelProfileTuneMeas, true);
    ui_LabelProfileTuneMeasValue = NULL;
    ui_ButtonProfileTuneMeasEntry = NULL;
    ui_LabelProfileTuneMeasEntry = NULL;
    ui_ButtonProfileTuneMeasMinus = NULL;
    ui_ButtonProfileTuneMeasPlus = NULL;
    ui_ButtonProfileTuneMeasCancel = NULL;
    ui_ButtonProfileTuneMeasSave = NULL;
}

// Shared pieces of both tune-save flows (weight/rev and measurements). Every
// save follows: guard checks -> write the profile file -> reload -> confirm.

// Common guards. `valid` carries the dialog-specific value check; the same
// error message covers a bad name and a bad value, as before.
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

    if (!canTuneProfile(profileName, weightPerRev > 0.0))
    {
        return;
    }

    updateDisplayLog(String(langText("status_tuning_profile")) + profileName, true);
    if (!tuneProfileWeightPerRev(profileName.c_str(), weightPerRev))
    {
        reportProfileTuneError();
        return;
    }

    finishProfileTune(profileName);
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

// Shared by the cancel buttons of all three tune dialogs:
// closeProfileTuneDialog() closes whichever of them is open.
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

    ui_PanelProfileTune = createDialogPanel();

    // The panel is deleted on close and rebuilt per show, so the title text is
    // always current; no re-translation on reuse is needed.
    createDialogTitle(ui_PanelProfileTune, -95, langText("msg_tune_profile_title"));

    ui_LabelProfileTuneValue = createDialogValueLabel(ui_PanelProfileTune, -42);

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
        updateProfileTuneValueLabel();
        showDialog(ui_PanelProfileTune);
        lvglUnlock();
    }
}

// --- Measurements editor ----------------------------------------------------
// Structured exactly like the weight-per-rev dialog above: the white value box
// with -/+ edits the selected entry's measurements, and the center button
// (the step-size button's slot) shows the entry's diffWeight and cycles to the
// next trickleMap entry when tapped.

static void updateProfileTuneMeasValueLabel()
{
    if (ui_LabelProfileTuneMeasValue != NULL)
    {
        char text[16];
        snprintf(text, sizeof(text), "%d", profileMeasTuneValues[profileMeasTuneSelected]);
        lv_label_set_text(ui_LabelProfileTuneMeasValue, text);
    }
}

static void updateProfileTuneMeasEntryLabel()
{
    if (profileMeasTuneSelected >= profileMeasTuneCount)
    {
        profileMeasTuneSelected = 0;
    }
    if (ui_LabelProfileTuneMeasEntry != NULL)
    {
        char text[16];
        formatWeight(text, sizeof(text), config.profileDiffWeight[profileMeasTuneSelected]);
        lv_label_set_text(ui_LabelProfileTuneMeasEntry, text);
    }
}

static void saveProfileTuneMeas()
{
    String profileName = profileTuneName;
    if (ui_LabelProfileTuneMeasValue != NULL)
    {
        profileMeasTuneValues[profileMeasTuneSelected] = String(lv_label_get_text(ui_LabelProfileTuneMeasValue)).toInt();
    }
    int values[PROFILE_MAX_ENTRIES];
    memcpy(values, profileMeasTuneValues, sizeof(values));
    int count = profileMeasTuneCount;
    closeProfileTuneDialog();
    profileTuneName = "";

    if (!canTuneProfile(profileName, count > 0))
    {
        return;
    }

    updateDisplayLog(String(langText("status_tuning_profile")) + profileName, true);
    if (!tuneProfileMeasurements(profileName.c_str(), values, count))
    {
        reportProfileTuneError();
        return;
    }

    finishProfileTune(profileName);
}

void profileTuneMeasMinus_event_cb(lv_event_t *e)
{
    profileMeasTuneValues[profileMeasTuneSelected] -= 1;
    if (profileMeasTuneValues[profileMeasTuneSelected] < 0)
    {
        profileMeasTuneValues[profileMeasTuneSelected] = 0;
    }
    updateProfileTuneMeasValueLabel();
}

void profileTuneMeasPlus_event_cb(lv_event_t *e)
{
    profileMeasTuneValues[profileMeasTuneSelected] += 1;
    if (profileMeasTuneValues[profileMeasTuneSelected] > 99)
    {
        profileMeasTuneValues[profileMeasTuneSelected] = 99;
    }
    updateProfileTuneMeasValueLabel();
}

void profileTuneMeasEntry_event_cb(lv_event_t *e)
{
    profileMeasTuneSelected++;
    if (profileMeasTuneSelected >= profileMeasTuneCount)
    {
        profileMeasTuneSelected = 0;
    }
    updateProfileTuneMeasEntryLabel();
    updateProfileTuneMeasValueLabel();
}

void profileTuneMeasSave_event_cb(lv_event_t *e)
{
    saveProfileTuneMeas();
}

static void createProfileTuneMeasDialog()
{
    if (ui_PanelProfileTuneMeas != NULL)
    {
        return;
    }

    ui_PanelProfileTuneMeas = createDialogPanel();

    createDialogTitle(ui_PanelProfileTuneMeas, -95, langText("msg_tune_measurements_title"));

    ui_LabelProfileTuneMeasValue = createDialogValueLabel(ui_PanelProfileTuneMeas, -42);

    ui_ButtonProfileTuneMeasMinus = createDialogButton(ui_PanelProfileTuneMeas, 115, -42, 60, 45, "-", UI_FONT_NORMAL, profileTuneMeasMinus_event_cb);
    ui_ButtonProfileTuneMeasPlus = createDialogButton(ui_PanelProfileTuneMeas, -115, -42, 60, 45, "+", UI_FONT_NORMAL, profileTuneMeasPlus_event_cb);
    // Spans from the inner edge of the "+" button to the inner edge of the "-"
    // button (both 60 wide at +/-115), and uses the value-box font.
    ui_ButtonProfileTuneMeasEntry = createDialogButton(ui_PanelProfileTuneMeas, 0, 22, 170, 45, "0.0000", UI_FONT_LARGE, profileTuneMeasEntry_event_cb);
    ui_LabelProfileTuneMeasEntry = lv_obj_get_child(ui_ButtonProfileTuneMeasEntry, 0);
    ui_ButtonProfileTuneMeasCancel = createDialogButton(ui_PanelProfileTuneMeas, 70, 88, 110, 45, UI_SYMBOL_CANCEL, UI_FONT_NORMAL, profileTuneCancel_event_cb);
    ui_ButtonProfileTuneMeasSave = createDialogButton(ui_PanelProfileTuneMeas, -70, 88, 110, 45, UI_SYMBOL_SAVE, UI_FONT_NORMAL, profileTuneMeasSave_event_cb);
}

static void showProfileTuneMeasDialog()
{
    if (lvglLock())
    {
        createProfileTuneMeasDialog();
        profileMeasTuneCount = config.profileEntryCount;
        if (profileMeasTuneCount > PROFILE_MAX_ENTRIES)
        {
            profileMeasTuneCount = PROFILE_MAX_ENTRIES;
        }
        profileMeasTuneSelected = 0;
        for (int i = 0; i < profileMeasTuneCount; i++)
        {
            profileMeasTuneValues[i] = config.profileMeasurements[i];
        }
        updateProfileTuneMeasEntryLabel();
        updateProfileTuneMeasValueLabel();
        showDialog(ui_PanelProfileTuneMeas);
        lvglUnlock();
    }
}

// --- Tune chooser ------------------------------------------------------------
// Small modal shown by the Tune button: pick which editor to open.
//
// The editor is opened via lv_async_call instead of directly from the button
// callback: closeDialog() frees the chooser with lv_obj_delete_async(), so a
// direct open would allocate the editor while the chooser still occupies the
// LVGL pool. The async call is queued after the pending delete, so by the time
// it runs the chooser memory has been returned to the pool.

static void openProfileTuneWeightAsync(void *param)
{
    (void)param;
    showProfileTuneDialog();
}

static void openProfileTuneMeasAsync(void *param)
{
    (void)param;
    showProfileTuneMeasDialog();
}

void profileTuneChoiceWeight_event_cb(lv_event_t *e)
{
    closeDialog(&ui_PanelProfileTuneChoice, true);
    lv_async_call(openProfileTuneWeightAsync, NULL);
}

void profileTuneChoiceMeas_event_cb(lv_event_t *e)
{
    closeDialog(&ui_PanelProfileTuneChoice, true);
    lv_async_call(openProfileTuneMeasAsync, NULL);
}

static void createProfileTuneChoiceDialog()
{
    if (ui_PanelProfileTuneChoice != NULL)
    {
        return;
    }

    ui_PanelProfileTuneChoice = createDialogPanel();

    createDialogTitle(ui_PanelProfileTuneChoice, -95, langText("msg_tune_choose_title"));

    lv_obj_t *weightButton = createDialogButton(ui_PanelProfileTuneChoice, 0, -30, 200, 50, "", UI_FONT_NORMAL, profileTuneChoiceWeight_event_cb);
    lv_label_set_text(lv_obj_get_child(weightButton, 0), langText("msg_tune_profile_title"));
    lv_obj_t *measButton = createDialogButton(ui_PanelProfileTuneChoice, 0, 30, 200, 50, "", UI_FONT_NORMAL, profileTuneChoiceMeas_event_cb);
    lv_label_set_text(lv_obj_get_child(measButton, 0), langText("msg_tune_measurements_title"));
    createDialogButton(ui_PanelProfileTuneChoice, 0, 90, 110, 45, UI_SYMBOL_CANCEL, UI_FONT_NORMAL, profileTuneCancel_event_cb);
}

static void showProfileTuneChoiceDialog()
{
    if (lvglLock())
    {
        createProfileTuneChoiceDialog();
        showDialog(ui_PanelProfileTuneChoice);
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
    showProfileTuneChoiceDialog();
    return true;
}
