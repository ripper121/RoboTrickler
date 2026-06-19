void initSetup()
{
    Serial.begin(115200); /* prepare for possible serial debug */
    disableRuntimeWatchdogs();

    displayInit();

    DEBUG_PRINTLN("Display Init");

    if (!initFilesystem())
    {
        ui_init();
        disableTouchGestures();
        disp_task_init();
        restart_now = true;
        errorBox(langText("msg_filesystem_mount_failed"), true);
        return;
    }

    ui_init();
    disableTouchGestures();
    disp_task_init();

    updateDisplayLog((String("Robo-Trickler v") + FW_VERSION + " // strenuous.dev").c_str());

    // Run the SD firmware/LittleFS update before loading config/profiles so it
    // also works from a bare SD card that only contains firmware.bin /
    // littlefs.bin. The display task is already up, so progress is shown on
    // screen (config/language are not loaded yet, so messages use the built-in
    // English fallback). initUpdate() self-guards on activeFSIsSD.
    initUpdate();

    String infoText = langText("status_init_steppers");
    updateDisplayLog(infoText, true);
    initStepper();

    infoText = langText("status_loading_config");
    updateDisplayLog(infoText, true);
    if (!loadConfiguration("/config.txt", config))
    {
        String readError = getSdReadError();
        if (readError.length() <= 0)
        {
            readError = langText("msg_unknown_config_read_error");
        }
        updateDisplayLog(readError);

        // loadConfiguration() applies full defaults to config before it touches
        // the file, so config is already consistent here regardless of whether
        // config.txt was missing or corrupt. Persist the defaults and keep
        // booting instead of rebooting; the device ends up in the same state a
        // fresh boot would produce. Non-blocking so boot continues behind the
        // notice.
        setDefaultConfiguration(config);
        saveConfiguration("/config.txt", config);

        String message = String(langText("msg_config_corrupted")) + readError + "\n\n" + langText("msg_config_default");
        errorBox(message, false);
    }

    applyLanguage();
    updateScaleProtocolButtonLabel();
    updateFilesystemSyncControls();
    updateDisplayLog(langText(activeFSIsSD ? "status_storage_sd" : "status_storage_flash"));

    infoText = langText("status_reading_profiles");
    updateDisplayLog(infoText, true);
    getProfileList();

    // If config.txt names a missing profile, fall back through loadSelectedProfile()
    // so the normal corruption handling and calibration profile recovery run.
    bool selectedProfileFound = false;
    for (int i = 0; i < profileListCount; i++)
    {
        if (strcmp(config.profile, profileListBuff[i]) == 0)
        {
            selectedProfileFound = true;
            profileListCounter = i;
            break;
        }
    }
    // Load the configured profile once. loadSelectedProfile() recovers onto the
    // calibration profile if it is missing/corrupt.
    if (!loadSelectedProfile(true))
    {
        return;
    }

    // If config named a profile that was not in the list it was recovered onto
    // calibrate; persist that and re-sync the highlighted list entry.
    if (!selectedProfileFound)
    {
        saveConfiguration("/config.txt", config);
        for (int i = 0; i < profileListCount; i++)
        {
            if (strcmp(config.profile, profileListBuff[i]) == 0)
            {
                profileListCounter = i;
                break;
            }
        }
    }

    applyLanguage();

    infoText = langText("status_starting_scale");
    updateDisplayLog(infoText, true);
    initRs232();

    initWebServer();

    if (WiFi.status() == WL_CONNECTED)
    {
        updateDisplayLog("WIFI:" + WiFi.localIP().toString());
    }

    if (WIFI_SETUP_AP_ACTIVE)
    {
        showWifiSetupInfo();
    }
    else
    {
        setLabelText(ui_LabelInfo, (String("Robo-Trickler v") + FW_VERSION + " // strenuous.dev").c_str());
    }
    updateTargetWeightLabel();
    setLabelText(ui_LabelProfile, config.profile);
    updateProfileActionButtonVisibility();

    tempTargetWeight = config.targetWeight;

    infoText = langText("status_ready");
    updateDisplayLog(infoText, true);

    DEBUG_PRINTLN("Setup done.");
}

