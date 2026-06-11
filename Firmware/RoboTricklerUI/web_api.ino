void handleReboot()
{
  server.send(200, "text/html", "<h3>Reboot now.</h3><br><button onClick='javascript:history.back()'>Back</button>");
  ESP.restart();
}

void handleGetWeight()
{
  server.send(200, "text/plain", String(weight, 3));
}

void handleGetTarget()
{
  server.send(200, "text/plain", String(config.targetWeight, 3));
}

void handleSetTarget()
{
  for (uint8_t i = 0; i < server.args(); i++)
  {
    if (server.argName(i) == "targetWeight")
    {
      if ((server.arg(i).toFloat() > 0) && (server.arg(i).toFloat() < MAX_TARGET_WEIGHT))
      {
        if (config.targetWeight != server.arg(i).toFloat())
        {
          saveTargetWeight(server.arg(i).toFloat());
        }
      }
    }
  }
  server.send(200, "text/html", "<h3>Value Set.</h3><br><button onClick='javascript:history.back()'>Back</button>");
}

void handleSetProfile()
{
  for (uint8_t i = 0; i < server.args(); i++)
  {
    if (server.argName(i) == "profileNumber")
    {
      if (server.arg(i).toFloat() >= 0)
      {
        setProfile(server.arg(i).toInt());
      }
    }
  }
  server.send(200, "text/html", "<h3>Value Set.</h3><br><button onClick='javascript:history.back()'>Back</button>");
}

void handleGetProfile()
{
  server.send(200, "text/plain", String(config.profile));
}

void handleGetLanguage()
{
  server.send(200, "text/plain", String(config.language));
}

void handleGetProfileList()
{
  String message = "{";
  for (int i = 0; i < profileListCount; i++)
  {
    message += "\"";
    message += String(i);
    message += "\":\"";
    message += jsonEscape(String(profileListBuff[i]));
    message += "\"";
    if (i < profileListCount - 1)
      message += ",";
  }
  message += "}";

  server.send(200, "text/json", message);
}

void handleStart()
{
  startTrickler();
  server.send(200, "text/html", "<h3>Running...</h3><br><button onClick='javascript:history.back()'>Back</button>");
}
void handleStop()
{
  stopTrickler();
  server.send(200, "text/html", "<h3>Stopped...</h3><br><button onClick='javascript:history.back()'>Back</button>");
}

