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

void parseWeightString(const char *input, float *value, int *decimalPlaces)
{
    char filteredBuffer[64];
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

bool sendScaleRequest(String req, bool flush)
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
    Serial1.write(bytes, byteCount);

    return serialWait();
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

    return serialWait();
}

void readScaleWeight()
{
    if (Serial1.available())
    {
        char buff[64];
        size_t bytesRead = Serial1.readBytesUntil(0x0A, buff, sizeof(buff) - 1);
        buff[bytesRead] = '\0';

        DEBUG_PRINTLN(buff);

        if ((strchr(buff, '.') != NULL) || (strchr(buff, ',') != NULL))
        {
            weight = 0;
            decimalPlaces = 0;
            parseWeightString(buff, &weight, &decimalPlaces);

            if (strchr(buff, '-') != NULL)
            {
                weight = weight * (-1.0);
            }

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
            DEBUG_PRINT("decimalPlaces: ");
            DEBUG_PRINTLN(decimalPlaces);

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
                updateWeightLabel(ui_LabelTricklerWeight);
            }
            lastWeight = weight;
        }
    }
    else
    {
        bool timeout = requestScaleWeight();
        if (timeout)
        {
            updateDisplayLog(languageText("status_timeout"), true);
            delay(500);
            newData = false;
        }
    }
}
