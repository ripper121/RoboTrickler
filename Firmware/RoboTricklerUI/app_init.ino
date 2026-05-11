void initApp()
{
    Serial.begin(115200); /* prepare for possible serial debug */

    initDisplay();
    ui_init();
    disableTouchGestures();
    initDisplayTask();

    DEBUG_PRINTLN("Display Init");

    updateDisplayLog(String("Robo-Trickler v" + String(FW_VERSION, 2) + " // strenuous.dev").c_str());

    String infoText = languageText("status_init_steppers");
    updateDisplayLog(infoText, true);
    initStepper();

    infoText = languageText("status_mount_sd");
    updateDisplayLog(infoText, true);
    if (!mountSdCard())
    {
        return;
    }

    infoText = languageText("status_loading_config");
    updateDisplayLog(infoText, true);
    if (!loadConfiguration("/config.txt", config))
    {
        String readError = getSdReadError();
        if (readError.length() <= 0)
        {
            readError = "Unknown config read error";
        }
        updateDisplayLog(readError);

        setDefaultConfiguration(config);
        saveConfiguration("/config.txt", config);
        delay(100);

        String message = String("Config File Corrupted / Not Found!\n\n") + readError + "\n\n" + languageText("msg_config_default");
        restart_now = true;
        messageBox(message.c_str(), &lv_font_montserrat_14, lv_color_hex(0xFF0000), true);
        return;
    }

    applyLanguage();

    infoText = languageText("status_reading_profiles");
    updateDisplayLog(infoText, true);
    refreshProfileList();

    profileListCounter = profileListIndexOf(config.profile);
    if (profileListCounter < 0)
    {
        if (!loadSelectedProfile())
        {
            return;
        }
        saveConfiguration("/config.txt", config);
        profileListCounter = profileListIndexOf(config.profile);
    }

    if ((tempProfile != String(config.profile)) && !loadSelectedProfile())
    {
        return;
    }

    applyLanguage();

    infoText = languageText("status_starting_scale");
    updateDisplayLog(infoText, true);
    Serial1.begin(config.scale_baud, SERIAL_8N1, SCALE_RX_PIN, SCALE_TX_PIN);

    initFirmwareUpdate();

    initWebServer();

    if (wifiActive)
    {
        updateDisplayLog("WIFI:" + WiFi.localIP().toString());
    }

    setLabelText(ui_LabelInfo, String("Robo-Trickler v" + String(FW_VERSION, 2) + " // strenuous.dev").c_str());
    setLabelText(ui_LabelTarget, String(config.targetWeight, 3).c_str());
    setLabelText(ui_LabelProfile, config.profile);

    tempTargetWeight = config.targetWeight;

    infoText = languageText("status_ready");
    updateDisplayLog(infoText, true);
    DEBUG_PRINTLN("Setup done.");
}
