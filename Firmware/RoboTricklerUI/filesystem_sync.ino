extern volatile bool messageBoxOpen;
FilesystemSyncDirection pendingFilesystemSync = FILESYSTEM_SYNC_NONE;

#if ENABLE_LITTLEFS

bool copyFilesystemFile(fs::FS &source, fs::FS &destination, const char *path)
{
  File sourceFile = source.open(path, FILE_READ);
  if (!sourceFile || sourceFile.isDirectory())
  {
    sourceFile.close();
    return false;
  }

  char temporaryPath[96];
  snprintf(temporaryPath, sizeof(temporaryPath), "%s.sync.tmp", path);
  destination.remove(temporaryPath);

  File destinationFile = destination.open(temporaryPath, FILE_WRITE);
  if (!destinationFile)
  {
    sourceFile.close();
    return false;
  }

  uint8_t buffer[512];
  bool copied = true;
  while (sourceFile.available())
  {
    size_t bytesRead = sourceFile.read(buffer, sizeof(buffer));
    if ((bytesRead == 0) || (destinationFile.write(buffer, bytesRead) != bytesRead))
    {
      copied = false;
      break;
    }
    yield();
  }

  destinationFile.close();
  sourceFile.close();

  if (!copied)
  {
    destination.remove(temporaryPath);
    return false;
  }

  destination.remove(path);
  if (!destination.rename(temporaryPath, path))
  {
    destination.remove(temporaryPath);
    return false;
  }
  return true;
}

bool syncConfigAndProfiles(fs::FS &source, fs::FS &destination, int &copiedFiles)
{
  copiedFiles = 0;
  if (!source.exists("/config.txt") || !copyFilesystemFile(source, destination, "/config.txt"))
  {
    return false;
  }
  copiedFiles++;

  if (!source.exists("/profiles"))
  {
    return true;
  }
  if (!destination.exists("/profiles") && !destination.mkdir("/profiles"))
  {
    return false;
  }

  File profileDirectory = source.open("/profiles");
  if (!profileDirectory || !profileDirectory.isDirectory())
  {
    profileDirectory.close();
    return false;
  }

  File profileFile = profileDirectory.openNextFile();
  while (profileFile)
  {
    if (!profileFile.isDirectory())
    {
      char profilePath[96];
      strlcpy(profilePath, profileFile.path(), sizeof(profilePath));
      profileFile.close();
      if ((strncmp(profilePath, "/profiles/", 10) != 0) ||
          !copyFilesystemFile(source, destination, profilePath))
      {
        profileDirectory.close();
        return false;
      }
      copiedFiles++;
    }
    else
    {
      profileFile.close();
    }
    profileFile = profileDirectory.openNextFile();
  }

  profileDirectory.close();
  return true;
}

void updateFilesystemSyncControls()
{
  if ((ui_ButtonSyncFlashToSd == NULL) || (ui_ButtonSyncSdToFlash == NULL))
  {
    return;
  }

  bool showControls = sdMounted && littleFSMounted;
  if (lvglLock())
  {
    if (showControls)
    {
      lv_obj_clear_flag(ui_ButtonSyncFlashToSd, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(ui_ButtonSyncSdToFlash, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
      lv_obj_add_flag(ui_ButtonSyncFlashToSd, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_ButtonSyncSdToFlash, LV_OBJ_FLAG_HIDDEN);
    }

    char label[64];
    snprintf(label, sizeof(label), "%s %s", LV_SYMBOL_UPLOAD, langText("label_sync_flash_to_sd"));
    lv_label_set_text(ui_LabelSyncFlashToSd, label);
    snprintf(label, sizeof(label), "%s %s", LV_SYMBOL_DOWNLOAD, langText("label_sync_sd_to_flash"));
    lv_label_set_text(ui_LabelSyncSdToFlash, label);
    lvglUnlock();
  }
}

void requestFilesystemSync(FilesystemSyncDirection direction)
{
  if (messageBoxOpen)
  {
    return;
  }
  if (isTricklerRunning())
  {
    errorBox(langText("msg_stop_trickler_before_sync"), false);
    return;
  }
  if (!sdMounted || !littleFSMounted)
  {
    errorBox(langText("msg_sync_filesystems_unavailable"), false);
    updateFilesystemSyncControls();
    return;
  }

  pendingFilesystemSync = direction;
  const char *messageKey = (direction == FILESYSTEM_SYNC_FLASH_TO_SD)
                               ? "msg_sync_flash_to_sd_confirm"
                               : "msg_sync_sd_to_flash_confirm";
  showConfirmBox(langText(messageKey), UI_FONT_NORMAL, lv_color_hex(0xFFFFFF));
}

void finishFilesystemSyncConfirm(bool confirmed)
{
  FilesystemSyncDirection direction = pendingFilesystemSync;
  pendingFilesystemSync = FILESYSTEM_SYNC_NONE;
  if (!confirmed || (direction == FILESYSTEM_SYNC_NONE))
  {
    return;
  }
  if (!sdMounted || !littleFSMounted)
  {
    errorBox(langText("msg_sync_filesystems_unavailable"), false);
    updateFilesystemSyncControls();
    return;
  }

  fs::FS &source = (direction == FILESYSTEM_SYNC_FLASH_TO_SD) ? static_cast<fs::FS &>(LittleFS) : static_cast<fs::FS &>(SD);
  fs::FS &destination = (direction == FILESYSTEM_SYNC_FLASH_TO_SD) ? static_cast<fs::FS &>(SD) : static_cast<fs::FS &>(LittleFS);

  int copiedFiles = 0;
  if (!syncConfigAndProfiles(source, destination, copiedFiles))
  {
    errorBox(langText("msg_sync_failed"), false);
    return;
  }

  char message[96];
  snprintf(message, sizeof(message), langText("msg_sync_complete"), copiedFiles);
  if (direction == FILESYSTEM_SYNC_FLASH_TO_SD)
  {
    strlcat(message, langText("msg_sync_restarting"), sizeof(message));
    restart_now = true;
  }
  successBox(message, false);
}

void syncFlashToSd_event_cb(lv_event_t *e)
{
  requestFilesystemSync(FILESYSTEM_SYNC_FLASH_TO_SD);
}

void syncSdToFlash_event_cb(lv_event_t *e)
{
  requestFilesystemSync(FILESYSTEM_SYNC_SD_TO_FLASH);
}

#else

void updateFilesystemSyncControls()
{
  if ((ui_ButtonSyncFlashToSd == NULL) || (ui_ButtonSyncSdToFlash == NULL) || !lvglLock())
  {
    return;
  }
  lv_obj_add_flag(ui_ButtonSyncFlashToSd, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_ButtonSyncSdToFlash, LV_OBJ_FLAG_HIDDEN);
  lvglUnlock();
}

void finishFilesystemSyncConfirm(bool confirmed)
{
  pendingFilesystemSync = FILESYSTEM_SYNC_NONE;
}

void syncFlashToSd_event_cb(lv_event_t *e)
{
}

void syncSdToFlash_event_cb(lv_event_t *e)
{
}

#endif
