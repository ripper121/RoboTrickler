const unsigned long SCALE_SERIAL_WAIT_MS = 2000;
const size_t SCALE_SERIAL_MAX_REQUEST_BYTES = 64;
const size_t SCALE_SERIAL_REQUEST_TEXT_LEN = 128;
const size_t SCALE_SERIAL_LINE_LEN = 64;

struct ScaleRequestCommand
{
  const char *protocol;
  const char *request;
};

const ScaleRequestCommand SCALE_REQUEST_COMMANDS[] = {
    {"GG", "0x1B 0x70 0x0D 0x0A"},
    {"SBI", "0x50 0x0D 0x0A"},
    {"KERN", "0x77"},
    {"KERN-ABT", "0x44 0x30 0x35 0x0D 0x0A"},
    {"KERN-ABS", "0x44 0x30 0x31 0x0D 0x0A"},
    {"AD", "0x53 0x49 0x0D 0x0A"},
    {"CUSTOM", NULL},
    {"", NULL}};

const size_t SCALE_PROTOCOL_COUNT = sizeof(SCALE_REQUEST_COMMANDS) / sizeof(SCALE_REQUEST_COMMANDS[0]);

const char *scaleProtocolDisplayName(const char *protocol)
{
  return ((protocol == NULL) || (protocol[0] == '\0')) ? "STREAM" : protocol;
}

const char *nextScaleProtocol(const char *currentProtocol)
{
  for (size_t i = 0; i < SCALE_PROTOCOL_COUNT; i++)
  {
    if (strcmp(currentProtocol, SCALE_REQUEST_COMMANDS[i].protocol) == 0)
    {
      return SCALE_REQUEST_COMMANDS[(i + 1) % SCALE_PROTOCOL_COUNT].protocol;
    }
  }
  return SCALE_REQUEST_COMMANDS[0].protocol;
}

bool serialWait()
{
  unsigned long start = millis();
  while ((millis() - start) < SCALE_SERIAL_WAIT_MS)
  {
    if (Serial1.available())
    {
      return false;
    }
    delay(1);
  }
  return true;
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

bool serialReq(const char *req)
{

  // Requests are stored as human-readable hex bytes in the profile/config files.
  char request[SCALE_SERIAL_REQUEST_TEXT_LEN];
  strlcpy(request, req, sizeof(request));

  uint8_t bytes[SCALE_SERIAL_MAX_REQUEST_BYTES];
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

  #if DEBUG
  if (strcmp(config.scaleProtocol, "CUSTOM") == 0)
  {
    updateDisplayLog("TX:" + serialBytesToDisplay(bytes, byteCount) + " / " + serialBytesToHex(bytes, byteCount), false);
  }
  #endif

  Serial1.write(bytes, byteCount);

  return serialWait();
}

void initRs232()
{
  Serial1.begin(config.scaleBaud, SERIAL_8N1, SCALE_RX_PIN, SCALE_TX_PIN);
}

bool isWeightNumberChar(char c)
{
  return ((c >= '0') && (c <= '9')) || (c == '.') || (c == '+') || (c == '-');
}

bool isWeightSpace(char c)
{
  return (c == ' ') || (c == '\t');
}

bool parseWeightToken(const char *token, int tokenLen, float *value, int *decimalPlaces, bool *hasDecimal)
{
  char number[24];
  int pos = 0;
  int decimals = 0;
  bool dotFound = false;
  bool digitFound = false;

  if ((token == NULL) || (tokenLen <= 0) || (tokenLen >= (int)sizeof(number)))
  {
    return false;
  }

  for (int i = 0; i < tokenLen; i++)
  {
    char c = token[i];
    if ((c == '+') || (c == '-'))
    {
      if (i != 0)
      {
        return false;
      }
    }
    else if (c == '.')
    {
      if (dotFound)
      {
        return false;
      }
      dotFound = true;
    }
    else if ((c >= '0') && (c <= '9'))
    {
      digitFound = true;
      if (dotFound)
      {
        decimals++;
      }
    }
    else
    {
      return false;
    }

    number[pos++] = c;
  }

  if (!digitFound)
  {
    return false;
  }

  number[pos] = '\0';
  char *end = NULL;
  float parsed = strtof(number, &end);
  if ((end == number) || (*end != '\0'))
  {
    return false;
  }

  *value = parsed;
  *decimalPlaces = decimals;
  *hasDecimal = dotFound;
  return true;
}

// Parse one complete numeric token from a scale line. Prefer a decimal token and
// reject ambiguous lines instead of merging unrelated digits into a fake weight.
bool stringToWeight(const char *input, float *value, int *decimalPlaces)
{
  float parsedValue = -1.0;
  int parsedDecimals = 0;
  int decimalTokenCount = 0;
  int integerTokenCount = 0;

  *value = -1.0;
  *decimalPlaces = 0;

  if (input == NULL)
  {
    return false;
  }

  for (int i = 0; input[i] != '\0';)
  {
    while ((input[i] != '\0') && !isWeightNumberChar(input[i]))
    {
      i++;
    }

    int tokenStart = i;
    while ((input[i] != '\0') && isWeightNumberChar(input[i]))
    {
      i++;
    }

    int tokenLen = i - tokenStart;
    float tokenValue = 0.0;
    int tokenDecimals = 0;
    bool tokenHasDecimal = false;
    if (parseWeightToken(input + tokenStart, tokenLen, &tokenValue, &tokenDecimals, &tokenHasDecimal))
    {
      if ((input[tokenStart] != '+') && (input[tokenStart] != '-'))
      {
        int signScan = tokenStart - 1;
        while ((signScan >= 0) && isWeightSpace(input[signScan]))
        {
          signScan--;
        }

        if (((signScan == 0) || ((signScan > 0) && !isWeightNumberChar(input[signScan - 1]))) && (input[signScan] == '-'))
        {
          tokenValue *= -1.0;
        }
      }

      if (tokenHasDecimal)
      {
        decimalTokenCount++;
        parsedValue = tokenValue;
        parsedDecimals = tokenDecimals;
      }
      else
      {
        integerTokenCount++;
        if (decimalTokenCount == 0)
        {
          parsedValue = tokenValue;
          parsedDecimals = 0;
        }
      }
    }
  }

  if ((decimalTokenCount == 1) || ((decimalTokenCount == 0) && (integerTokenCount == 1)))
  {
    *value = parsedValue;
    *decimalPlaces = parsedDecimals;
    return true;
  }

  return false;
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

bool requestScaleWeight()
{
  updateActiveProfileStepCounterDisplay(weightCounter);

  if (strcmp(config.scaleProtocol, "CUSTOM") == 0)
  {
    return serialReq(config.scaleCustomCode);
  }

  for (size_t i = 0; i < SCALE_PROTOCOL_COUNT; i++)
  {
    if ((SCALE_REQUEST_COMMANDS[i].request != NULL) &&
        (strcmp(config.scaleProtocol, SCALE_REQUEST_COMMANDS[i].protocol) == 0))
    {
      return serialReq(SCALE_REQUEST_COMMANDS[i].request);
    }
  }

  return serialWait();
}

bool readScaleLine(float *parsedWeight, int *parsedDecimalPlaces, String *parsedUnit)
{
  if (!Serial1.available())
  {
    return false;
  }

  char buff[SCALE_SERIAL_LINE_LEN];
  size_t bytesRead = Serial1.readBytesUntil(0x0A, buff, sizeof(buff) - 1);
  buff[bytesRead] = '\0';

  #if DEBUG
  if (strcmp(config.scaleProtocol, "CUSTOM") == 0)
  {
    updateDisplayLog("RX:" + String(buff), false);
  }
  #endif

  DEBUG_PRINTLN(buff);

  float candidateWeight = -1.0;
  int candidateDecimalPlaces = 0;
  if (!stringToWeight(buff, &candidateWeight, &candidateDecimalPlaces))
  {
    return false;
  }

  *parsedWeight = candidateWeight;
  *parsedDecimalPlaces = candidateDecimalPlaces;
  *parsedUnit = "";
  if (containsIgnoreCase(buff, "g"))
  {
    *parsedUnit = " g";
    if (containsIgnoreCase(buff, "gn") || containsIgnoreCase(buff, "gr"))
    {
      *parsedUnit = " gn";
    }
  }

  return true;
}

void readWeight()
{
  // Require repeated identical readings while trickling, but stay responsive
  // during idle display refreshes.
  int stableTarget = measurementCount;
  if ((stableTarget <= 0) || (!isTricklerRunning() && !isCalibrationProfilePromptPending()))
  {
    stableTarget = 1;
  }

  int maxAttempts = stableTarget * 4;
  if (maxAttempts < 4)
  {
    maxAttempts = 4;
  }

  bool timeout = false;
  bool gotValidWeight = false;
  int stableCount = 0;
  long previousRoundedWeight = 0;
  float stableWeight = -1.0;
  int stableDecimalPlaces = DECIMAL_PLACES;
  String stableUnit = "";

  serialFlush();

  for (int attempt = 0; (stableCount < stableTarget) && (attempt < maxAttempts); attempt++)
  {
    timeout = requestScaleWeight();
    if (timeout)
    {
      break;
    }

    float candidateWeight = -1.0;
    int candidateDecimalPlaces = DECIMAL_PLACES;
    String candidateUnit = "";
    if (!readScaleLine(&candidateWeight, &candidateDecimalPlaces, &candidateUnit))
    {
      continue;
    }

    long candidateRoundedWeight = lroundf(candidateWeight * WEIGHT_SCALE_FACTOR);
    if (gotValidWeight && (candidateRoundedWeight == previousRoundedWeight))
    {
      stableCount++;
    }
    else
    {
      stableCount = 1;
    }

    gotValidWeight = true;
    previousRoundedWeight = candidateRoundedWeight;
    stableWeight = candidateWeight;
    stableDecimalPlaces = candidateDecimalPlaces;
    stableUnit = candidateUnit;

    DEBUG_PRINT("Weight Counter: ");
    DEBUG_PRINTLN(stableCount);
    weightCounter = stableCount;
  }

  updateActiveProfileStepCounterDisplay(weightCounter);

  static bool timeoutLogged = false;

  if (timeout || (stableCount < stableTarget))
  {
    if (timeout)
    {
      // Keep the transient status line updated every cycle, but push only one
      // entry into the scrolling log when the timeout first occurs so it does
      // not flood the log while the scale stays unreachable.
      updateDisplayLog(langText("status_timeout"), true);
      delay(200);
      if (!timeoutLogged)
      {
        updateDisplayLog(langText("status_timeout"));
        timeoutLogged = true;
      }
    }
    weight = NAN;
    newWeightData = false;
  }
  else
  {
    timeoutLogged = false;
    weight = stableWeight;
    decimalPlaces = (stableDecimalPlaces > 0) ? stableDecimalPlaces : DECIMAL_PLACES;
    weightUnit = stableUnit;
    lastWeight = weight;
    lastScaleWeightReadTime = millis();
    weightCounter = stableCount;
    newWeightData = isTricklerRunning() || isCalibrationProfilePromptPending();

    DEBUG_PRINT("Weight: ");
    DEBUG_PRINTLN(weight);
    DEBUG_PRINT("Decimal places: ");
    DEBUG_PRINTLN(decimalPlaces);
  }

  setWeightLabel(ui_LabelTricklerWeight);

}
