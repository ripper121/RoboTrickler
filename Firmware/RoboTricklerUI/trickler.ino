void startTrickler()
{
    if (tempProfile != String(config.profile))
    {
        if (!loadSelectedProfile())
        {
            return;
        }
    }
    String selectedText = String(languageText("placeholder_profile")) + ": " + config.profile + languageText("status_profile_selected_suffix");
    updateDisplayLog(selectedText);

    if (tempTargetWeight != config.targetWeight)
    {
        updateDisplayLog(languageText("status_saving_target"), true);
        saveTargetWeight(config.targetWeight);
    }
    tempTargetWeight = config.targetWeight;

    setLabelText(ui_LabelTricklerStart, languageText("button_stop"));
    setObjBgColor(ui_ButtonTricklerStart, UI_COLOR_ERROR);
    updateDisplayLog(languageText("status_starting_trickler"), true);
    startMeasurement();
}

void stopTrickler()
{
    stopMeasurement();
    setLabelText(ui_LabelTricklerStart, languageText("button_start"));
    setObjBgColor(ui_ButtonTricklerStart, UI_COLOR_OK);
    setLabelText(ui_LabelTricklerWeight, "-.-");
    updateDisplayLog(languageText("status_stopped"), true);
}

void startMeasurement()
{
    newData = false;
    weightCounter = 0;
    measurementCount = config.profile_generalMeasurements;
    lastWeight = weight;
    running = true;
    finished = false;
    firstThrow = true;
    playConfiguredBeep("button");
}

void stopMeasurement()
{
    running = false;
    finished = true;
    playConfiguredBeep("button");
}

static void playDoneBeeps(byte count)
{
    for (byte i = 0; i < count; i++)
    {
        playConfiguredBeep("done");
        if ((i + 1) < count)
        {
            delay(DONE_BEEP_GAP_MS);
        }
    }
}

static void handleTargetReached(float tolerance, float alarmThreshold)
{
    uint32_t weightColor = weight <= (config.targetWeight + tolerance + EPSILON) ? UI_COLOR_OK : UI_COLOR_WARN;
    setLabelTextColor(ui_LabelTricklerWeight, weightColor);

    if ((alarmThreshold > 0) && (weight >= (config.targetWeight + alarmThreshold + EPSILON)))
    {
        setLabelTextColor(ui_LabelTricklerWeight, UI_COLOR_ERROR);
        updateDisplayLog(languageText("status_over_trickle"), true);
        playDoneBeeps(3);
        stopTrickler();
        messageBox(languageText("msg_over_trickle"), &lv_font_montserrat_32, lv_color_hex(UI_COLOR_ERROR), true);
    }

    if (!finished)
    {
        playConfiguredBeep("done");
        updateDisplayLog(languageText("status_done"), true);
    }
    measurementCount = 0;
    finished = true;
}

static bool handleFirstThrow(String &infoText)
{
    if (!firstThrow)
    {
        return true;
    }

    firstThrow = false;
    if (!runBulkStepperMove(infoText))
    {
        updateDisplayLog(languageText("status_bulk_failed"), true);
        stopTrickler();
        return false;
    }

    if (infoText.length() <= 0)
    {
        return true;
    }

    if (strcmp(config.profile, "calibrate") == 0)
    {
        startCalibrationProfilePrompt();
        return false;
    }
    measurementCount = config.profile_generalMeasurements;
    updateDisplayLog(infoText, true);
    return false;
}

static bool selectProfileStep(int *profileStep)
{
    if (config.profile_count <= 0)
    {
        updateDisplayLog(languageText("status_invalid_profile"), true);
        stopTrickler();
        return false;
    }

    *profileStep = config.profile_count - 1;
    for (int i = 0; i < config.profile_count; i++)
    {
        if (weight <= (config.targetWeight - config.profile_step[i].weight))
        {
            *profileStep = i;
            break;
        }
    }
    return true;
}

static bool runProfileStep(int profileStep, String &infoText)
{
    static int stepperSpeedOld[PROFILE_ACTUATOR_SLOTS] = {0, 0, 0};
    ProfileStep &stepConfig = config.profile_step[profileStep];
    byte stepperNum = stepConfig.actuator;
    if ((stepperNum < PROFILE_ACTUATOR_MIN) || (stepperNum > PROFILE_ACTUATOR_MAX))
    {
        updateDisplayLog(languageText("status_invalid_stepper"), true);
        stopTrickler();
        return false;
    }

    if (stepperSpeedOld[stepperNum] != stepConfig.speed)
    {
        setStepperSpeed(stepperNum, stepConfig.speed);
        stepperSpeedOld[stepperNum] = stepConfig.speed;
    }

    step(stepperNum, stepConfig.steps, stepConfig.reverse);

    measurementCount = stepConfig.measurements;

    char infoLine[TRICKLER_INFO_LINE_SIZE];
    snprintf(infoLine,
             sizeof(infoLine),
             "W%.3f ST%ld SP%d M%d",
             stepConfig.weight,
             stepConfig.steps,
             stepConfig.speed,
             stepConfig.measurements);
    infoText = infoLine;

    if (strcmp(config.profile, "calibrate") == 0)
    {
        startCalibrationProfilePrompt();
        return false;
    }

    updateDisplayLog(infoText, true);
    return true;
}

static void processTricklerMeasurement()
{
    newData = false;
    weightCounter = 0;

    if (weight <= 0.0000)
    {
        firstThrow = true;
        measurementCount = config.profile_generalMeasurements;
    }

    float tolerance = config.profile_tolerance;
    float alarmThreshold = config.profile_alarmThreshold;

    DEBUG_PRINT("Weight: ");
    DEBUG_PRINTLN(weight);
    DEBUG_PRINT("TargetWeight: ");
    DEBUG_PRINTLN(String((config.targetWeight - tolerance), 5));
    DEBUG_PRINTLN(String((config.targetWeight + tolerance), 5));
    DEBUG_PRINT("profile_alarmThreshold: ");
    DEBUG_PRINTLN(String((config.targetWeight + alarmThreshold), 5));

    if ((weight >= (config.targetWeight - tolerance - EPSILON)) && (weight >= 0))
    {
        handleTargetReached(tolerance, alarmThreshold);
    }

    if (!finished && (weight >= 0))
    {
        updateDisplayLog(languageText("status_running"), true);
        setLabelTextColor(ui_LabelTricklerWeight, UI_COLOR_TEXT);

        DEBUG_PRINTLN("Profile Running");
        String infoText = "";
        infoText.reserve(TRICKLER_INFO_TEXT_RESERVE);
        if (!handleFirstThrow(infoText))
        {
            return;
        }

        int profileStep = 0;
        if (!selectProfileStep(&profileStep))
        {
            return;
        }

        if (!runProfileStep(profileStep, infoText))
        {
            return;
        }
    }

    if ((weight <= 0.0000) && finished)
    {
        updateDisplayLog(languageText("status_ready"), true);
        finished = false;
    }
}

static void handleIdleScaleRead()
{
    static uint32_t scaleReadTime = millis();

    handleCalibrationProfilePrompt();

    uint32_t now = millis();
    if (now - scaleReadTime < IDLE_SCALE_READ_INTERVAL_MS)
    {
        return;
    }

    scaleReadTime = now;
    updateDisplayLog(languageText("status_get_weight"), true);
    readScaleWeight();
    updateWeightLabel(ui_LabelTricklerWeight);
}

void handleTricklerLoop()
{
    if (!running)
    {
        handleIdleScaleRead();
        return;
    }

    readScaleWeight();
    if (newData)
    {
        processTricklerMeasurement();
    }
}
