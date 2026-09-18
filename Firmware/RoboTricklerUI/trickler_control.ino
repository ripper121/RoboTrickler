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
    if (isTricklerRunning() || isCalibrationProfilePromptPending() ||
        isProfileTuneTestActive() || isWebFileUploadActive())
    {
        return;
    }

    // Persist an edited target before reloading the complete profile. A clean
    // target skips all profile-file I/O here; failed saves remain dirty so a
    // later Start can retry without losing the displayed value.
    char requestedProfile[sizeof(config.profileName)];
    strlcpy(requestedProfile, config.profileName, sizeof(requestedProfile));
    if (targetWeightUnsaved && !saveTargetWeight(config.targetWeight))
    {
        updateTargetWeightLabel();
        updateDisplayLog(langText("status_saving_target_failed"), true);
        return;
    }

    // Always reload the selected profile from the filesystem so
    // every runtime value comes from the same on-disk profile.
    if (!loadSelectedProfile(false))
    {
        return;
    }
    // loadSelectedProfile() recovers onto the calibration profile when the
    // selected file is corrupt or missing. Never carry that recovery straight
    // into a run: the calibrate profile would immediately dispense a full
    // calibration throw the user never asked for. Abort behind the recovery
    // error box and require an explicit new Start on the recovered profile.
    if (strcmp(requestedProfile, config.profileName) != 0)
    {
        return;
    }
    // The calibration throw is only useful if a powder profile can be created
    // from it afterwards. Bail out before wasting a throw when the profile list
    // is already full (createProfileFromCalibration would refuse later anyway).
    if (isCalibrationProfile())
    {
        refreshProfileList();
        if (profileListCount >= PROFILE_LIST_MAX)
        {
            // Non-blocking: startTrickler() is also reached from the web handler
            // (/system/start), which runs inside server.handleClient() on the
            // display task. A waiting dialog there stalls all web serving until
            // the touchscreen is tapped.
            errorBox(langText("msg_max_profiles_reached"), false);
            return;
        }
    }

    // Deferred from setProfile(): browsing profiles no longer rewrites
    // config.txt on every prev/next tap; persist the final selection once a
    // run actually starts.
    if (profileSelectionUnsaved)
    {
        profileSelectionUnsaved = !saveConfiguration("/config.txt", config);
    }

    String selectedText = String(langText("placeholder_profile")) + ": " + config.profileName + langText("status_profile_selected_suffix");
    updateDisplayLog(selectedText);

    String infoText = langText("status_starting_trickler");
    updateDisplayLog(infoText, true);
    cancelInteractiveDialogs();
    setLabelText(ui_LabelToggleTrickler, UI_SYMBOL_STOP);
    setObjBgColor(ui_ButtonToggleTrickler, 0xFF0000);
    setProfileTabEnabled(false);
    startMeasurement();
}

static void finishStopTrickler()
{
    // Reserve the filesystem before publishing logs/beeps. For Core 1 safety
    // stops this prevents an editor mutation from winning the gap between the
    // state transition and the total-counter save.
    FilesystemLockGuard filesystemGuard;
    setStepperRunEnabled(false);
    activeProfileStep = -1;
    beep("button");
    setProfileTabEnabled(true);
    // Persist the total counter only when a charge actually finished since the
    // last save; a manual stop without a completed throw changes nothing.
    if (config.totalCounterEnable && (config.totalCount != persistedTotalCount))
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

bool stopRunningTricklerInState(TricklerState state)
{
    if (!transitionTricklerState(TRICKLER_RUNNING, state))
    {
        return false;
    }
    finishStopTrickler();
    return true;
}

void stopTrickler()
{
    setTricklerState(TRICKLER_IDLE);
    finishStopTrickler();
}

void startMeasurement()
{
    // Start with a stable-weight window before the first throw.
    newWeightData = false;
    weightCounter = 0;
    activeProfileStep = -1;
    measurementCount = config.profileGeneralMeasurements;
    firstProfileMovePending = true;
    setStepperRunEnabled(true);
    setTricklerState(TRICKLER_RUNNING);
    beep("button");
}

bool saveTargetWeight(float weight)
{
    config.targetWeight = clampWeight(weight);
    updateTargetWeightLabel();
    String infoText = langText("status_saving_target");
    updateDisplayLog(infoText, true);
    if (!saveProfileTargetWeight(config.profileName, config.targetWeight))
    {
        String readError = getSdReadError();
        updateDisplayLog(readError.length() > 0 ? readError : langText("status_saving_target_failed"), true);
        return false;
    }
    targetWeightUnsaved = false;
    infoText = langText("status_target_saved");
    updateDisplayLog(infoText, true);
    return true;
}

