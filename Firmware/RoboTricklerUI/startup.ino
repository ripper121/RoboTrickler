void initSetup()
{
    Serial.begin(115200); /* prepare for possible serial debug */
    //disableRuntimeWatchdogs();

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

        setDefaultConfiguration(config);
        saveConfiguration("/config.txt", config);
        delay(100);

        String message = String(langText("msg_config_corrupted")) + readError + "\n\n" + langText("msg_config_default");
        restart_now = true;
        errorBox(message, true);
        return;
    }

    applyLanguage();
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
    if (!selectedProfileFound)
    {
        if (!loadSelectedProfile())
        {
            return;
        }
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

    if ((tempProfile != String(config.profile)) && !loadSelectedProfile())
    {
        return;
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

    if (!activeFSIsSD)
    {
        warnBox(langText("msg_sd_card_not_connected"), false);
    }

    DEBUG_PRINTLN("Setup done.");
}

