static void deleteRootLegacyProfileFiles()
{
    const char *legacyFiles[] = {"/calibrate.txt", "/avg.txt"};

    for (size_t i = 0; i < (sizeof(legacyFiles) / sizeof(legacyFiles[0])); i++)
    {
        if (ACTIVE_FS.exists(legacyFiles[i]))
        {
            DEBUG_PRINT("Deleting legacy root profile file: ");
            DEBUG_PRINTLN(legacyFiles[i]);
            if (!ACTIVE_FS.remove(legacyFiles[i]))
            {
                DEBUG_PRINT("Failed to delete legacy root profile file: ");
                DEBUG_PRINTLN(legacyFiles[i]);
            }
        }
    }
}

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
        messageBox("Filesystem mount failed", UI_FONT_NORMAL, lv_color_hex(0xFF0000), true);
        return;
    }

    deleteRootLegacyProfileFiles();

    showSplashLogo();

    ui_init();
    disableTouchGestures();
    initWifiQrCode();
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
        messageBox(message.c_str(), UI_FONT_NORMAL, lv_color_hex(0xFF0000), true);
        return;
    }

    infoText = langText("status_reading_profiles");
    updateDisplayLog(infoText, true);
    getProfileList();

    // If config.txt names a missing profile, fall back through loadSelectedProfile()
    // so the normal corruption handling and calibration profile recovery run.
    bool selectedProfileFound = false;
    for (int i = 0; i < profileListCount; i++)
    {
        if (String(config.profile) == profileListBuff[i])
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
            if (String(config.profile) == profileListBuff[i])
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
    setLabelText(ui_LabelTarget, String(config.targetWeight, 3).c_str());
    setLabelText(ui_LabelProfile, config.profile);
    updateProfileActionButtonVisibility();

    tempTargetWeight = config.targetWeight;

    infoText = langText("status_ready");
    updateDisplayLog(infoText, true);
    DEBUG_PRINTLN("Setup done.");
}

