extern int selectedProfileIndex;
extern std::atomic<bool> messageBoxOpen;
bool profileDeleteConfirmPending = false;
String profileDeleteName = "";
String profileDeleteFilename = "";

// The profile tune dialog (its state, widgets, and event callbacks) lives in
// ui_dialogs.ino next to the other dialogs. Only the profile data side stays
// here. deleteSelectedProfile() reaches into it via the non-static helpers
// isProfileTuneDialogOpen()/closeProfileTuneDialog()/clearProfileTuneState().

// Recover from a profile that failed to load by switching to the built-in
// calibration profile in memory. Returns true when calibrate is loaded and
// config is left in a consistent state, false only if calibrate itself is
// unusable (the last-resort reboot path).
//
// `blocking` must be false when called from the LVGL display task (any profile
// switch triggered by a touch event). It is only consulted on the fatal reboot
// path, where errorBox(..., true) pumps lv_timer_handler() via pumpUntil();
// re-entering the handler from inside an event callback crashes LVGL, so only
// the Core 1 startup path may block.
bool recoverCorruptProfile(String badFilename, bool blocking)
{
    FilesystemLockGuard filesystemGuard;
    if (!filesystemGuard || isWebFileUploadActive() || !activeFilesystemAvailable())
    {
        return false;
    }
    String readError = getSdReadError();
    String message = String(langText("msg_profile_corrupted")) + badFilename;
    if (readError.length() > 0)
    {
        updateDisplayLog(readError);
        message += "\n\n";
        message += readError;
    }
    // Quarantine a genuinely corrupt file so it is not picked again. A missing
    // file simply does not exist, so nothing is renamed in that case.
    if (ACTIVE_FS.exists(badFilename))
    {
        String corruptedName = String(badFilename);
        corruptedName.replace(".txt", ".cor.txt");
        // Keep one recovery copy, matching config recovery. Remove the old
        // copy first so SD and LittleFS behave consistently.
        ACTIVE_FS.remove(corruptedName.c_str());
        if (ACTIVE_FS.rename(badFilename, corruptedName.c_str()))
        {
            DEBUG_PRINT("Corrupted file renamed to: ");
            DEBUG_PRINTLN(corruptedName);
        }
        else
        {
            DEBUG_PRINTLN("Failed to rename corrupted file");
        }
    }

    // Switch to the calibration profile and load it in memory instead of
    // rebooting. ensureCalibrateProfile() regenerates the calibrate file from
    // the built-in template if it is itself missing or corrupt, and loadProfile()
    // resets every profile field before applying the values, so config ends up
    // fully consistent.
    strlcpy(config.profileName,          // <- destination
            CALIBRATE_PROFILE_NAME,      // <- source
            sizeof(config.profileName)); // <- destination's capacity
    profileSelectionUnsaved = !saveConfiguration("/config.txt", config);

    if (ensureCalibrateProfile(config))
    {
        targetWeightUnsaved = false;
        message += langText("msg_calibration_profile_loaded");
        // Refresh the list from the filesystem so the quarantined/missing
        // profile drops out of the selection and findProfileIndex() searches
        // the current state rather than the stale buffer.
        refreshProfileList();

        int calibrateIndex = findProfileIndex(CALIBRATE_PROFILE_NAME);
        if (calibrateIndex >= 0)
        {
            selectedProfileIndex = calibrateIndex;
        }

        setLabelText(ui_LabelProfile, config.profileName);
        updateProfileActionButtonVisibility();
        updateTargetWeightLabel();

        errorBox(message, false);
        return true;
    }

    // Even regenerating the calibration profile failed (the filesystem is
    // unwritable), so there is no good in-memory state to fall back to. Reboot
    // to recover through the normal boot path.
    message += langText("msg_calibration_profile_recovery_failed");
    DEBUG_PRINTLN("Calibration profile recovery failed; rebooting");
    delay(100);
    restartNow = true;
    errorBox(message, blocking);
    return false;
}

bool loadSelectedProfile(bool blocking)
{
    String selectedProfileFilename = profileFilename(config.profileName);
    if (!loadProfile(selectedProfileFilename.c_str(), config))
    {
        // recoverCorruptProfile() recovers onto calibrate in memory; it returns true
        // when that succeeds so the caller can continue without a reboot.
        return recoverCorruptProfile(selectedProfileFilename, blocking);
    }
    targetWeightUnsaved = false;
    return true;
}

void setProfile(int index)
{
    if (isTricklerRunning() || isWebFileUploadActive() || !activeFilesystemAvailable() ||
        (index < 0) || (index >= profileListCount))
    {
        DEBUG_PRINT("Invalid profile index: ");
        DEBUG_PRINTLN(index);
        return;
    }

    cancelInteractiveDialogs();

    strlcpy(config.profileName,          // <- destination
            profileList[index],    // <- source
            sizeof(config.profileName)); // <- destination's capacity
    selectedProfileIndex = index;

    String infoText = String(langText("status_selecting_profile")) + config.profileName;
    updateDisplayLog(infoText, true);

    // Defer the config.txt write: browsing prev/next would otherwise rewrite
    // the file on every tap. startTrickler() persists the selection when a run
    // starts; delete/create persist explicitly right after calling setProfile().
    profileSelectionUnsaved = true;

    if (!loadSelectedProfile(false))
    {
        return;
    }

    DEBUG_PRINT("index: ");
    DEBUG_PRINTLN(index);

    setLabelText(ui_LabelProfile, config.profileName);
    updateProfileActionButtonVisibility();
    updateTargetWeightLabel();
    infoText = String(langText("status_profile_ready")) + config.profileName;
    updateDisplayLog(infoText, true);
}

int findProfileIndex(const char *profileName)
{
    for (int i = 0; i < profileListCount; i++)
    {
        if (strcmp(profileList[i], profileName) == 0)
        {
            return i;
        }
    }
    return -1;
}

bool deleteSelectedProfile()
{
    // Deletion is two-stage so the LVGL confirm dialog can return through its
    // normal event callback instead of blocking the UI task.
    FilesystemLockGuard filesystemGuard;
    if (!filesystemGuard || isWebFileUploadActive() || !activeFilesystemAvailable() || messageBoxOpen.load())
    {
        return false;
    }

    if (isProfileTuneDialogOpen())
    {
        closeProfileTuneDialog();
        clearProfileTuneState();
    }

    String profileName = config.profileName;
    if (profileName == CALIBRATE_PROFILE_NAME)
    {
        errorBox(langText("msg_cannot_delete_calibrate_profile"), false);
        return false;
    }

    String filename = profileFilename(profileName.c_str());
    if (!ACTIVE_FS.exists(filename.c_str()))
    {
        errorBox(String(langText("msg_delete_profile_file_not_found")) + filename, false);
        refreshProfileList();
        return false;
    }

    profileDeleteName = profileName;
    profileDeleteFilename = filename;
    profileDeleteConfirmPending = true;
    showConfirmBox(String(langText("msg_delete_profile_confirm_prefix")) + profileName + langText("msg_delete_profile_confirm_suffix"), UI_FONT_LARGE, lv_color_hex(0xFFFFFF), langText("action_delete"));
    return true;
}

void finishProfileDeleteConfirm(bool confirmed)
{
    if (!profileDeleteConfirmPending)
    {
        return;
    }

    String profileName = profileDeleteName;
    String filename = profileDeleteFilename;
    profileDeleteConfirmPending = false;
    profileDeleteName = "";
    profileDeleteFilename = "";

    if (!confirmed)
    {
        return;
    }

    FilesystemLockGuard filesystemGuard;
    if (!filesystemGuard || isWebFileUploadActive() || !activeFilesystemAvailable())
    {
        errorBox(langText("msg_sync_filesystems_unavailable"), false);
        return;
    }

    if ((profileName.length() <= 0) || (profileName == CALIBRATE_PROFILE_NAME))
    {
        errorBox(langText("msg_cannot_delete_profile"), false);
        return;
    }

    if (!ACTIVE_FS.exists(filename.c_str()))
    {
        errorBox(String(langText("msg_delete_profile_file_not_found")) + filename, false);
        refreshProfileList();
        return;
    }

    int calibrateIndex = findProfileIndex(CALIBRATE_PROFILE_NAME);
    if (calibrateIndex < 0)
    {
        errorBox(langText("msg_delete_profile_calibrate_missing"), false);
        return;
    }

    setProfile(calibrateIndex);
    if (!isCalibrationProfile())
    {
        errorBox(langText("msg_delete_profile_calibrate_load_failed"), false);
        return;
    }

    // setProfile() defers config saves; persist the calibrate selection before
    // the file disappears so config.txt never names a deleted profile.
    if (!saveConfiguration("/config.txt", config))
    {
        profileSelectionUnsaved = true;
        errorBox(langText("status_saving_config_failed"), false);
        return;
    }

    profileSelectionUnsaved = false;

    if (!ACTIVE_FS.remove(filename.c_str()))
    {
        errorBox(String(langText("msg_could_not_delete_profile")) + filename, false);
        refreshProfileList();
        return;
    }

    refreshProfileList();
    updateProfileActionButtonVisibility();
    successBox(String(langText("msg_profile_deleted")) + profileName, false);
}
