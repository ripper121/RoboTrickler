float tempTargetWeight = 0.0;

void beep(const char *beepMode)
{
    bool requestDone = strstr(beepMode, "done") != NULL;
    bool requestButton = strstr(beepMode, "button") != NULL;
    bool enableDone = strstr(config.beeper, "done") != NULL;
    bool enableButton = strstr(config.beeper, "button") != NULL;
    bool enableBoth = strstr(config.beeper, "both") != NULL;

    if (requestDone && (enableDone || enableBoth))
        stepperBeep(500);
    if (requestButton && (enableButton || enableBoth))
        stepperBeep(100);
}

void startTrickler()
{
    if (isTricklerRunning())
    {
        return;
    }

    // Always reload the selected profile from the filesystem on Start so the
    // run uses the on-disk values even if the file changed since it was picked.
    if (!loadSelectedProfile(false))
    {
        return;
    }
    // The calibration throw is only useful if a powder profile can be created
    // from it afterwards. Bail out before wasting a throw when the profile list
    // is already full (createProfileFromCalibration would refuse later anyway).
    if (strcmp(config.profile, "calibrate") == 0)
    {
        getProfileList();
        if (profileListCount >= PROFILE_LIST_MAX)
        {
            errorBox(langText("msg_max_profiles_reached"), true);
            return;
        }
    }

    String selectedText = String(langText("placeholder_profile")) + ": " + config.profile + langText("status_profile_selected_suffix");
    updateDisplayLog(selectedText);

    if (tempTargetWeight != config.targetWeight)
    {
        String infoText = langText("status_saving_target");
        updateDisplayLog(infoText, true);
        saveTargetWeight(config.targetWeight);
    }
    tempTargetWeight = config.targetWeight;

    String infoText = langText("status_starting_trickler");
    updateDisplayLog(infoText, true);
    cancelInteractiveDialogs();
    setLabelText(ui_LabelToggleTrickler, UI_SYMBOL_STOP);
    setObjBgColor(ui_ButtonToggleTrickler, 0xFF0000);
    setProfileTabEnabled(false);
    startMeasurement();
}

void stopTrickler()
{
    stopMeasurement();
    setProfileTabEnabled(true);
    if (config.trickleCounter)
    {
        saveConfiguration("/config.txt", config);
    }
    trickleCounter = 0;
    setLabelText(ui_LabelToggleTrickler, UI_SYMBOL_START);
    setObjBgColor(ui_ButtonToggleTrickler, 0x00FF00);
    setLabelTextColor(ui_LabelTricklerWeight, 0xFFFFFF);
    setLabelText(ui_LabelTricklerWeight, "-.-");
    String infoText = langText("status_stopped");
    updateDisplayLog(infoText, true);
}

void startMeasurement()
{
    // Start with a stable-weight window before the first throw.
    newData = false;
    weightCounter = 0;
    activeProfileStep = -1;
    measurementCount = config.profile_generalMeasurements;
    lastWeight = weight;
    firstProfileMovePending = true;
    setTricklerState(TRICKLER_RUNNING);
    beep("button");
}

void stopMeasurement()
{
    activeProfileStep = -1;
    setTricklerState(TRICKLER_IDLE);
    beep("button");
}

void saveTargetWeight(float weight)
{
    config.targetWeight = weight;
    updateTargetWeightLabel();
    String infoText = langText("status_saving_target");
    updateDisplayLog(infoText, true);
    if (!saveProfileTargetWeight(config.profile, config.targetWeight))
    {
        String readError = getSdReadError();
        updateDisplayLog(readError.length() > 0 ? readError : langText("status_saving_target_failed"), true);
    }
    else
    {
        infoText = langText("status_target_saved");
        updateDisplayLog(infoText, true);
    }
    tempTargetWeight = config.targetWeight;
}

