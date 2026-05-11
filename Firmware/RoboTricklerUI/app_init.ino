static String appVersionText()
{
    return String("Robo-Trickler v") + String(FW_VERSION, 2) + " // strenuous.dev";
}

static void initDisplayStack()
{
    initDisplay();
    ui_init();
    disableTouchGestures();
    initDisplayTask();
    DEBUG_PRINTLN("Display Init");
}

static bool loadOrCreateConfiguration()
{
    updateDisplayLog(languageText("status_loading_config"), true);
    if (loadConfiguration("/config.txt", config))
    {
        return true;
    }

    String readError = getSdReadError();
    if (readError.length() <= 0)
    {
        readError = "Unknown config read error";
    }
    updateDisplayLog(readError);

    setDefaultConfiguration(config);
    saveConfiguration("/config.txt", config);
    delay(CONFIG_SAVE_SETTLE_DELAY_MS);

    String message = String("Config File Corrupted / Not Found!\n\n") + readError + "\n\n" + languageText("msg_config_default");
    restart_now = true;
    messageBox(message.c_str(), &lv_font_montserrat_14, lv_color_hex(UI_COLOR_ERROR), true);
    return false;
}

static bool loadStartupProfile()
{
    updateDisplayLog(languageText("status_reading_profiles"), true);
    refreshProfileList();

    profileListCounter = profileListIndexOf(config.profile);
    if (profileListCounter < 0)
    {
        if (!loadSelectedProfile())
        {
            return false;
        }
        saveConfiguration("/config.txt", config);
        profileListCounter = profileListIndexOf(config.profile);
    }

    if ((tempProfile != String(config.profile)) && !loadSelectedProfile())
    {
        return false;
    }
    return true;
}

static void applyStartupLabels(const String &versionText)
{
    if (wifiActive)
    {
        updateDisplayLog("WIFI:" + WiFi.localIP().toString());
    }

    setLabelText(ui_LabelInfo, versionText.c_str());
    setLabelText(ui_LabelTarget, String(config.targetWeight, 3).c_str());
    setLabelText(ui_LabelProfile, config.profile);
    tempTargetWeight = config.targetWeight;
}

void initApp()
{
    Serial.begin(115200); /* prepare for possible serial debug */

    initDisplayStack();

    String versionText = appVersionText();
    updateDisplayLog(versionText.c_str());

    updateDisplayLog(languageText("status_init_steppers"), true);
    initStepper();

    updateDisplayLog(languageText("status_mount_sd"), true);
    if (!mountSdCard())
    {
        return;
    }

    if (!loadOrCreateConfiguration())
    {
        return;
    }

    applyLanguage();

    if (!loadStartupProfile())
    {
        return;
    }

    applyLanguage();

    updateDisplayLog(languageText("status_starting_scale"), true);
    Serial1.begin(config.scale_baud, SERIAL_8N1, SCALE_RX_PIN, SCALE_TX_PIN);

    initFirmwareUpdate();

    initWebServer();

    applyStartupLabels(versionText);

    updateDisplayLog(languageText("status_ready"), true);
    DEBUG_PRINTLN("Setup done.");
}
