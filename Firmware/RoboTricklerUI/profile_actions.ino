extern String tempProfile;
extern float tempTargetWeight;
extern int profileListCounter;
bool profileDeleteConfirmPending = false;
String profileDeleteName = "";
String profileDeleteFilename = "";

void corruptProfile(String profile_filename)
{
    String readError = getSdReadError();
    String message = String(langText("msg_profile_corrupted")) + profile_filename;
    if (readError.length() > 0)
    {
        updateDisplayLog(readError);
        message += "\n\n";
        message += readError;
    }
    message += langText("msg_calibration_profile_loaded");

    // Rename file to indicate corruption
    if (SD.exists(profile_filename))
    {
        String corruptedName = String(profile_filename);
        corruptedName.replace(".txt", ".cor.txt");
        if (SD.rename(profile_filename, corruptedName.c_str()))
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
    messageBox(message.c_str(), &lv_font_montserrat_14, lv_color_hex(0xFF0000), true);
}

bool loadSelectedProfile()
{
    String profile_filename = profileFilename(config.profile);
    if (!readProfile(profile_filename.c_str(), config))
    {
        corruptProfile(profile_filename);
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
    updateProfileDeleteButtonVisibility();
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
    if (isTricklerRunning())
    {
        messageBox(langText("msg_stop_trickler_before_delete_profile"), &lv_font_montserrat_14, lv_color_hex(0xFF0000), false);
        return false;
    }

    String profileName = config.profile;
    if (profileName == "calibrate")
    {
        messageBox(langText("msg_cannot_delete_calibrate_profile"), &lv_font_montserrat_14, lv_color_hex(0xFF0000), false);
        return false;
    }

    String filename = profileFilename(profileName.c_str());
    if (!SD.exists(filename.c_str()))
    {
        messageBox(String(langText("msg_delete_profile_file_not_found")) + filename, &lv_font_montserrat_14, lv_color_hex(0xFF0000), false);
        getProfileList();
        return false;
    }

    profileDeleteName = profileName;
    profileDeleteFilename = filename;
    profileDeleteConfirmPending = true;
    showConfirmBox(String(langText("msg_delete_profile_confirm_prefix")) + profileName + langText("msg_delete_profile_confirm_suffix"), &lv_font_montserrat_14, lv_color_hex(0xFFFFFF));
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

    if (isTricklerRunning())
    {
        messageBox(langText("msg_stop_trickler_before_delete_profile"), &lv_font_montserrat_14, lv_color_hex(0xFF0000), false);
        return;
    }

    if ((profileName.length() <= 0) || (profileName == "calibrate"))
    {
        messageBox(langText("msg_cannot_delete_profile"), &lv_font_montserrat_14, lv_color_hex(0xFF0000), false);
        return;
    }

    if (!SD.exists(filename.c_str()))
    {
        messageBox(String(langText("msg_delete_profile_file_not_found")) + filename, &lv_font_montserrat_14, lv_color_hex(0xFF0000), false);
        getProfileList();
        return;
    }

    int calibrateIndex = findProfileIndex("calibrate");
    if (calibrateIndex < 0)
    {
        messageBox(langText("msg_delete_profile_calibrate_missing"), &lv_font_montserrat_14, lv_color_hex(0xFF0000), false);
        return;
    }

    setProfile(calibrateIndex);
    if (strcmp(config.profile, "calibrate") != 0)
    {
        messageBox(langText("msg_delete_profile_calibrate_load_failed"), &lv_font_montserrat_14, lv_color_hex(0xFF0000), false);
        return;
    }

    if (!SD.remove(filename.c_str()))
    {
        messageBox(String(langText("msg_could_not_delete_profile")) + filename, &lv_font_montserrat_14, lv_color_hex(0xFF0000), false);
        getProfileList();
        return;
    }

    getProfileList();
    updateProfileDeleteButtonVisibility();
    messageBox(String(langText("msg_profile_deleted")) + profileName, &lv_font_montserrat_14, lv_color_hex(0x00FF00), false);
}

