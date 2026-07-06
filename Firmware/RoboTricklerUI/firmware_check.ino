String firmwareCheckUrl()
{
  return String(DEFAULT_FW_UPDATE_URL) + "?mac=" + String(WiFi.macAddress()) + "&version=" + String(FW_VERSION);
}

String normalizeFirmwareVersion(String version)
{
  version.trim();
  if ((version.length() > 0) && ((version.charAt(0) == 'v') || (version.charAt(0) == 'V')))
  {
    version.remove(0, 1);
    version.trim();
  }
  return version;
}

bool readFirmwareVersionSegment(const String &version, int &index, unsigned long &segment, bool &hasSegment)
{
  // Parse dotted numeric versions without atoi() so malformed payloads and
  // overflowing segments cannot be treated as valid updates.
  segment = 0;
  hasSegment = false;
  if (index >= version.length())
  {
    return true;
  }

  int digitCount = 0;
  while (index < version.length())
  {
    char c = version.charAt(index);
    if (!isdigit((unsigned char)c))
    {
      break;
    }

    unsigned long digit = (unsigned long)(c - '0');
    if ((segment > 429496729UL) || ((segment == 429496729UL) && (digit > 5)))
    {
      return false;
    }
    segment = (segment * 10UL) + digit;
    digitCount++;
    index++;
  }

  if (digitCount == 0)
  {
    return false;
  }

  hasSegment = true;
  if (index >= version.length())
  {
    return true;
  }

  if (version.charAt(index) != '.')
  {
    return false;
  }

  index++;
  return index < version.length();
}

int compareFirmwareVersions(const String &leftVersion, const String &rightVersion, bool &valid)
{
  String left = normalizeFirmwareVersion(leftVersion);
  String right = normalizeFirmwareVersion(rightVersion);
  valid = (left.length() > 0) && (right.length() > 0);
  if (!valid)
  {
    return 0;
  }

  int leftIndex = 0;
  int rightIndex = 0;
  while ((leftIndex < left.length()) || (rightIndex < right.length()))
  {
    unsigned long leftSegment = 0;
    unsigned long rightSegment = 0;
    bool leftHasSegment = false;
    bool rightHasSegment = false;

    if (!readFirmwareVersionSegment(left, leftIndex, leftSegment, leftHasSegment) ||
        !readFirmwareVersionSegment(right, rightIndex, rightSegment, rightHasSegment))
    {
      valid = false;
      return 0;
    }

    if (leftSegment > rightSegment)
    {
      return 1;
    }
    if (leftSegment < rightSegment)
    {
      return -1;
    }
  }

  return 0;
}

bool isRemoteFirmwareNewer(const String &remoteVersion)
{
  bool valid = false;
  int comparison = compareFirmwareVersions(FW_VERSION, remoteVersion, valid);
  if (!valid)
  {
    DEBUG_PRINT("Invalid firmware version payload: ");
    DEBUG_PRINTLN(remoteVersion);
    return false;
  }
  return comparison < 0;
}

void makeHttpGetRequest(String serverPath)
{
  HTTPClient http;
  http.setTimeout(5000);

  if (http.begin(serverPath))
  {
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0)
    {
      DEBUG_PRINT("HTTP Response code: ");
      DEBUG_PRINTLN(httpResponseCode);
      String payload = http.getString();
      DEBUG_PRINTLN(payload);
      if (isRemoteFirmwareNewer(payload))
      {
        successBox(String(langText("msg_new_firmware")) + payload + langText("msg_check_url"), true);
      }
    }
    else
    {
      DEBUG_PRINT("Error code: ");
      DEBUG_PRINTLN(httpResponseCode);
      DEBUG_PRINT("HTTP error: ");
      DEBUG_PRINTLN(HTTPClient::errorToString(httpResponseCode));
    }
    http.end();
  }
  else
  {
    DEBUG_PRINTLN("Unable to connect");
  }
}

