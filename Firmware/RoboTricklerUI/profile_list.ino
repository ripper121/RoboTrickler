static String fileNameFromPath(const char *path)
{
  String fileName = String(path);
  int slashIndex = fileName.lastIndexOf('/');
  if (slashIndex >= 0)
  {
    fileName = fileName.substring(slashIndex + 1);
  }
  return fileName;
}

static bool isProfileCandidateFile(const String &fileName)
{
  return fileName.endsWith(".txt") &&
         !fileName.endsWith("config.txt") &&
         (fileName.indexOf(".cor") == -1);
}

static bool profileListContains(const String &profileName, byte profileCounter)
{
  for (int i = 0; i < profileCounter; i++)
  {
    if (profileListBuff[i] == profileName)
    {
      return true;
    }
  }
  return false;
}

static void addProfileName(const String &fileName, byte &profileCounter)
{
  String profileName = fileName;
  profileName.replace(".txt", "");
  if (!profileListContains(profileName, profileCounter))
  {
    profileListBuff[profileCounter] = profileName;
    profileCounter++;
  }
}

static void rememberInvalidProfile(const String &fileName, byte &invalidProfileCounter, String &invalidProfiles)
{
  updateDisplayLog(String(languageText("status_invalid_profile_ignored")) + fileName);
  invalidProfileCounter++;
  if (invalidProfileCounter <= MAX_INVALID_PROFILES_SHOWN)
  {
    invalidProfiles += "\n";
    invalidProfiles += fileName;
  }
}

void scanProfileDirectory(const char *directory, byte &profileCounter, byte &invalidProfileCounter, String &invalidProfiles)
{
  File root = SD.open(directory);
  if (!root)
  {
    DEBUG_PRINTLN("Failed to open directory");
    return;
  }
  if (!root.isDirectory())
  {
    DEBUG_PRINTLN("Not a directory");
    root.close();
    return;
  }

  File file = root.openNextFile();

  while (file)
  {
    if (profileCounter >= MAX_PROFILE_LIST_ITEMS)
    {
      file.close();
      break;
    }
    if (file.isDirectory())
    {
      DEBUG_PRINT("  DIR : ");
      DEBUG_PRINTLN(file.name());
    }
    else
    {
      String fileName = fileNameFromPath(file.name());
      bool isProfileCandidate = isProfileCandidateFile(fileName);
      if (isProfileCandidate && isValidProfileFile(file.path()))
      {
        addProfileName(fileName, profileCounter);
      }
      else if (isProfileCandidate)
      {
        rememberInvalidProfile(fileName, invalidProfileCounter, invalidProfiles);
      }

      DEBUG_PRINT("  FILE: ");
      DEBUG_PRINT(file.name());
      DEBUG_PRINT("  SIZE: ");
      DEBUG_PRINTLN(file.size());
    }
    file.close();
    file = root.openNextFile();
  }

  root.close();
}

int profileListIndexOf(const char *profileName)
{
  String requestedProfile = String(profileName);
  for (int i = 0; i < profileListCount; i++)
  {
    if (requestedProfile == profileListBuff[i])
    {
      return i;
    }
  }
  return -1;
}

void refreshProfileList()
{
  profileListCount = 0;

  updateDisplayLog(languageText("status_search_profiles"));

  byte profileCounter = 0;
  byte invalidProfileCounter = 0;
  String invalidProfiles = "";

  if (SD.exists("/profiles"))
  {
    scanProfileDirectory("/profiles", profileCounter, invalidProfileCounter, invalidProfiles);
  }

  profileListCount = profileCounter;
  DEBUG_PRINT("  profileListCount: ");
  DEBUG_PRINTLN(profileListCount);
  for (int i = 0; i < profileListCount; i++)
  {
    DEBUG_PRINTLN(profileListBuff[i]);
  }
  if (invalidProfileCounter > 0)
  {
    String message = languageText("msg_invalid_profiles_found");
    message += invalidProfiles;
    if (invalidProfileCounter > MAX_INVALID_PROFILES_SHOWN)
    {
      message += "\n...";
    }
    message += languageText("msg_invalid_profiles_ignored");
    messageBox(message.c_str(), &lv_font_montserrat_14, lv_color_hex(UI_COLOR_ERROR), true);
  }
}
