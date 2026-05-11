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
    if (profileCounter > 31)
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
      String fileName = String(file.name());
      int slashIndex = fileName.lastIndexOf('/');
      if (slashIndex >= 0)
      {
        fileName = fileName.substring(slashIndex + 1);
      }
      bool isProfileCandidate = fileName.endsWith(".txt") && !fileName.endsWith("config.txt") && (fileName.indexOf(".cor") == -1);
      if (isProfileCandidate && isValidProfileFile(file.path()))
      {
        String filename = fileName;
        filename.replace(".txt", "");
        bool duplicate = false;
        for (int i = 0; i < profileCounter; i++)
        {
          if (profileListBuff[i] == filename)
          {
            duplicate = true;
            break;
          }
        }
        if (!duplicate)
        {
          profileListBuff[profileCounter] = filename;
          profileCounter++;
        }
      }
      else if (isProfileCandidate)
      {
        updateDisplayLog(String(languageText("status_invalid_profile_ignored")) + fileName);
        invalidProfileCounter++;
        if (invalidProfileCounter <= 8)
        {
          invalidProfiles += "\n";
          invalidProfiles += fileName;
        }
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
    if (invalidProfileCounter > 8)
    {
      message += "\n...";
    }
    message += languageText("msg_invalid_profiles_ignored");
    messageBox(message.c_str(), &lv_font_montserrat_14, lv_color_hex(0xFF0000), true);
  }
}
