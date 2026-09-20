// Actual LVGL screen and extracted firmware dialog factories, illustrative data.
// Included only by the native harness; hardware callbacks remain inert.
static void renderManualScreenshots()
{
    german = true;
    lv_tabview_set_tab_text(ui_TabView, 0, langText("tab_trickler"));
    lv_tabview_set_tab_text(ui_TabView, 1, langText("tab_profile"));
    lv_tabview_set_tab_text(ui_TabView, 2, "Info " UI_SYMBOL_WIFI " " UI_SYMBOL_SD_CARD);
    lv_label_set_text(ui_LabelTarget, "40.000");
    lv_label_set_text(ui_LabelTricklerWeight, "0.000 gn");
    lv_label_set_text(ui_LabelAddWeightCycle, "0.001");
    lv_label_set_text(ui_LabelInfo, "Robo-Trickler v2.15 // strenuous.dev");
    lv_label_set_text(ui_LabelToggleTrickler, UI_SYMBOL_START);
    savePreview("touch_trickler.ppm");

    setProfileTabEnabled(false);
    lv_label_set_text(ui_LabelToggleTrickler, UI_SYMBOL_STOP);
    lv_obj_set_style_bg_color(ui_ButtonToggleTrickler, lv_color_hex(0xFF0000), LV_PART_MAIN);
    lv_label_set_text(ui_LabelTricklerWeight, "39.600 gn");
    lv_label_set_text(ui_LabelInfo, "W0.241 ST84 RPM200 M5/3");
    savePreview("touch_running.ppm");
    lv_label_set_text(ui_LabelTricklerWeight, "40.000 gn");
    lv_obj_set_style_text_color(ui_LabelTricklerWeight, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_label_set_text(ui_LabelInfo, langText("status_done"));
    savePreview("touch_finished.ppm");
    setProfileTabEnabled(true);
    lv_label_set_text(ui_LabelToggleTrickler, UI_SYMBOL_START);
    lv_obj_set_style_bg_color(ui_ButtonToggleTrickler, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_set_style_text_color(ui_LabelTricklerWeight, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(ui_LabelTricklerWeight, "-.-");
    lv_label_set_text(ui_LabelInfo, langText("status_stopped"));
    savePreview("touch_stopped.ppm");

    lv_tabview_set_active(ui_TabView, 1, LV_ANIM_OFF);
    lv_label_set_text(ui_LabelProfile, "powder_1");
    savePreview("touch_profile.ppm");

    const char *titles[] = {"msg_tune_profile_title", "msg_tune_limit_factor_title",
                           "msg_tune_measurements_title", "msg_tune_steps_title"};
    const char *values[] = {"0.375", "0.650", "5", "167"};
    const char *files[] = {"touch_tune_weight.ppm", "touch_tune_factor.ppm",
                          "touch_tune_measurements.ppm", "touch_tune_steps.ppm"};
    for (int mode = 0; mode < 4; ++mode)
    {
        createProfileTuneDialog();
        lv_label_set_text(profileTuneTitleLabel, langText(titles[mode]));
        lv_label_set_text(profileTuneValueLabel, values[mode]);
        lv_label_set_text(profileTuneEntryLabel, mode < 2 ? "0.001" : "0.482");
        lv_label_set_text(profileTuneStepSizeLabel, "1");
        layoutProfileTuneDialog(mode == 3);
        showDialog(ui_PanelProfileTune);
        savePreview(files[mode]);
        closeDialog(&ui_PanelProfileTune, true);
        settle();
    }

    char message[256];
    std::snprintf(message, sizeof(message), "%spowder_1%s",
                  langText("msg_delete_profile_confirm_prefix"), langText("msg_delete_profile_confirm_suffix"));
    presentDialog(message, UI_FONT_LARGE, lv_color_white(), true, UI_SYMBOL_DELETE);
    savePreview("touch_delete.ppm");
    closeDialog(&ui_PanelMessages, true);
    ui_ButtonMessageNo = nullptr;

    lv_label_set_text(ui_LabelProfile, "calibrate");
    lv_obj_add_flag(ui_ButtonProfileTune, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_ButtonProfileDelete, LV_OBJ_FLAG_HIDDEN);
    savePreview("touch_calibrate.ppm");
    lv_tabview_set_active(ui_TabView, 0, LV_ANIM_OFF);
    std::snprintf(message, sizeof(message), "%s37.500 gn", langText("msg_create_profile_prompt"));
    presentDialog(message, UI_FONT_LARGE, lv_color_white(), true, UI_SYMBOL_CREATE);
    savePreview("touch_create_profile.ppm");
    closeDialog(&ui_PanelMessages, true);
    ui_ButtonMessageNo = nullptr;

    lv_tabview_set_active(ui_TabView, 2, LV_ANIM_OFF);
    std::snprintf(message, sizeof(message), "%spowder_1", langText("msg_profile_created"));
    presentDialog(message, UI_FONT_LARGE, lv_color_hex(0x00FF00), false, nullptr);
    savePreview("touch_profile_created.ppm");
    closeDialog(&ui_PanelMessages, true);
    ui_ButtonMessageNo = nullptr;
    lv_label_set_text(ui_LabelScaleProtocol, "Waage: G&G");
    lv_label_set_text(ui_LabelLog, "Robo-Trickler v2.15 // strenuous.dev\nSpeicher: SD-Karte\nWLAN verbunden\n192.168.178.50");
    lv_obj_add_state(ui_ButtonWifi, LV_STATE_CHECKED);
    lv_obj_clear_flag(ui_ButtonSyncFlashToSd, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_ButtonSyncSdToFlash, LV_OBJ_FLAG_HIDDEN);
    savePreview("touch_info.ppm");
    presentDialog(langText("msg_sync_flash_to_sd_confirm"), UI_FONT_LARGE,
                  lv_color_white(), true, UI_SYMBOL_COPY);
    savePreview("touch_sync.ppm");
    closeDialog(&ui_PanelMessages, true);
    ui_ButtonMessageNo = nullptr;
    presentDialog(langText("msg_sync_sd_to_flash_confirm"), UI_FONT_LARGE,
                  lv_color_white(), true, UI_SYMBOL_COPY);
    savePreview("touch_sync_to_flash.ppm");
    closeDialog(&ui_PanelMessages, true);
    ui_ButtonMessageNo = nullptr;
    std::puts("Rendered German manual screenshots (illustrative data; no hardware access).");
}
