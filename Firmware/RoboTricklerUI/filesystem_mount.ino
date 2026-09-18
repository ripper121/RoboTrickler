bool activeFilesystemAvailable()
{
  return filesystemActive && (activeFs != NULL);
}

bool filesystemLock()
{
  return (filesystemMutex != NULL) &&
         (xSemaphoreTakeRecursive(filesystemMutex, 0) == pdTRUE);
}

void filesystemUnlock()
{
  if (filesystemMutex != NULL)
  {
    xSemaphoreGiveRecursive(filesystemMutex);
  }
}

class FilesystemLockGuard
{
public:
  FilesystemLockGuard() : locked(filesystemLock()) {}
  ~FilesystemLockGuard()
  {
    if (locked)
    {
      filesystemUnlock();
    }
  }
  operator bool() const { return locked; }

private:
  bool locked;
};

// Restore a file left between the original-to-backup and temp-to-original
// renames. This works for either mounted filesystem, including an inactive
// sync destination that may become active on a later boot.
bool recoverFilesystemBackup(fs::FS &filesystem, const char *path)
{
  String backupPath = String(path) + ".bak";
  if (!filesystem.exists(backupPath.c_str()))
  {
    return true;
  }
  if (filesystem.exists(path))
  {
    return filesystem.remove(backupPath.c_str());
  }
  return filesystem.rename(backupPath.c_str(), path);
}

static void recoverFilesystemSyncBackups(fs::FS &filesystem)
{
  recoverFilesystemBackup(filesystem, "/config.txt");
  File directory = filesystem.open("/profiles");
  if (!directory || !directory.isDirectory())
  {
    directory.close();
    return;
  }
  File entry = directory.openNextFile();
  while (entry)
  {
    String path = entry.path();
    bool isBackup = !entry.isDirectory() && path.endsWith(".bak");
    entry.close();
    if (isBackup)
    {
      path.remove(path.length() - 4);
      recoverFilesystemBackup(filesystem, path.c_str());
    }
    entry = directory.openNextFile();
  }
  directory.close();
}

bool initFilesystem()
{
  if (filesystemMutex == NULL)
  {
    filesystemMutex = xSemaphoreCreateRecursiveMutex();
    if (filesystemMutex == NULL)
    {
      return false;
    }
  }
  filesystemActive = false;
  activeFs = NULL;
  activeFsIsSd = false;
  sdMounted = false;
  littleFsMounted = false;

  sdSpi = new SPIClass(HSPI);
  sdSpi->begin(GRBL_SPI_SCK, GRBL_SPI_MISO, GRBL_SPI_MOSI, GRBL_SPI_SS);
  delay(100); // give the SD card time to initialize before querying it
  bool sdBegan = SD.begin(GRBL_SPI_SS, *sdSpi, SD_SPI_FREQ, "/sd", 5);
  delay(100); // give the SD card time to initialize before querying it
  if (sdBegan && (SD.cardType() != CARD_NONE))
  {
    sdMounted = true;
    activeFs = &SD;
    activeFsIsSd = true;
    filesystemActive = true;
    DEBUG_PRINTLN("SD card mounted.");
  }
  else
  {
    DEBUG_PRINTLN("SD card mount failed or card not present.");
    SD.end();
    delete sdSpi;
    sdSpi = NULL;
  }

  if (LittleFS.begin(false))
  {
    littleFsMounted = true;
    if (!filesystemActive)
    {
      activeFs = &LittleFS;
      activeFsIsSd = false;
      filesystemActive = true;
    }
    DEBUG_PRINTLN("LittleFS mounted.");
  }
  else
  {
    DEBUG_PRINTLN("LittleFS mount failed.");
  }

  if (sdMounted)
  {
    DEBUG_PRINT("SD total bytes: ");
    DEBUG_PRINTLN(SD.totalBytes());
    DEBUG_PRINT("SD used bytes: ");
    DEBUG_PRINTLN(SD.usedBytes());
  }
  if (littleFsMounted)
  {
    DEBUG_PRINT("LittleFS total bytes: ");
    DEBUG_PRINTLN(LittleFS.totalBytes());
    DEBUG_PRINT("LittleFS used bytes: ");
    DEBUG_PRINTLN(LittleFS.usedBytes());
  }
  if (sdMounted)
  {
    recoverFilesystemSyncBackups(SD);
  }
  if (littleFsMounted)
  {
    recoverFilesystemSyncBackups(LittleFS);
  }
  return filesystemActive;
}
