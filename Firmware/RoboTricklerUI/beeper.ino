void playConfiguredBeep(const char *beepMode)
{
    bool requestDone = strstr(beepMode, "done") != NULL;
    bool requestButton = strstr(beepMode, "button") != NULL;
    bool enableDone = strstr(config.beeper, "done") != NULL;
    bool enableButton = strstr(config.beeper, "button") != NULL;
    bool enableBoth = strstr(config.beeper, "both") != NULL;

    if (requestDone && (enableDone || enableBoth))
        stepperBeep(BEEP_DONE_DURATION_MS);
    if (requestButton && (enableButton || enableBoth))
        stepperBeep(BEEP_BUTTON_DURATION_MS);
}
