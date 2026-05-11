void handleCorruptProfile(String profile_filename)
{
    String readError = getSdReadError();
    String message = String("Profile Corrupted / Not Found:\n\n") + profile_filename;
    if (readError.length() > 0)
    {
        updateDisplayLog(readError);
        message += "\n\n";
        message += readError;
    }
    message += "\n\nCalibration Profile Loaded.";

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
        handleCorruptProfile(profile_filename);
        return false;
    }
    tempProfile = config.profile;
    tempTargetWeight = config.targetWeight;
    return true;
}

void selectProfile(int num)
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

    String infoText = String(languageText("status_selecting_profile")) + config.profile;
    updateDisplayLog(infoText, true);

    saveConfiguration("/config.txt", config);

    if (!loadSelectedProfile())
    {
        return;
    }

    DEBUG_PRINT("num: ");
    DEBUG_PRINTLN(num);

    setLabelText(ui_LabelProfile, config.profile);
    setLabelText(ui_LabelTarget, String(config.targetWeight, 3).c_str());
    infoText = String(languageText("status_profile_ready")) + config.profile;
    updateDisplayLog(infoText, true);
}

void saveTargetWeight(float weight)
{
    config.targetWeight = weight;
    setLabelText(ui_LabelTarget, String(config.targetWeight, 3).c_str());
    String infoText = languageText("status_saving_target");
    updateDisplayLog(infoText, true);
    if (!saveProfileTargetWeight(config.profile, config.targetWeight))
    {
        String readError = getSdReadError();
        updateDisplayLog(readError.length() > 0 ? readError : languageText("status_saving_target_failed"), true);
    }
    else
    {
        infoText = languageText("status_target_saved");
        updateDisplayLog(infoText, true);
    }
    tempTargetWeight = config.targetWeight;
}
