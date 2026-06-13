extern String tempProfile;
extern float tempTargetWeight;
extern int profileListCounter;
extern volatile bool messageBoxOpen;
bool profileDeleteConfirmPending = false;
String profileDeleteName = "";
String profileDeleteFilename = "";
String profileTuneName = "";
float profileTuneUnitsPerThrow = 0.0;
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

static bool isProfileTuneDialogOpen()
{
    bool open = false;
    if ((ui_PanelProfileTune != NULL) && lvglLock())
    {
        open = !lv_obj_has_flag(ui_PanelProfileTune, LV_OBJ_FLAG_HIDDEN);
        lvglUnlock();
    }
    return open;
}

static void clearProfileTuneState()
{
    profileTuneName = "";
    profileTuneUnitsPerThrow = 0.0;
}

static void updateProfileTuneValueLabel()
{
    if (ui_LabelProfileTuneValue != NULL)
    {
        lv_label_set_text(ui_LabelProfileTuneValue, String(profileTuneUnitsPerThrow, 3).c_str());
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
        lv_label_set_text(ui_LabelProfileTuneStep, String(profileTuneStepSize, 3).c_str());
    }
}

static float currentProfileTuneUnitsPerThrow()
{
    if (config.profile_stepperUnitsPerThrow[1] > 0.0)
    {
        return config.profile_stepperUnitsPerThrow[1];
    }
    return 0.001;
}

static void closeProfileTuneDialog()
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
        profileTuneUnitsPerThrow = String(lv_label_get_text(ui_LabelProfileTuneValue)).toFloat();
    }
    float unitsPerThrow = profileTuneUnitsPerThrow;
    closeProfileTuneDialog();
    profileTuneName = "";
    profileTuneUnitsPerThrow = 0.0;

    if (isTricklerRunning())
    {
        messageBox(langText("msg_stop_trickler_before_tune_profile"), UI_FONT_NORMAL, lv_color_hex(0xFF0000), false);
        return;
    }

    if ((profileName.length() <= 0) || (profileName == "calibrate") || (unitsPerThrow <= 0.0))
    {
        messageBox(langText("msg_cannot_tune_profile"), UI_FONT_NORMAL, lv_color_hex(0xFF0000), false);
        return;
    }

    updateDisplayLog(String(langText("status_tuning_profile")) + profileName, true);
    if (!tuneProfileUnitsPerThrow(profileName.c_str(), unitsPerThrow))
    {
        String errorText = getSdReadError();
        if (errorText.length() <= 0)
        {
            errorText = langText("msg_could_not_tune_profile");
        }
        messageBox(errorText, UI_FONT_NORMAL, lv_color_hex(0xFF0000), false);
        return;
    }

    if (!loadSelectedProfile())
    {
        return;
    }

    setLabelText(ui_LabelProfile, config.profile);
    updateProfileActionButtonVisibility();
    setLabelText(ui_LabelTarget, String(config.targetWeight, 3).c_str());
    messageBox(String(langText("msg_profile_tuned")) + profileName, UI_FONT_NORMAL, lv_color_hex(0x00FF00), false);
}

void profile_tune_minus_event_cb(lv_event_t *e)
{
    profileTuneUnitsPerThrow -= profileTuneStepSize;
    if (profileTuneUnitsPerThrow < 0.001)
    {
        profileTuneUnitsPerThrow = 0.001;
    }
    updateProfileTuneValueLabel();
}

void profile_tune_plus_event_cb(lv_event_t *e)
{
    profileTuneUnitsPerThrow += profileTuneStepSize;
    if (profileTuneUnitsPerThrow > 99.999)
    {
        profileTuneUnitsPerThrow = 99.999;
    }
    updateProfileTuneValueLabel();
}

void profile_tune_step_event_cb(lv_event_t *e)
{
    profileTuneStepIndex++;
    if (profileTuneStepIndex >= WEIGHT_STEP_COUNT)
    {
        profileTuneStepIndex = 0;
    }
    updateProfileTuneStepLabel();
}

void profile_tune_cancel_event_cb(lv_event_t *e)
{
    closeProfileTuneDialog();
    clearProfileTuneState();
}

void profile_tune_save_event_cb(lv_event_t *e)
{
    saveProfileTune();
}

void corruptProfile(String profileFilename)
{
    String readError = getSdReadError();
    String message = String(langText("msg_profile_corrupted")) + profileFilename;
    if (readError.length() > 0)
    {
        updateDisplayLog(readError);
        message += "\n\n";
        message += readError;
    }
    message += langText("msg_calibration_profile_loaded");

    // Rename file to indicate corruption
    if (ACTIVE_FS.exists(profileFilename))
    {
        String corruptedName = String(profileFilename);
        corruptedName.replace(".txt", ".cor.txt");
        if (ACTIVE_FS.rename(profileFilename, corruptedName.c_str()))
        {
            DEBUG_PRINT("Corrupted file renamed to: ");
            DEBUG_PRINTLN(corruptedName);
        }
        else
        {
            DEBUG_PRINTLN("Failed to rename corrupted file");
        }
    }

    strlcpy(config.profile,          // <- destination
            "calibrate",             // <- source
            sizeof(config.profile)); // <- destination's capacity
    saveConfiguration("/config.txt", config);
    delay(100);
    restart_now = true;
    messageBox(message.c_str(), UI_FONT_NORMAL, lv_color_hex(0xFF0000), true);
}

bool loadSelectedProfile()
{
    String selectedProfileFilename = profileFilename(config.profile);
    if (!readProfile(selectedProfileFilename.c_str(), config))
    {
        corruptProfile(selectedProfileFilename);
        return false;
    }
    tempProfile = config.profile;
    tempTargetWeight = config.targetWeight;
    return true;
}

void setProfile(int num)
{
    if ((num < 0) || (num >= profileListCount))
    {
        DEBUG_PRINT("Invalid profile number: ");
        DEBUG_PRINTLN(num);
        return;
    }

    strlcpy(config.profile,               // <- destination
            profileListBuff[num].c_str(), // <- source
            sizeof(config.profile));      // <- destination's capacity
    profileListCounter = num;

    String infoText = String(langText("status_selecting_profile")) + config.profile;
    updateDisplayLog(infoText, true);

    saveConfiguration("/config.txt", config);

    if (!loadSelectedProfile())
    {
        return;
    }

    DEBUG_PRINT("num: ");
    DEBUG_PRINTLN(num);

    setLabelText(ui_LabelProfile, config.profile);
    updateProfileActionButtonVisibility();
    setLabelText(ui_LabelTarget, String(config.targetWeight, 3).c_str());
    infoText = String(langText("status_profile_ready")) + config.profile;
    updateDisplayLog(infoText, true);
}

int findProfileIndex(const char *profileName)
{
    for (int i = 0; i < profileListCount; i++)
    {
        if (profileListBuff[i] == profileName)
        {
            return i;
        }
    }
    return -1;
}

bool deleteSelectedProfile()
{
    // Deletion is two-stage so the LVGL confirm dialog can return through its
    // normal event callback instead of blocking the UI task.
    if (messageBoxOpen)
    {
        return false;
    }

    if (isProfileTuneDialogOpen())
    {
        closeProfileTuneDialog();
        clearProfileTuneState();
    }

    String profileName = config.profile;
    if (profileName == "calibrate")
    {
        messageBox(langText("msg_cannot_delete_calibrate_profile"), UI_FONT_NORMAL, lv_color_hex(0xFF0000), false);
        return false;
    }

    String filename = profileFilename(profileName.c_str());
    if (!ACTIVE_FS.exists(filename.c_str()))
    {
        messageBox(String(langText("msg_delete_profile_file_not_found")) + filename, UI_FONT_NORMAL, lv_color_hex(0xFF0000), false);
        getProfileList();
        return false;
    }

    profileDeleteName = profileName;
    profileDeleteFilename = filename;
    profileDeleteConfirmPending = true;
    showConfirmBox(String(langText("msg_delete_profile_confirm_prefix")) + profileName + langText("msg_delete_profile_confirm_suffix"), UI_FONT_NORMAL, lv_color_hex(0xFFFFFF));
    return true;
}

static lv_obj_t *createProfileTuneButton(lv_obj_t *parent, int x, int y, int width, const char *text, lv_event_cb_t eventCb)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_set_width(button, width);
    lv_obj_set_height(button, 45);
    lv_obj_set_x(button, x);
    lv_obj_set_y(button, y);
    lv_obj_set_align(button, LV_ALIGN_CENTER);
    lv_obj_add_flag(button, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(button, eventCb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label = lv_label_create(button);
    lv_obj_set_width(label, LV_SIZE_CONTENT);
    lv_obj_set_height(label, LV_SIZE_CONTENT);
    lv_obj_set_align(label, LV_ALIGN_CENTER);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, UI_FONT_NORMAL, LV_PART_MAIN);
    return button;
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
    lv_label_set_text(ui_LabelProfileTuneValue, "0.000");
    lv_obj_set_style_text_align(ui_LabelProfileTuneValue, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_LabelProfileTuneValue, UI_FONT_LARGE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_LabelProfileTuneValue, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_LabelProfileTuneValue, 255, LV_PART_MAIN);
    lv_obj_set_style_text_color(ui_LabelProfileTuneValue, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_border_color(ui_LabelProfileTuneValue, lv_color_hex(0x808080), LV_PART_MAIN);
    lv_obj_set_style_border_width(ui_LabelProfileTuneValue, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_top(ui_LabelProfileTuneValue, 5, LV_PART_MAIN);
    lv_obj_clear_flag(ui_LabelProfileTuneValue, LV_OBJ_FLAG_SCROLLABLE);

    ui_ButtonProfileTuneMinus = createProfileTuneButton(ui_PanelProfileTune, 115, -42, 60, "-", profile_tune_minus_event_cb);
    ui_ButtonProfileTunePlus = createProfileTuneButton(ui_PanelProfileTune, -115, -42, 60, "+", profile_tune_plus_event_cb);
    ui_ButtonProfileTuneStep = createProfileTuneButton(ui_PanelProfileTune, 0, 22, 110, "0.001", profile_tune_step_event_cb);
    ui_LabelProfileTuneStep = lv_obj_get_child(ui_ButtonProfileTuneStep, 0);
    ui_ButtonProfileTuneCancel = createProfileTuneButton(ui_PanelProfileTune, 70, 88, 110, UI_SYMBOL_CANCEL, profile_tune_cancel_event_cb);
    ui_ButtonProfileTuneSave = createProfileTuneButton(ui_PanelProfileTune, -70, 88, 110, UI_SYMBOL_SAVE, profile_tune_save_event_cb);
}

static void showProfileTuneDialog()
{
    if (lvglLock())
    {
        createProfileTuneDialog();
        profileTuneUnitsPerThrow = currentProfileTuneUnitsPerThrow();
        updateProfileTuneStepLabel();
        lv_label_set_text(ui_LabelProfileTuneTitle, (String(langText("msg_tune_profile_title"))).c_str());
        updateProfileTuneValueLabel();
        lv_obj_move_to_index(ui_PanelProfileTune, -1);
        lv_obj_clear_flag(ui_PanelProfileTune, LV_OBJ_FLAG_HIDDEN);
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
        messageBox(langText("msg_stop_trickler_before_tune_profile"), UI_FONT_NORMAL, lv_color_hex(0xFF0000), false);
        return false;
    }

    String profileName = config.profile;
    String filename = profileFilename(profileName.c_str());
    if (!ACTIVE_FS.exists(filename.c_str()))
    {
        messageBox(String(langText("msg_delete_profile_file_not_found")) + filename, UI_FONT_NORMAL, lv_color_hex(0xFF0000), false);
        getProfileList();
        return false;
    }

    profileTuneName = profileName;
    showProfileTuneDialog();
    return true;
}

void finishProfileDeleteConfirm(bool confirmed)
{
    if (!profileDeleteConfirmPending)
    {
        return;
    }

    String profileName = profileDeleteName;
    String filename = profileDeleteFilename;
    profileDeleteConfirmPending = false;
    profileDeleteName = "";
    profileDeleteFilename = "";

    if (!confirmed)
    {
        return;
    }

    if ((profileName.length() <= 0) || (profileName == "calibrate"))
    {
        messageBox(langText("msg_cannot_delete_profile"), UI_FONT_NORMAL, lv_color_hex(0xFF0000), false);
        return;
    }

    if (!ACTIVE_FS.exists(filename.c_str()))
    {
        messageBox(String(langText("msg_delete_profile_file_not_found")) + filename, UI_FONT_NORMAL, lv_color_hex(0xFF0000), false);
        getProfileList();
        return;
    }

    int calibrateIndex = findProfileIndex("calibrate");
    if (calibrateIndex < 0)
    {
        messageBox(langText("msg_delete_profile_calibrate_missing"), UI_FONT_NORMAL, lv_color_hex(0xFF0000), false);
        return;
    }

    setProfile(calibrateIndex);
    if (strcmp(config.profile, "calibrate") != 0)
    {
        messageBox(langText("msg_delete_profile_calibrate_load_failed"), UI_FONT_NORMAL, lv_color_hex(0xFF0000), false);
        return;
    }

    if (!ACTIVE_FS.remove(filename.c_str()))
    {
        messageBox(String(langText("msg_could_not_delete_profile")) + filename, UI_FONT_NORMAL, lv_color_hex(0xFF0000), false);
        getProfileList();
        return;
    }

    getProfileList();
    updateProfileActionButtonVisibility();
    messageBox(String(langText("msg_profile_deleted")) + profileName, UI_FONT_NORMAL, lv_color_hex(0x00FF00), false);
}
