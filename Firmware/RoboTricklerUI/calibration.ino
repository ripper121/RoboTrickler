void startCalibrationProfilePrompt()
{
    stopTrickler();
    calibrationProfilePromptPending = true;
    calibrationProfilePromptTime = millis();
    measurementCount = config.profile_generalMeasurements;
    weightCounter = 0;
    newData = false;
}

void handleCalibrationProfilePrompt()
{
    if (!calibrationProfilePromptPending)
    {
        return;
    }

    readScaleWeight();
    updateWeightLabel(ui_LabelTricklerWeight);

    if (newData && (lastScaleWeightReadTime > calibrationProfilePromptTime))
    {
        calibrationProfilePromptPending = false;
        newData = false;
        weightCounter = 0;
        if (confirmBox(String(languageText("msg_create_profile_prompt")) + String(weight, 3) + " gn", &lv_font_montserrat_14, lv_color_hex(UI_COLOR_TEXT)))
        {
            String profileName = "";
            if (createProfileFromCalibration(weight, profileName))
            {
                messageBox(String(languageText("msg_profile_created")) + profileName + ".txt", &lv_font_montserrat_14, lv_color_hex(UI_COLOR_OK), true);
            }
            else
            {
                messageBox(languageText("msg_create_profile_failed"), &lv_font_montserrat_14, lv_color_hex(UI_COLOR_ERROR), true);
            }
        }
    }
}
