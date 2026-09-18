String firmwareCheckUrl()
{
  return String(DEFAULT_FW_UPDATE_URL) + "?mac=" + String(WiFi.macAddress()) + "&version=" + String(FW_VERSION);
}

class FirmwareVersionResponse : public Stream
{
public:
  static const size_t MAX_LENGTH = 64;
  FirmwareVersionResponse() : length(0), overflow(false) { buffer[0] = '\0'; }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
  size_t write(uint8_t value) override { return write(&value, 1); }
  size_t write(const uint8_t *data, size_t size) override
  {
    if (size > MAX_LENGTH - length)
    {
      overflow = true;
      return 0;
    }
    memcpy(buffer + length, data, size);
    length += size;
    buffer[length] = '\0';
    return size;
  }
  char buffer[MAX_LENGTH + 1];
  size_t length;
  bool overflow;
};

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
  int comparison = 0;
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

    // Keep parsing after finding a difference so malformed trailing segments
    // cannot turn a bad payload such as "3.bad" into an accepted update.
    if ((comparison == 0) && (leftSegment > rightSegment))
    {
      comparison = 1;
    }
    else if ((comparison == 0) && (leftSegment < rightSegment))
    {
      comparison = -1;
    }
  }

  return comparison;
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

    if (httpResponseCode == HTTP_CODE_OK)
    {
      DEBUG_PRINT("HTTP Response code: ");
      DEBUG_PRINTLN(httpResponseCode);
      // HTTPClient handles both fixed and chunked responses. Keep the decoded
      // body in a fixed buffer regardless of Content-Length.
      int contentSize = http.getSize();
      if (contentSize > (int)FirmwareVersionResponse::MAX_LENGTH)
      {
        DEBUG_PRINTLN("Firmware version response too long");
        http.end();
        return;
      }
      FirmwareVersionResponse response;
      int bytesRead = http.writeToStream(&response);
      if ((bytesRead < 0) || response.overflow ||
          ((contentSize >= 0) && (bytesRead != contentSize)))
      {
        DEBUG_PRINTLN("Invalid firmware version response length");
        http.end();
        return;
      }
      String payload(response.buffer);
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
