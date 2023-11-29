const char *host = "robo-trickler";

File uploadFile;

void returnOK()
{
  server.send(200, "text/plain", "");
}

void returnFail(String msg)
{
  server.send(500, "text/plain", msg + "\r\n");
}

bool loadFromSdCard(String path)
{
  String dataType = "text/plain";
  if (path.endsWith("/"))
  {
    path += "index.html";
  }

  if (path.endsWith(".src"))
  {
    path = path.substring(0, path.lastIndexOf("."));
  }
  else if (path.endsWith(".html") || path.endsWith(".htm"))
  {
    dataType = "text/html";
  }
  else if (path.endsWith(".css"))
  {
    dataType = "text/css";
  }
  else if (path.endsWith(".js"))
  {
    dataType = "application/javascript";
  }
  else if (path.endsWith(".png"))
  {
    dataType = "image/png";
  }
  else if (path.endsWith(".gif"))
  {
    dataType = "image/gif";
  }
  else if (path.endsWith(".jpg"))
  {
    dataType = "image/jpeg";
  }
  else if (path.endsWith(".ico"))
  {
    dataType = "image/x-icon";
  }
  else if (path.endsWith(".xml"))
  {
    dataType = "text/xml";
  }
  else if (path.endsWith(".pdf"))
  {
    dataType = "application/pdf";
  }
  else if (path.endsWith(".zip"))
  {
    dataType = "application/zip";
  }
  else if (path.endsWith(".gz"))
  {
    dataType = "application/x-gzip";
    server.sendHeader("Content-Encoding", "gzip");
  }
  else
  {
    dataType = "text/plain";
  }

  if (server.hasArg("download"))
  {
    dataType = "application/octet-stream";
  }

  File dataFile = SD.open(path.c_str());
  if (dataFile.isDirectory())
  {
    path += "/index.html";
    dataType = "text/html";
  }
  dataFile.close();

  String pathWithGz = path + ".gz";
  if (SD.exists(pathWithGz) || SD.exists(path))
  {                            // If the file exists, either as a compressed archive, or normal
    if (SD.exists(pathWithGz)) // If there's a compressed version available{
      path += ".gz";           // Use the compressed version
    dataFile = SD.open(path.c_str());
    if (dataFile.isDirectory())
    {
      path += "/index.html";
      if (SD.exists(pathWithGz)) // If there's a compressed version available{
        path += ".gz";           // Use the compressed version
      dataType = "text/html";
    }

    if (!dataFile)
    {
      return false;
    }

    DEBUG_PRINTLN(String("\tSent file: ") + path);
    if (server.streamFile(dataFile, dataType) != dataFile.size())
    {
      DEBUG_PRINTLN("Sent less data than expected!");
    }
    dataFile.close();
  }
  return true;
}

void handleFileUpload()
{
  if (server.uri() != "/resources/edit")
  {
    return;
  }
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START)
  {
    if (SD.exists((char *)upload.filename.c_str()))
    {
      SD.remove((char *)upload.filename.c_str());
    }
    uploadFile = SD.open(upload.filename.c_str(), FILE_WRITE);
    DEBUG_PRINT("Upload: START, filename: ");
    DEBUG_PRINTLN(upload.filename);
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    if (uploadFile)
    {
      uploadFile.write(upload.buf, upload.currentSize);
    }
    DEBUG_PRINT("Upload: WRITE, Bytes: ");
    DEBUG_PRINTLN(upload.currentSize);
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (uploadFile)
    {
      uploadFile.close();
    }
    DEBUG_PRINT("Upload: END, Size: ");
    DEBUG_PRINTLN(upload.totalSize);
    if ((String(upload.filename).indexOf(".txt")) != -1 && (String(upload.filename).indexOf("conf") == -1))
    {
      readPowder(upload.filename.c_str(), config);
    }
  }
}

void deleteRecursive(String path)
{
  File file = SD.open((char *)path.c_str());
  if (!file.isDirectory())
  {
    file.close();
    SD.remove((char *)path.c_str());
    return;
  }

  file.rewindDirectory();
  while (true)
  {
    File entry = file.openNextFile();
    if (!entry)
    {
      break;
    }
    String entryPath = path + "/" + entry.name();
    if (entry.isDirectory())
    {
      entry.close();
      deleteRecursive(entryPath);
    }
    else
    {
      entry.close();
      SD.remove((char *)entryPath.c_str());
    }
    yield();
  }

  SD.rmdir((char *)path.c_str());
  file.close();
}

void handleDelete()
{
  if (server.args() == 0)
  {
    return returnFail("BAD ARGS");
  }
  String path = server.arg(0);
  if (path == "/" || !SD.exists((char *)path.c_str()))
  {
    returnFail("BAD PATH");
    return;
  }
  deleteRecursive(path);
  returnOK();
}

void handleCreate()
{
  if (server.args() == 0)
  {
    return returnFail("BAD ARGS");
  }
  String path = server.arg(0);
  if (path == "/" || SD.exists((char *)path.c_str()))
  {
    returnFail("BAD PATH");
    return;
  }

  if (path.indexOf('.') > 0)
  {
    File file = SD.open((char *)path.c_str(), FILE_WRITE);
    if (file)
    {
      file.write(0);
      file.close();
    }
  }
  else
  {
    SD.mkdir((char *)path.c_str());
  }
  returnOK();
}

void printDirectory()
{
  if (!server.hasArg("dir"))
  {
    return returnFail("BAD ARGS");
  }
  String path = server.arg("dir");
  if (path != "/" && !SD.exists((char *)path.c_str()))
  {
    return returnFail("BAD PATH");
  }
  File dir = SD.open((char *)path.c_str());
  path = String();
  if (!dir.isDirectory())
  {
    dir.close();
    return returnFail("NOT DIR");
  }
  dir.rewindDirectory();
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/json", "");
  WiFiClient client = server.client();

  server.sendContent("[");
  for (int cnt = 0; true; ++cnt)
  {
    File entry = dir.openNextFile();
    if (!entry)
    {
      break;
    }

    if (entry.path() != "/resources/css")
    {

      String output;
      if (cnt > 0)
      {
        output = ',';
      }

      output += "{\"type\":\"";
      output += (entry.isDirectory()) ? "dir" : "file";
      output += "\",\"name\":\"";
      output += entry.path();
      output += "\"";
      output += "}";
      server.sendContent(output);
    }
    entry.close();
  }
  server.sendContent("]");
  dir.close();
}

void handleNotFound()
{
  if (loadFromSdCard(server.uri()))
  {
    return;
  }
  String message = "SDCARD Not Detected\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " NAME:" + server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  DEBUG_PRINT(message);
}

void handleAjaxRequest()
{
  String message = "test";
  server.send(200, "text/plane", message);
}

void handleReboot()
{
  server.send(200, "text/html", "<h3>Reboot now.</h3><br><button onClick='javascript:history.back()'>Back</button>");
  ESP.restart();
}

void handleSetValue()
{
  for (uint8_t i = 0; i < server.args(); i++)
  {
    if (server.argName(i) == "targetWeight")
    {
      if (server.arg(i).toFloat() > 0 && server.arg(i).toFloat() < 100)
      {
        targetWeight = server.arg(i).toFloat();
        // labelBanner.drawButton(false, String(targetWeight, 3));
        preferences.putFloat("targetWeight", targetWeight);
      }
    }
  }
  server.send(200, "text/html", "<h3>Value Set.</h3><br><button onClick='javascript:history.back()'>Back</button>");
}

IPAddress stringToIPAddress(const String &ipAddressString)
{
  IPAddress ipAddress;
  int parts[4] = {0}; // Initialize the parts array with zeros
  int partCount = 0;

  // Split the string into four parts using '.' as the delimiter
  char *token = strtok((char *)ipAddressString.c_str(), ".");
  while (token != NULL && partCount < 4)
  {
    parts[partCount] = atoi(token);
    partCount++;
    token = strtok(NULL, ".");
  }

  // Check if we successfully parsed four parts
  if (partCount == 4)
  {
    ipAddress = IPAddress(parts[0], parts[1], parts[2], parts[3]);
  }
  else
  {
    DEBUG_PRINTLN("Invalid IP Address format.");
  }

  return ipAddress;
}

void makeHttpsGetRequest(String serverPath)
{
  HTTPClient http;
  if (http.begin(serverPath))
  {
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0)
    {
      DEBUG_PRINT("HTTP Response code: ");
      DEBUG_PRINTLN(httpResponseCode);
      String payload = http.getString();
      DEBUG_PRINTLN(payload);
    }
    else
    {
      DEBUG_PRINT("Error code: ");
      DEBUG_PRINTLN(httpResponseCode);
    }
    http.end();
  }
  else
  {
    DEBUG_PRINTLN("Unable to connect");
  }
}

void initWebServer()
{
  WIFI_AKTIVE = false;
  if (String(config.wifi_ssid).length() > 0)
  {
    DEBUG_PRINT("Connect to Wifi: ");
    DEBUG_PRINTLN(config.wifi_ssid);

    updateDisplayLog("Connect to Wifi: ");
    updateDisplayLog(config.wifi_ssid);

    WiFi.disconnect();           // added to start with the wifi off, avoid crashing
    WiFi.softAPdisconnect(true); // Function will set currently configured SSID and password of the soft-AP to null values. The parameter  is optional. If set to true it will switch the soft-AP mode off.
    WiFi.mode(WIFI_OFF);         // added to start with the wifi off, avoid crashing

    delay(500);

    IPAddress ipDNS = IPAddress(8, 8, 8, 8);
    if (String(config.IPDns).length() > 0)
    {
      IPAddress ipDNS = stringToIPAddress(String(config.IPDns));
    }

    if (String(config.IPStatic).length() > 0)
    {
      IPAddress staticIP = stringToIPAddress(String(config.IPStatic)); // Example IP
      IPAddress gateway = stringToIPAddress(String(config.IPGateway)); // Gateway of your network
      IPAddress subnet = stringToIPAddress(String(config.IPSubnet));   // Subnet mask

      if (!WiFi.config(staticIP, gateway, subnet, ipDNS))
      {
        // labelInfo.drawButton(false, "Static IP Failed to configure!");
        updateDisplayLog("Static IP Failed to configure!");
        delay(3000);
      }
    }
    else
    {
      if (!WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, ipDNS))
      {
        // labelInfo.drawButton(false, "DNS Failed to configure!");
        updateDisplayLog("DNS Failed to configure!");
        delay(3000);
      }
    }

    DEBUG_PRINTLN("DNS-Server:");
    DEBUG_PRINTLN(ipDNS);
    updateDisplayLog("DNS-Server:");
    updateDisplayLog(String(ipDNS));

    WiFi.mode(WIFI_STA);
    WiFi.begin(config.wifi_ssid, config.wifi_psk);

    if (WiFi.waitForConnectResult() == WL_CONNECTED)
    {
      DEBUG_PRINTLN("Wifi Connected");
      updateDisplayLog("Wifi Connected");
      MDNS.begin(host);

      server.on("/list", HTTP_GET, printDirectory);
      server.on("/resources/edit", HTTP_DELETE, handleDelete);
      server.on("/resources/edit", HTTP_PUT, handleCreate);
      server.on(
          "/resources/edit", HTTP_POST, []()
          { returnOK(); },
          handleFileUpload);
      // server.on("/ajaxRequest", handleAjaxRequest);//To get update of ADC Value only
      server.onNotFound(handleNotFound);
      server.on("/generate_204", handleNotFound);
      server.on("/favicon.ico", handleNotFound);
      server.on("/fwlink", handleNotFound);
      server.on("/reboot", handleReboot);
      server.on("/setValue", handleSetValue);

      server.begin();
      MDNS.addService("http", "tcp", 80);

      DEBUG_PRINT("Ready! Open http://");
      DEBUG_PRINT(host);
      DEBUG_PRINTLN(".local in your browser");

      updateDisplayLog(String(String("Ready! Open http://") + host + String(".local in your browser")));

      String serverPath = "https://ripper121.com/roboTrickler/userTracker.php?mac=" + String(WiFi.macAddress());
      makeHttpsGetRequest(serverPath);

      WIFI_AKTIVE = true;
    }
    else
    {
      DEBUG_PRINTLN("No Wifi");
      updateDisplayLog("No Wifi");
    }
  }
  else
  {
    DEBUG_PRINTLN("No Wifi");
    updateDisplayLog("No Wifi");
  }
}
