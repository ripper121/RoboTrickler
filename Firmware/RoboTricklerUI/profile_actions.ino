extern String tempProfile;
extern float tempTargetWeight;
extern int profileListCounter;
extern volatile bool messageBoxOpen;
bool profileDeleteConfirmPending = false;
String profileDeleteName = "";
String profileDeleteFilename = "";

// The profile tune dialog (its state, widgets, and event callbacks) lives in
// ui_dialogs.ino next to the other dialogs. Only the profile data side stays
// here. deleteSelectedProfile() reaches into it via the non-static helpers
// isProfileTuneDialogOpen()/closeProfileTuneDialog()/clearProfileTuneState().

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
    errorBox(message, true);
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

    cancelInteractiveDialogs();

    strlcpy(config.profile,          // <- destination
            profileListBuff[num],    // <- source
            sizeof(config.profile)); // <- destination's capacity
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
    updateTargetWeightLabel();
    infoText = String(langText("status_profile_ready")) + config.profile;
    updateDisplayLog(infoText, true);
}

int findProfileIndex(const char *profileName)
{
    for (int i = 0; i < profileListCount; i++)
    {
        if (strcmp(profileListBuff[i], profileName) == 0)
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
        errorBox(langText("msg_cannot_delete_calibrate_profile"), false);
        return false;
    }

    String filename = profileFilename(profileName.c_str());
    if (!ACTIVE_FS.exists(filename.c_str()))
    {
        errorBox(String(langText("msg_delete_profile_file_not_found")) + filename, false);
        getProfileList();
        return false;
    }

    profileDeleteName = profileName;
    profileDeleteFilename = filename;
    profileDeleteConfirmPending = true;
    showConfirmBox(String(langText("msg_delete_profile_confirm_prefix")) + profileName + langText("msg_delete_profile_confirm_suffix"), UI_FONT_NORMAL, lv_color_hex(0xFFFFFF));
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
        errorBox(langText("msg_cannot_delete_profile"), false);
        return;
    }

    if (!ACTIVE_FS.exists(filename.c_str()))
    {
        errorBox(String(langText("msg_delete_profile_file_not_found")) + filename, false);
        getProfileList();
        return;
    }

    int calibrateIndex = findProfileIndex("calibrate");
    if (calibrateIndex < 0)
    {
        errorBox(langText("msg_delete_profile_calibrate_missing"), false);
        return;
    }

    setProfile(calibrateIndex);
    if (strcmp(config.profile, "calibrate") != 0)
    {
        errorBox(langText("msg_delete_profile_calibrate_load_failed"), false);
        return;
    }

    if (!ACTIVE_FS.remove(filename.c_str()))
    {
        errorBox(String(langText("msg_could_not_delete_profile")) + filename, false);
        getProfileList();
        return;
    }

    getProfileList();
    updateProfileActionButtonVisibility();
    successBox(String(langText("msg_profile_deleted")) + profileName, false);
}
