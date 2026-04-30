String tempProfile = "";
float tempTargetWeight = 0.0;
extern int profileListCounter;

IRAM_ATTR void disp_task_init(void)
{
    if (lvglMutex == NULL)
    {
        lvglMutex = xSemaphoreCreateRecursiveMutex();
    }

    xTaskCreatePinnedToCore(lvgl_disp_task,  // task
                            "lvglTask",      // name for task
                            DISP_TASK_STACK, // size of task stack
                            NULL,            // parameters
                            DISP_TASK_PRO,   // priority
                            // nullptr,
                            &lv_disp_tcb,
                            DISP_TASK_CORE // must run the task on same core
                                           // core
    );
}

IRAM_ATTR void lvgl_disp_task(void *parg)
{
    while (1)
    {
        if (lvglLock())
        {
            lv_timer_handler();
            lvglUnlock();
        }
        if (WiFi.status() == WL_CONNECTED)
        {
            server.handleClient();
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

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

bool serialWait()
{
    bool timeout = true;
    for (int i = 0; i < 2000; i++)
    {
        if (Serial1.available())
        {
            timeout = false;
            break;
        }
        delay(1);
    }
    return timeout;
}

void insertLine(String newLine)
{
    // Shift all lines up by one position
    for (int i = 0; i < (sizeof(infoMessagBuff) / sizeof(infoMessagBuff[0])) - 1; i++)
    {
        infoMessagBuff[i] = infoMessagBuff[i + 1];
    }
    // Add new line at the bottom
    infoMessagBuff[(sizeof(infoMessagBuff) / sizeof(infoMessagBuff[0])) - 1] = newLine;
}

void setLabelTextColor(lv_obj_t *label, uint32_t colorHex)
{
    if (lvglLock())
    {
        lv_obj_set_style_text_color(label, lv_color_hex(colorHex), LV_PART_MAIN | LV_STATE_DEFAULT);
        lvglUnlock();
    }
}

void disableTouchGestures()
{
    if (lvglLock())
    {
        lv_obj_t *tabViewContent = lv_tabview_get_content(ui_TabView);
        lv_obj_clear_flag(tabViewContent, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(tabViewContent, LV_DIR_NONE);
        lv_obj_set_scrollbar_mode(tabViewContent, LV_SCROLLBAR_MODE_OFF);
        lvglUnlock();
    }
}


void corruptProfile(String profile_filename)
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

void startTrickler()
{
    setLabelText(ui_LabelTricklerStart, "Stop");
    setObjBgColor(ui_ButtonTricklerStart, 0xFF0000);

    if (tempProfile != String(config.profile))
    {
        String profile_filename = profileFilename(config.profile);
        String infoText = String("Loading profile: ") + config.profile;
        updateDisplayLog(infoText, true);
        if (!readProfile(profile_filename.c_str(), config))
        {
            corruptProfile(profile_filename);
            return;
        }
        tempProfile = config.profile;
    }
    char message[64];
    snprintf(message, sizeof(message), "Profile: %s selected!", config.profile);
    updateDisplayLog(message);

    if (tempTargetWeight != config.targetWeight)
    {
        String infoText = "Saving target weight...";
        updateDisplayLog(infoText, true);
        saveTargetWeight(config.targetWeight);
    }
    tempTargetWeight = config.targetWeight;

    String infoText = "Starting trickler...";
    updateDisplayLog(infoText, true);
    startMeasurment();
}

void stopTrickler()
{
    stopMeasurment();
    setLabelText(ui_LabelTricklerStart, "Start");
    setObjBgColor(ui_ButtonTricklerStart, 0x00FF00);
    setLabelText(ui_LabelTricklerWeight, "-.-");
    String infoText = "Stopped";
    updateDisplayLog(infoText, true);
}

void startMeasurment()
{
    newData = false;
    weightCounter = 0;
    measurementCount = config.profile_generalMeasurements;
    lastWeight = weight;
    running = true;
    finished = false;
    firstThrow = true;
    beep("button");
}

void stopMeasurment()
{
    running = false;
    finished = true;
    beep("button");
}

void setProfile(int num)
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

    String infoText = String("Selecting profile: ") + config.profile;
    updateDisplayLog(infoText, true);

    saveConfiguration("/config.txt", config);

    String profile_filename = profileFilename(config.profile);
    infoText = String("Loading profile: ") + config.profile;
    updateDisplayLog(infoText, true);
    if (!readProfile(profile_filename.c_str(), config))
    {
        corruptProfile(profile_filename);
        return;
    }
    tempProfile = config.profile;
    tempTargetWeight = config.targetWeight;

    DEBUG_PRINT("num: ");
    DEBUG_PRINTLN(num);

    setLabelText(ui_LabelProfile, config.profile);
    setLabelText(ui_LabelTarget, String(config.targetWeight, 3).c_str());
    infoText = String("Profile ready: ") + config.profile;
    updateDisplayLog(infoText, true);
}

void serialFlush()
{
    // flush TX
    Serial1.flush();
    // flush RX
    while (Serial1.available())
    {
        Serial1.read();
    }
}

void initSetup()
{
    Serial.begin(115200); /* prepare for possible serial debug */

    rtc_wdt_protect_off();
    rtc_wdt_disable();

    displayInit();
    ui_init();
    disableTouchGestures();
    disp_task_init();

    DEBUG_PRINTLN("Display Init");

    updateDisplayLog(String("Robo-Trickler v" + String(FW_VERSION, 2) + " // strenuous.dev").c_str());

    String infoText = "Init steppers...";
    updateDisplayLog(infoText, true);
    initStepper();

    infoText = "Mounting SD card...";
    updateDisplayLog(infoText, true);
    SDspi = new SPIClass(HSPI);
    SDspi->begin(GRBL_SPI_SCK, GRBL_SPI_MISO, GRBL_SPI_MOSI, GRBL_SPI_SS);
    if (!SD.begin(GRBL_SPI_SS, *SDspi, SD_SPI_FREQ, "/sd", 10))
    {
        restart_now = true;
        messageBox(String("Card Mount Failed!\n").c_str(), &lv_font_montserrat_14, lv_color_hex(0xFF0000), true);
        return;
    }
    else
    {
        uint8_t cardType = SD.cardType();

        if (cardType == CARD_NONE)
        {
            restart_now = true;
            messageBox(String("No SD card attached!\n").c_str(), &lv_font_montserrat_14, lv_color_hex(0xFF0000), true);
            return;
        }
        else
        {
            DEBUG_PRINT("SD Card Type: ");
            if (cardType == CARD_MMC)
            {
                DEBUG_PRINTLN("MMC");
            }
            else if (cardType == CARD_SD)
            {
                DEBUG_PRINTLN("SDSC");
            }
            else if (cardType == CARD_SDHC)
            {
                DEBUG_PRINTLN("SDHC");
            }
            else
            {
                DEBUG_PRINTLN("UNKNOWN");
                updateDisplayLog("CardType UNKNOWN");
            }

            uint64_t cardSize = SD.cardSize() / (1024 * 1024);
            DEBUG_PRINT("SD Card Size: ");
            DEBUG_PRINT(cardSize);
            DEBUG_PRINTLN("MB");
            (void)cardSize;
        }
    }

    infoText = "Loading config...";
    updateDisplayLog(infoText, true);
    if (!loadConfiguration("/config.txt", config))
    {
        String readError = getSdReadError();
        if (readError.length() <= 0)
        {
            readError = "Unknown config read error";
        }
        updateDisplayLog(readError);

        strlcpy(config.wifi_ssid,                 // <- destination
                "",                               // <- source
                sizeof(config.wifi_ssid));        // <- destination's capacity
        strlcpy(config.wifi_psk,                  // <- destination
                "",                               // <- source
                sizeof(config.wifi_psk));         // <- destination's capacity
        strlcpy(config.IPStatic,                  // <- destination
                "",                               // <- source
                sizeof(config.IPStatic));         // <- destination's capacity
        strlcpy(config.IPGateway,                 // <- destination
                "",                               // <- source
                sizeof(config.IPGateway));        // <- destination's capacity
        strlcpy(config.IPSubnet,                  // <- destination
                "",                               // <- source
                sizeof(config.IPSubnet));         // <- destination's capacity
        strlcpy(config.IPDns,                     // <- destination
                "",                               // <- source
                sizeof(config.IPDns));            // <- destination's capacity
        strlcpy(config.scale_protocol,            // <- destination
                "GG",                             // <- source
                sizeof(config.scale_protocol));   // <- destination's capacity
        strlcpy(config.scale_customCode,          // <- destination
                "",                               // <- source
                sizeof(config.scale_customCode)); // <- destination's capacity
        config.scale_baud = 9600;
        strlcpy(config.profile,          // <- destination
                "calibrate",             // <- source
                sizeof(config.profile)); // <- destination's capacity
        config.targetWeight = 40.0;
        strlcpy(config.beeper,          // <- destination
                "done",                 // <- source
                sizeof(config.beeper)); // <- destination's capacity
        config.fwCheck = true;

        saveConfiguration("/config.txt", config);
        delay(100);

        String message = String("Config File Corrupted / Not Found!\n\n") + readError + "\n\nDefault Config generated.";
        restart_now = true;
        messageBox(message.c_str(), &lv_font_montserrat_14, lv_color_hex(0xFF0000), true);
        return;
    }

    infoText = "Reading profiles...";
    updateDisplayLog(infoText, true);
    getProfileList();

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
        String profile_filename = profileFilename(config.profile);
        if (!readProfile(profile_filename.c_str(), config))
        {
            corruptProfile(profile_filename);
            return;
        }
        tempProfile = config.profile;
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

    String profile_filename = profileFilename(config.profile);
    infoText = String("Loading profile: ") + config.profile;
    updateDisplayLog(infoText, true);
    if (!readProfile(profile_filename.c_str(), config))
    {
        corruptProfile(profile_filename);
        return;
    }
    tempProfile = config.profile;

    infoText = "Starting scale serial...";
    updateDisplayLog(infoText, true);
    Serial1.begin(config.scale_baud, SERIAL_8N1, SCALE_RX_PIN, SCALE_TX_PIN);

    initUpdate();

    initWebServer();

    if (WIFI_AKTIVE)
    {
        updateDisplayLog("WIFI:" + WiFi.localIP().toString());
    }

    setLabelText(ui_LabelInfo, String("Robo-Trickler v" + String(FW_VERSION, 2) + " // strenuous.dev").c_str());
    setLabelText(ui_LabelTarget, String(config.targetWeight, 3).c_str());
    setLabelText(ui_LabelProfile, config.profile);

    tempTargetWeight = config.targetWeight;

    infoText = "Ready";
    updateDisplayLog(infoText, true);
    DEBUG_PRINTLN("Setup done.");
}

void saveTargetWeight(float weight)
{
    config.targetWeight = weight;
    setLabelText(ui_LabelTarget, String(config.targetWeight, 3).c_str());
    String infoText = "Writing target to profile...";
    updateDisplayLog(infoText, true);
    if (!saveProfileTargetWeight(config.profile, config.targetWeight))
    {
        String readError = getSdReadError();
        updateDisplayLog(readError.length() > 0 ? readError : "Target weight save failed!", true);
    }
    else
    {
        infoText = "Target saved";
        updateDisplayLog(infoText, true);
    }
    tempTargetWeight = config.targetWeight;
}
