String tempProfile = "";
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
    setLabelText(ui_LabelTricklerStart, langText("button_stop"));
    setObjBgColor(ui_ButtonTricklerStart, 0xFF0000);

    if (tempProfile != String(config.profile))
    {
        if (!loadSelectedProfile())
        {
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
    startMeasurement();
}

void stopTrickler()
{
    stopMeasurement();
    if (config.trickleCounter)
    {
        saveConfiguration("/config.txt", config);
    }
    trickleCounter = 0;
    setLabelText(ui_LabelTricklerStart, langText("button_start"));
    setObjBgColor(ui_ButtonTricklerStart, 0x00FF00);
    setLabelText(ui_LabelTricklerWeight, "-.-");
    String infoText = langText("status_stopped");
    updateDisplayLog(infoText, true);
}

void startMeasurement()
{
    // Start with a stable-weight window before the first throw.
    newData = false;
    weightCounter = 0;
    measurementCount = config.profile_generalMeasurements;
    lastWeight = weight;
    firstProfileMovePending = true;
    setTricklerState(TRICKLER_RUNNING);
    beep("button");
}

void stopMeasurement()
{
    setTricklerState(TRICKLER_IDLE);
    beep("button");
}

void saveTargetWeight(float weight)
{
    config.targetWeight = weight;
    setLabelText(ui_LabelTarget, String(config.targetWeight, 3).c_str());
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

