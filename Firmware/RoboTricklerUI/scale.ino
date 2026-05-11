bool waitForScaleResponseTimedOut()
{
    for (int i = 0; i < SCALE_REQUEST_TIMEOUT_MS; i++)
    {
        if (Serial1.available())
        {
            return false;
        }
        delay(1);
    }
    return true;
}

void parseWeightString(const char *input, float *value, int *decimalPlaces)
{
    char filteredBuffer[SCALE_LINE_BUFFER_SIZE];
    int j = 0;
    bool dotFound = false;
    int decimals = 0;

    for (int i = 0; input[i] != '\0'; i++)
    {
        if ((input[i] >= '0' && input[i] <= '9') || input[i] == '.' || input[i] == ',')
        {
            if (input[i] == '.' || input[i] == ',')
            {
                if (dotFound)
                {
                    break;
                }
                dotFound = true;
                if (j < (int)sizeof(filteredBuffer) - 1)
                {
                    filteredBuffer[j++] = '.';
                }
                continue;
            }
            if (dotFound)
            {
                decimals++;
            }
            if (j < (int)sizeof(filteredBuffer) - 1)
            {
                filteredBuffer[j++] = input[i];
            }
        }
    }
    filteredBuffer[j] = '\0';

    *decimalPlaces = decimals;
    *value = atof(filteredBuffer);
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

void serialFlush()
{
    Serial1.flush();
    while (Serial1.available())
    {
        Serial1.read();
    }
}

bool sendScaleRequest(const char *req, bool flush)
{
    char request[SCALE_REQUEST_BUFFER_SIZE];
    strlcpy(request, req, sizeof(request));

    uint8_t bytes[SCALE_REQUEST_MAX_BYTES];
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
    Serial1.write(bytes, byteCount);

    return waitForScaleResponseTimedOut();
}

struct ScaleProtocolRequest
{
    const char *protocol;
    const char *request;
    bool flush;
};

static const ScaleProtocolRequest scaleProtocolRequests[] = {
    {"GG", "0x1B 0x70 0x0D 0x0A", false},
    {"AD", "0x51 0x0D 0x0A", true},
    {"KERN", "0x77", true},
    {"KERN-ABT", "0x44 0x30 0x35 0x0D 0x0A", true},
    {"SBI", "0x50 0x0D 0x0A", true},
};

static bool requestScaleWeight()
{
    for (size_t i = 0; i < ARRAY_COUNT(scaleProtocolRequests); i++)
    {
        if (strcmp(config.scale_protocol, scaleProtocolRequests[i].protocol) == 0)
        {
            return sendScaleRequest(scaleProtocolRequests[i].request, scaleProtocolRequests[i].flush);
        }
    }

    if (strcmp(config.scale_protocol, "CUSTOM") == 0)
    {
        return sendScaleRequest(config.scale_customCode, true);
    }

    return waitForScaleResponseTimedOut();
}

static bool readScaleLine(char *buff, size_t buffSize)
{
    if (!Serial1.available())
    {
        return false;
    }

    size_t bytesRead = Serial1.readBytesUntil(0x0A, buff, buffSize - 1);
    buff[bytesRead] = '\0';
    DEBUG_PRINTLN(buff);
    return true;
}

static bool isScaleWeightLine(const char *buff)
{
    return (strchr(buff, '.') != NULL) || (strchr(buff, ',') != NULL);
}

static void updateScaleUnit(const char *buff)
{
    if (containsIgnoreCase(buff, "g"))
    {
        unit = (containsIgnoreCase(buff, "gn") || containsIgnoreCase(buff, "gr")) ? " gn" : " g";
    }
}

static void parseScaleWeightLine(const char *buff)
{
    weight = 0;
    decimalPlaces = 0;
    parseWeightString(buff, &weight, &decimalPlaces);

    if (strchr(buff, '-') != NULL)
    {
        weight *= -1.0;
    }

    updateScaleUnit(buff);
}

static void logScaleWeight()
{
    DEBUG_PRINT("Weight: ");
    DEBUG_PRINTLN(weight);
    lastScaleWeightReadTime = millis();
    DEBUG_PRINT("decimalPlaces: ");
    DEBUG_PRINTLN(decimalPlaces);

    DEBUG_PRINT("Weight Counter: ");
    DEBUG_PRINTLN(weightCounter);
}

static void updateStableWeightState()
{
    long weightRounded = lround(weight * SCALE_WEIGHT_STABILITY_FACTOR);
    long lastWeightRounded = lround(lastWeight * SCALE_WEIGHT_STABILITY_FACTOR);
    if (weightRounded != lastWeightRounded)
    {
        weightCounter = 0;
        updateWeightLabel(ui_LabelTricklerWeight);
        lastWeight = weight;
        return;
    }

    if (running || calibrationProfilePromptPending)
    {
        if (weightCounter >= measurementCount)
        {
            newData = true;
            weightCounter = 0;
        }
        weightCounter++;
    }
    lastWeight = weight;
}

void readScaleWeight()
{
    char buff[SCALE_LINE_BUFFER_SIZE];
    if (!readScaleLine(buff, sizeof(buff)))
    {
        if (requestScaleWeight())
        {
            updateDisplayLog(languageText("status_timeout"), true);
            delay(SCALE_TIMEOUT_DISPLAY_DELAY_MS);
            newData = false;
        }
        return;
    }

    if (!isScaleWeightLine(buff))
    {
        return;
    }

    parseScaleWeightLine(buff);
    logScaleWeight();
    updateStableWeightState();
}
