bool serialWait()
{
  bool timeout = true;
  for (int i = 0; i < 2000; i++)
  {
    if (Serial1.available())
    {
      timeout = false;
      break;
    }
    delay(1);
  }
  return timeout;
}

void serialFlush()
{
  // flush TX
  Serial1.flush();
  // flush RX
  while (Serial1.available())
  {
    Serial1.read();
  }
}

String serialBytesToDisplay(const uint8_t *bytes, int byteCount)
{
  String data = "";
  data.reserve(byteCount);

  for (int i = 0; i < byteCount; i++)
  {
    if ((bytes[i] >= 32) && (bytes[i] <= 126))
    {
      data += (char)bytes[i];
    }
    else if (bytes[i] == '\r')
    {
      data += "\\r";
    }
    else if (bytes[i] == '\n')
    {
      data += "\\n";
    }
    else if (bytes[i] == '\t')
    {
      data += "\\t";
    }
    else
    {
      data += ".";
    }
  }

  return data;
}

String serialBytesToHex(const uint8_t *bytes, int byteCount)
{
  String data = "";
  data.reserve(byteCount * 5);

  for (int i = 0; i < byteCount; i++)
  {
    char byteText[6];
    snprintf(byteText, sizeof(byteText), "0x%02X", bytes[i]);
    if (i > 0)
    {
      data += " ";
    }
    data += byteText;
  }

  return data;
}

bool serialReq(String req, bool flush)
{
  char request[128];
  req.toCharArray(request, sizeof(request));

  uint8_t bytes[64];
  int byteCount = 0;
  bool hexRequest = true;
  bool hasToken = false;

  char *token = strtok(request, " ,\t\r\n");
  while (token != NULL)
  {
    hasToken = true;
    if ((strlen(token) < 3) || (token[0] != '0') || ((token[1] != 'x') && (token[1] != 'X')) || (byteCount >= (int)sizeof(bytes)))
    {
      hexRequest = false;
      break;
    }

    char *end = NULL;
    unsigned long value = strtoul(token + 2, &end, 16);
    if ((*end != '\0') || (value > 0xFF))
    {
      hexRequest = false;
      break;
    }

    bytes[byteCount++] = (uint8_t)value;
    token = strtok(NULL, " ,\t\r\n");
  }

  if (!hexRequest || !hasToken)
  {
    DEBUG_PRINT("Invalid serial request hex string: ");
    DEBUG_PRINTLN(req);
    return true;
  }

  if (flush)
  {
    serialFlush();
  }

  if (strcmp(config.scale_protocol, "CUSTOM") == 0)
  {
    updateDisplayLog("TX:" + serialBytesToDisplay(bytes, byteCount) + " / " + serialBytesToHex(bytes, byteCount), false);
  }

  Serial1.write(bytes, byteCount);

  return serialWait();
}

void initRs232()
{
  Serial1.begin(config.scale_baud, SERIAL_8N1, SCALE_RX_PIN, SCALE_TX_PIN);
}

// Function to find the first decimal weight, return a float, and count decimal places
void stringToWeight(const char *input, float *value, int *decimalPlaces)
{
  char filteredBuffer[64]; // Make sure this is large enough to hold the filtered result
  *value = -1.0;
  *decimalPlaces = 0;

  if (input == NULL)
  {
    return;
  }

  for (int i = 0; input[i] != '\0'; i++)
  {
    int scan = i;

    if ((input[scan] == '+') || (input[scan] == '-'))
    {
      scan++;
    }

    int digitStart = scan;
    int digitsBeforeDot = 0;
    while ((input[scan] >= '0') && (input[scan] <= '9'))
    {
      digitsBeforeDot++;
      scan++;
    }

    if ((digitsBeforeDot == 0) || (input[scan] != '.'))
    {
      continue;
    }

    scan++;
    int decimals = 0;
    while ((input[scan] >= '0') && (input[scan] <= '9'))
    {
      decimals++;
      scan++;
    }

    if (decimals == 0)
    {
      continue;
    }

    bool negative = false;
    for (int signScan = 0; signScan < digitStart; signScan++)
    {
      if (input[signScan] == '-')
      {
        negative = true;
        break;
      }
    }

    int len = scan - digitStart;
    int bufferOffset = 0;
    if (negative)
    {
      filteredBuffer[bufferOffset++] = '-';
    }
    if (len >= (int)sizeof(filteredBuffer))
    {
      len = sizeof(filteredBuffer) - 1 - bufferOffset;
    }
    memcpy(filteredBuffer + bufferOffset, input + digitStart, len);
    filteredBuffer[bufferOffset + len] = '\0';

    *decimalPlaces = decimals;
    *value = atof(filteredBuffer);
    return;
  }
}

bool containsIgnoreCase(const char *text, const char *needle)
{
  if ((text == NULL) || (needle == NULL) || (*needle == '\0'))
  {
    return false;
  }

  for (; *text != '\0'; text++)
  {
    const char *h = text;
    const char *n = needle;
    while ((*h != '\0') && (*n != '\0') && (tolower((unsigned char)*h) == tolower((unsigned char)*n)))
    {
      h++;
      n++;
    }
    if (*n == '\0')
    {
      return true;
    }
  }
  return false;
}

void readWeight()
{
  if (Serial1.available())
  {
    char buff[64];
    size_t bytesRead = Serial1.readBytesUntil(0x0A, buff, sizeof(buff) - 1);
    buff[bytesRead] = '\0';

    if (strcmp(config.scale_protocol, "CUSTOM") == 0)
    {
      updateDisplayLog("RX:" + String(buff), false);
    }

    DEBUG_PRINTLN(buff);

    weight = -1.0;

    if (strchr(buff, '.') != NULL)
    {
      int decimalPlaces = 0;
      stringToWeight(buff, &weight, &decimalPlaces);
      decPlaces = (decimalPlaces > 0) ? decimalPlaces : DEC_PLACES;

      if (containsIgnoreCase(buff, "g"))
      {
        unit = " g";
        if (containsIgnoreCase(buff, "gn") || containsIgnoreCase(buff, "gr"))
        {
          unit = " gn";
        }
      }

      DEBUG_PRINT("Weight: ");
      DEBUG_PRINTLN(weight);
      lastScaleWeightReadTime = millis();
      DEBUG_PRINT("Decimal places: ");
      DEBUG_PRINTLN(decPlaces);

      DEBUG_PRINT("Weight Counter: ");
      DEBUG_PRINTLN(weightCounter);

      long weightRounded = lround(weight * 1000.0f);
      long lastWeightRounded = lround(lastWeight * 1000.0f);

      if (weightRounded == lastWeightRounded)
      {
        if (running || calibrationProfilePromptPending)
        {
          if (weightCounter >= measurementCount)
          {
            newData = true;
            weightCounter = 0;
          }
          weightCounter++;
        }
      }
      else
      {
        weightCounter = 0;
      }
      lastWeight = weight;
    }
  }
  else
  {
    bool timeout = false;
    if (strcmp(config.scale_protocol, "GG") == 0)
    {
      timeout = serialReq("0x1B 0x70 0x0D 0x0A", false);
    }
    else if (strcmp(config.scale_protocol, "AD") == 0)
    {
      timeout = serialReq("0x51 0x0D 0x0A", true);
    }
    else if (strcmp(config.scale_protocol, "KERN") == 0)
    {
      timeout = serialReq("0x77", true);
    }
    else if (strcmp(config.scale_protocol, "KERN-ABT") == 0)
    {
      timeout = serialReq("0x44 0x30 0x35 0x0D 0x0A", true);
    }
    else if (strcmp(config.scale_protocol, "SBI") == 0)
    {
      timeout = serialReq("0x50 0x0D 0x0A", true);
    }
    else if (strcmp(config.scale_protocol, "CUSTOM") == 0)
    {
      timeout = serialReq(config.scale_customCode, true);
    }
    else
    {
      timeout = serialWait();
    }

    if (timeout)
    {
      updateDisplayLog(langText("status_timeout"), true);
      weight = -1.0;
      delay(3000);
      newData = false;
    }
  }    
  setWeightLabel(ui_LabelTricklerWeight);
}
