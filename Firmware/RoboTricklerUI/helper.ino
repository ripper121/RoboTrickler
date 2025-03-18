String tempProfile = "";
float tempTargetWeight = 0.0;

IRAM_ATTR void disp_task_init(void)
{
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
        lv_timer_handler();
        if (WiFi.status() == WL_CONNECTED)
        {
            server.handleClient();
        }
        // esp_task_wdt_reset();
    }
}

void beep(String beepMode)
{

    if ((String(config.beeper).indexOf("done") != -1) && (String(beepMode).indexOf("done") != -1))
        stepper1.beep(500);
    if ((String(config.beeper).indexOf("button") != -1) && (String(beepMode).indexOf("button") != -1))
        stepper1.beep(100);

    if ((String(config.beeper).indexOf("both") != -1) && (String(beepMode).indexOf("done") != -1))
        stepper1.beep(500);
    if ((String(config.beeper).indexOf("both") != -1) && (String(beepMode).indexOf("button") != -1))
        stepper1.beep(100);
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
    lv_color_t color = lv_color_hex(colorHex); // Convert hex color to LVGL color type
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN | LV_STATE_DEFAULT);
}


void corruptProfile(String profile_filename)
{
    messageBox(String("Profile Corrupted / Not Found:\n\n" + profile_filename + "\n\nCalibration Profile Loaded.").c_str(), &lv_font_montserrat_14, lv_color_hex(0xFF0000), true);

    // Rename file to indicate corruption
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

    strlcpy(config.profile,          // <- destination
            "calibrate",             // <- source
            sizeof(config.profile)); // <- destination's capacity
    saveConfiguration("/config.txt", config);
    delay(100);
    restart_now = true;
}

void startTrickler()
{
    strlcpy(config.mode,          // <- destination
            "trickler",           // <- source
            sizeof(config.mode)); // <- destination's capacity
    lv_label_set_text(ui_LabelTricklerStart, "Stop");
    lv_obj_set_style_bg_color(ui_ButtonTricklerStart, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);

    if (tempProfile != String(config.profile))
    {
        String profile_filename = "/" + String(config.profile) + ".txt";
        if (!readProfile(profile_filename.c_str(), config))
        {
            corruptProfile(profile_filename);
            return;
        }
    }
    updateDisplayLog(String("Profile: " + String(config.profile) + " selected!"));

    if (tempTargetWeight != config.targetWeight)
    {
        saveTargetWeight(config.targetWeight);
    }
    tempTargetWeight = config.targetWeight;

    serialFlush();
    startMeasurment();
}

void stopTrickler()
{
    stopMeasurment();
    serialFlush();
    lv_label_set_text(ui_LabelTricklerStart, "Start");
    lv_obj_set_style_bg_color(ui_ButtonTricklerStart, lv_color_hex(0x00FF00), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(ui_LabelTricklerWeight, "-.-");
    lv_label_set_text(ui_LabelInfo, "");
    lv_label_set_text(ui_LabelLoggerInfo, "");
}

void startMeasurment()
{
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
    strlcpy(config.profile,               // <- destination
            profileListBuff[num].c_str(), // <- source
            sizeof(config.profile));      // <- destination's capacity

    DEBUG_PRINT("num: ");
    DEBUG_PRINTLN(num);

    lv_label_set_text(ui_LabelProfile, config.profile);
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

    // esp_task_wdt_init(WDT_TIMEOUT, true); // enable panic so ESP32 restarts
    // esp_task_wdt_add(NULL);               // add current thread to WDT watch
    // esp_task_wdt_add(lv_disp_tcb);

    rtc_wdt_protect_off();
    rtc_wdt_disable();

    displayInit();
    ui_init();
    disp_task_init();

    DEBUG_PRINTLN("Display Init");

    updateDisplayLog(String("Robo-Trickler v" + String(FW_VERSION, 2) + " // strenuous.dev").c_str());

    initStepper();

    SDspi = new SPIClass(HSPI);
    SDspi->begin(GRBL_SPI_SCK, GRBL_SPI_MISO, GRBL_SPI_MOSI, GRBL_SPI_SS);
    if (!SD.begin(GRBL_SPI_SS, *SDspi))
    {
        messageBox(String("Card Mount Failed!\n").c_str(), &lv_font_montserrat_14, lv_color_hex(0xFF0000), true);
        restart_now = true;
        return;
    }
    else
    {
        uint8_t cardType = SD.cardType();

        if (cardType == CARD_NONE)
        {
            messageBox(String("No SD card attached!\n").c_str(), &lv_font_montserrat_14, lv_color_hex(0xFF0000), true);
            restart_now = true;
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
        }
    }

    if (!loadConfiguration("/config.txt", config))
    {
        updateDisplayLog("config.txt deserializeJson() failed");

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
        config.log_measurements = 20;
        config.targetWeight = 40.0;
        config.microsteps = 1;
        strlcpy(config.beeper,          // <- destination
                "done",                 // <- source
                sizeof(config.beeper)); // <- destination's capacity
        config.debugLog = false;
        config.fwCheck = true;

        saveConfiguration("/config.txt", config);
        delay(100);

        messageBox(String("Config File Corrupted / Not Found!\nDefault Config generated.").c_str(), &lv_font_montserrat_14, lv_color_hex(0xFF0000), true);
        restart_now = true;
        return;
    }

    getProfileList();

    String profile_filename = "/" + String(config.profile) + ".txt";
    if (!readProfile(profile_filename.c_str(), config))
    {
        corruptProfile(profile_filename);
        return;
    }

    Serial1.begin(config.scale_baud, SERIAL_8N1, IIC_SCL, IIC_SDA);

    initUpdate();

    initWebServer();

    if (WIFI_AKTIVE)
    {
        updateDisplayLog("WIFI:" + WiFi.localIP().toString());
    }

    // initialize the variables we're linked to
    Input = 0;
    roboPID.SetOutputLimits(config.pidStepMin, config.pidStepMax);
    // turn the PID on
    roboPID.SetMode(roboPID.Control::automatic);

    lv_label_set_text(ui_LabelInfo, String("Robo-Trickler v" + String(FW_VERSION, 2) + " // strenuous.dev").c_str());
    lv_label_set_text(ui_LabelTarget, String(config.targetWeight, 3).c_str());
    lv_label_set_text(ui_LabelProfile, config.profile);

    tempTargetWeight = config.targetWeight;

    DEBUG_PRINTLN("Setup done.");
}

void saveTargetWeight(float weight)
{
    config.targetWeight = weight;
    lv_label_set_text(ui_LabelTarget, String(config.targetWeight, 3).c_str());
    saveConfiguration("/config.txt", config);
}