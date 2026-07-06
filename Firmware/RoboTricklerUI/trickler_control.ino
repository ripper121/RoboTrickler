float pendingTargetWeight = 0.0;

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

    // Capture any on-screen target-weight edit (the +/- buttons only change
    // config.targetWeight in RAM) before the reload below overwrites it.
    bool targetEdited = (pendingTargetWeight != config.targetWeight);
    float displayTarget = config.targetWeight;

    // Always reload the selected profile from the filesystem on Start so the
    // run uses the on-disk values even if the file changed since it was picked.
    if (!loadSelectedProfile(false))
    {
        return;
    }
    // The calibration throw is only useful if a powder profile can be created
    // from it afterwards. Bail out before wasting a throw when the profile list
    // is already full (createProfileFromCalibration would refuse later anyway).
    if (strcmp(config.profileName, "calibrate") == 0)
    {
        refreshProfileList();
        if (profileListCount >= PROFILE_LIST_MAX)
        {
            errorBox(langText("msg_max_profiles_reached"), true);
            return;
        }
    }

    String selectedText = String(langText("placeholder_profile")) + ": " + config.profileName + langText("status_profile_selected_suffix");
    updateDisplayLog(selectedText);

    // Honor an on-screen target edit made since the profile was picked: re-apply
    // it over the just-reloaded profile value and persist it. saveTargetWeight()
    // also resyncs pendingTargetWeight, so no separate assignment is needed here.
    if (targetEdited && displayTarget != config.targetWeight)
    {
        String infoText = langText("status_saving_target");
        updateDisplayLog(infoText, true);
        saveTargetWeight(displayTarget);
    }

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
    if (config.totalCounterEnable)
    {
        saveConfiguration("/config.txt", config);
    }
    sessionCount = 0;
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
    newWeightData = false;
    weightCounter = 0;
    activeProfileStep = -1;
    measurementCount = config.profileGeneralMeasurements;
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
    config.targetWeight = clampWeight(weight);
    updateTargetWeightLabel();
    String infoText = langText("status_saving_target");
    updateDisplayLog(infoText, true);
    if (!saveProfileTargetWeight(config.profileName, config.targetWeight))
    {
        String readError = getSdReadError();
        updateDisplayLog(readError.length() > 0 ? readError : langText("status_saving_target_failed"), true);
    }
    else
    {
        infoText = langText("status_target_saved");
        updateDisplayLog(infoText, true);
    }
    pendingTargetWeight = config.targetWeight;
}

