
const char* host = "robo-trickler";

const char* updatePage = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form><br><button onClick='javascript:history.back()'>Back</button>";

const char* test_root_ca = \
                           "-----BEGIN CERTIFICATE-----\n" \
                           "MIIE+TCCA+GgAwIBAgISA0G2hx1qiBcP3XSb0jtdihnrMA0GCSqGSIb3DQEBCwUA\n" \
                           "MDIxCzAJBgNVBAYTAlVTMRYwFAYDVQQKEw1MZXQncyBFbmNyeXB0MQswCQYDVQQD\n" \
                           "EwJSMzAeFw0yMzEwMDQwNjAzMTNaFw0yNDAxMDIwNjAzMTJaMBgxFjAUBgNVBAMT\n" \
                           "DXJpcHBlcjEyMS5jb20wggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCi\n" \
                           "6iqg/7unQacYeidkKJYGs39X/bazIBj7tw6RijuCg2ZuQ7OSrXLQ/TqBRqn045Qb\n" \
                           "Gru3x5AaXJlwGnX5QENf3RbYmfuVCkTvhN7UNpOwMDJkKwGBaEt47ur1OqWpAAD7\n" \
                           "SriZzyHFQQFHX/10lZ5JpkL0tk1ZVpyY8KDPSy1ub3fuSsZx6SNBtHXkgCkRK1W3\n" \
                           "5ilPnlFMYTxyJOl17RShjt116wNMsKSFFSrmSaGYpa96lAJ+nhrqB8xS6p1qEr09\n" \
                           "oDXJyNfd2Ao2mQHo7kNPDm4oGXRcWmw8FsjM4Lca25w6mk39usTiN0yUf6A68iqO\n" \
                           "LKMfRGctJc6KYw1T5yM7AgMBAAGjggIhMIICHTAOBgNVHQ8BAf8EBAMCBaAwHQYD\n" \
                           "VR0lBBYwFAYIKwYBBQUHAwEGCCsGAQUFBwMCMAwGA1UdEwEB/wQCMAAwHQYDVR0O\n" \
                           "BBYEFCifNw2ZhI5BRgEZhml9yltktQzvMB8GA1UdIwQYMBaAFBQusxe3WFbLrlAJ\n" \
                           "QOYfr52LFMLGMFUGCCsGAQUFBwEBBEkwRzAhBggrBgEFBQcwAYYVaHR0cDovL3Iz\n" \
                           "Lm8ubGVuY3Iub3JnMCIGCCsGAQUFBzAChhZodHRwOi8vcjMuaS5sZW5jci5vcmcv\n" \
                           "MCsGA1UdEQQkMCKCDXJpcHBlcjEyMS5jb22CEXd3dy5yaXBwZXIxMjEuY29tMBMG\n" \
                           "A1UdIAQMMAowCAYGZ4EMAQIBMIIBAwYKKwYBBAHWeQIEAgSB9ASB8QDvAHYA2ra/\n" \
                           "az+1tiKfm8K7XGvocJFxbLtRhIU0vaQ9MEjX+6sAAAGK+X9xjAAABAMARzBFAiAo\n" \
                           "PW6y35BrCxW610xgREHrkqZeiCjv0iqbdH8pXocWQQIhALL4cMGRS+dvwZRo3gKW\n" \
                           "nz3FPyBIorVEJWIK36mJJmuyAHUAO1N3dT4tuYBOizBbBv5AO2fYT8P0x70ADS1y\n" \
                           "b+H61BcAAAGK+X9zbwAABAMARjBEAiAok9lpGGSDED0nyiul2vWdZaXkHlaX5ggF\n" \
                           "o9x2IieSngIgDZWfbjW/xJQ8drK//jE53Y7rwwDfk1KkTNqNrcAMpq4wDQYJKoZI\n" \
                           "hvcNAQELBQADggEBAIrqmx2tkMYkrjPGTNnUSGkO3Nyvt25zQFzA1J+3w9rD1vL0\n" \
                           "Ld9lBayDcljkV45X4o6TJqqZekJIiQ00PfZWaeWY2cDHWrKN9wQVGw6DDWEseWO2\n" \
                           "7wXbSE88DnVO/t99U/XDMNosEekxn9EsBCMfCtReJ7XvH3ZG7po5zxAOoLYI6UKK\n" \
                           "5DRkkUereGPUhnzKQwm+qcNXlQ7HnwvQApknACrKrghxW290g8hknqYMRbPY3BQt\n" \
                           "KfztBtWRkd7f0XJvU/tNqCXxh9CERCfsHsy/BvevCBZDf4iYfuB5utEsXMu5aMX5\n" \
                           "zXr6aW5VnR/jkoo1iiFYKEc/ZU1VHpcecqpV+BA=\n" \
                           "-----END CERTIFICATE-----\n";

File uploadFile;

void returnOK() {
  server.send(200, "text/plain", "");
}

void returnFail(String msg) {
  server.send(500, "text/plain", msg + "\r\n");
}

bool loadFromSdCard(String path) {
  String dataType = "text/plain";
  if (path.endsWith("/")) {
    path += "index.html";
  }

  if (path.endsWith(".src")) {
    path = path.substring(0, path.lastIndexOf("."));
  } else if (path.endsWith(".html") || path.endsWith(".htm")) {
    dataType = "text/html";
  } else if (path.endsWith(".css")) {
    dataType = "text/css";
  } else if (path.endsWith(".js")) {
    dataType = "application/javascript";
  } else if (path.endsWith(".png")) {
    dataType = "image/png";
  } else if (path.endsWith(".gif")) {
    dataType = "image/gif";
  } else if (path.endsWith(".jpg")) {
    dataType = "image/jpeg";
  } else if (path.endsWith(".ico")) {
    dataType = "image/x-icon";
  } else if (path.endsWith(".xml")) {
    dataType = "text/xml";
  } else if (path.endsWith(".pdf")) {
    dataType = "application/pdf";
  } else if (path.endsWith(".zip")) {
    dataType = "application/zip";
  } else if (path.endsWith(".gz")) {
    dataType = "application/x-gzip";
    server.sendHeader("Content-Encoding", "gzip");
  } else {
    dataType = "text/plain";
  }

  if (server.hasArg("download")) {
    dataType = "application/octet-stream";
  }

  File dataFile = SD.open(path.c_str());
  if (dataFile.isDirectory()) {
    path += "/index.html";
    dataType = "text/html";
  }
  dataFile.close();

  String pathWithGz = path + ".gz";
  if (SD.exists(pathWithGz) || SD.exists(path)) { // If the file exists, either as a compressed archive, or normal
    if (SD.exists(pathWithGz))                         // If there's a compressed version available{
      path += ".gz";                                         // Use the compressed version
    dataFile = SD.open(path.c_str());
    if (dataFile.isDirectory()) {
      path += "/index.html";
      if (SD.exists(pathWithGz))                         // If there's a compressed version available{
        path += ".gz";                                         // Use the compressed version
      dataType = "text/html";
    }

    if (!dataFile) {
      return false;
    }

    Serial.println(String("\tSent file: ") + path);
    if (server.streamFile(dataFile, dataType) != dataFile.size()) {
      Serial.println("Sent less data than expected!");
    }
    dataFile.close();
  }
  return true;
}

void handleFileUpload() {
  if (server.uri() != "/resources/edit") {
    return;
  }
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    if (SD.exists((char *)upload.filename.c_str())) {
      SD.remove((char *)upload.filename.c_str());
    }
    uploadFile = SD.open(upload.filename.c_str(), FILE_WRITE);
    Serial.print("Upload: START, filename: "); Serial.println(upload.filename);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);
    }
    Serial.print("Upload: WRITE, Bytes: "); Serial.println(upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
    }
    Serial.print("Upload: END, Size: "); Serial.println(upload.totalSize);
    if (String(upload.filename).indexOf(".txt") != -1 ) {
      readPowder(upload.filename.c_str(), config);
    }
  }
}

void deleteRecursive(String path) {
  File file = SD.open((char *)path.c_str());
  if (!file.isDirectory()) {
    file.close();
    SD.remove((char *)path.c_str());
    return;
  }

  file.rewindDirectory();
  while (true) {
    File entry = file.openNextFile();
    if (!entry) {
      break;
    }
    String entryPath = path + "/" + entry.name();
    if (entry.isDirectory()) {
      entry.close();
      deleteRecursive(entryPath);
    } else {
      entry.close();
      SD.remove((char *)entryPath.c_str());
    }
    yield();
  }

  SD.rmdir((char *)path.c_str());
  file.close();
}

void handleDelete() {
  if (server.args() == 0) {
    return returnFail("BAD ARGS");
  }
  String path = server.arg(0);
  if (path == "/" || !SD.exists((char *)path.c_str())) {
    returnFail("BAD PATH");
    return;
  }
  deleteRecursive(path);
  returnOK();
}

void handleCreate() {
  if (server.args() == 0) {
    return returnFail("BAD ARGS");
  }
  String path = server.arg(0);
  if (path == "/" || SD.exists((char *)path.c_str())) {
    returnFail("BAD PATH");
    return;
  }

  if (path.indexOf('.') > 0) {
    File file = SD.open((char *)path.c_str(), FILE_WRITE);
    if (file) {
      file.write(0);
      file.close();
    }
  } else {
    SD.mkdir((char *)path.c_str());
  }
  returnOK();
}

void printDirectory() {
  if (!server.hasArg("dir")) {
    return returnFail("BAD ARGS");
  }
  String path = server.arg("dir");
  if (path != "/" && !SD.exists((char *)path.c_str())) {
    return returnFail("BAD PATH");
  }
  File dir = SD.open((char *)path.c_str());
  path = String();
  if (!dir.isDirectory()) {
    dir.close();
    return returnFail("NOT DIR");
  }
  dir.rewindDirectory();
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/json", "");
  WiFiClient client = server.client();

  server.sendContent("[");
  for (int cnt = 0; true; ++cnt) {
    File entry = dir.openNextFile();
    if (!entry) {
      break;
    }

    if (entry.path() != "/resources/css") {

      String output;
      if (cnt > 0) {
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

void handleNotFound() {
  if (loadFromSdCard(server.uri())) {
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
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " NAME:" + server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  Serial.print(message);
}

void handleAjaxRequest() {
  String message = "test";
  server.send(200, "text/plane", message);
}

void handleReboot() {
  server.send(200, "text/html", "<h3>Reboot now.</h3><br><button onClick='javascript:history.back()'>Back</button>");
  ESP.restart();
}

void handleSetValue() {
  for (uint8_t i = 0; i < server.args(); i++) {
    if (server.argName(i) == "targetWeight") {
      if (server.arg(i).toFloat() > 0 && server.arg(i).toFloat() < 100) {
        targetWeight = server.arg(i).toFloat();
        labelTarget.drawButton(false, String(targetWeight, 3));
        preferences.putFloat("targetWeight", targetWeight);
      }
    }
  }
  server.send(200, "text/html", "<h3>Value Set.</h3><br><button onClick='javascript:history.back()'>Back</button>");
}

IPAddress stringToIPAddress(const String& ipAddressString) {
  IPAddress ipAddress;
  int parts[4] = {0}; // Initialize the parts array with zeros
  int partCount = 0;

  // Split the string into four parts using '.' as the delimiter
  char* token = strtok((char*)ipAddressString.c_str(), ".");
  while (token != NULL && partCount < 4) {
    parts[partCount] = atoi(token);
    partCount++;
    token = strtok(NULL, ".");
  }

  // Check if we successfully parsed four parts
  if (partCount == 4) {
    ipAddress = IPAddress(parts[0], parts[1], parts[2], parts[3]);
  } else {
    Serial.println("Invalid IP Address format.");
  }

  return ipAddress;
}

void makeHttpsGetRequest(String serverPath) {
  HTTPClient http;
  if (http.begin(serverPath)) {
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      String payload = http.getString();
      Serial.println(payload);
    }
    else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  }
  else {
    Serial.println("Unable to connect");
  }
}

void initWebServer() {
  WIFI_AKTIVE = false;
  if (String(config.wifi_ssid).length() > 0) {
    Serial.print("Connect to Wifi: ");
    Serial.println(config.wifi_ssid);

    WiFi.disconnect();   //added to start with the wifi off, avoid crashing
    WiFi.softAPdisconnect(true); // Function will set currently configured SSID and password of the soft-AP to null values. The parameter  is optional. If set to true it will switch the soft-AP mode off.
    WiFi.mode(WIFI_OFF); //added to start with the wifi off, avoid crashing

    delay(500);

    if (String(config.IPStatic).length() > 0) {
      IPAddress staticIP = stringToIPAddress(String(config.IPStatic)); // Example IP
      IPAddress gateway = stringToIPAddress(String(config.IPGateway));   // Gateway of your network
      IPAddress subnet = stringToIPAddress(String(config.IPSubnet));  // Subnet mask

      if (!WiFi.config(staticIP, gateway, subnet, IPAddress(8, 8, 8, 8))) {
        labelInfo.drawButton(false, "STA Failed to configure!");
        delay(3000);
      }
    } else {
      WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, IPAddress(8, 8, 8, 8));
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(config.wifi_ssid, config.wifi_psk);

    if (WiFi.waitForConnectResult() == WL_CONNECTED) {
      Serial.println("Wifi Connected");
      MDNS.begin(host);

      server.on("/list", HTTP_GET, printDirectory);
      server.on("/resources/edit", HTTP_DELETE, handleDelete);
      server.on("/resources/edit", HTTP_PUT, handleCreate);
      server.on("/resources/edit", HTTP_POST, []() {
        returnOK();
      }, handleFileUpload);
      //server.on("/ajaxRequest", handleAjaxRequest);//To get update of ADC Value only
      server.onNotFound(handleNotFound);
      server.on("/generate_204", handleNotFound);
      server.on("/favicon.ico", handleNotFound);
      server.on("/fwlink", handleNotFound);
      server.on("/reboot", handleReboot);
      server.on("/setValue", handleSetValue);

      server.on("/fwupdate", HTTP_GET, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/html", updatePage);
      });
      server.on("/update", HTTP_POST, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/html", (Update.hasError()) ? "<h3>FAIL</h3><br><br><input type='button' value='Back' onClick='javascript:history.back()'>" : "<h3>OK</h3><br><br><input type='button' value='Back' onClick='javascript:history.back()'>");
        ESP.restart();
      }, []() {
        HTTPUpload& upload = server.upload();
        if (upload.status == UPLOAD_FILE_START) {
          Serial.setDebugOutput(true);
          Serial.printf("Update: %s\n", upload.filename.c_str());
          String infoText = "Update: " + String(upload.filename);
          labelInfo.drawButton(false, infoText);
          OTA_running = true;
          if (!Update.begin()) { //start with max available size
            Update.printError(Serial);
          }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
          if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
          }
          OTA_running = true;
        } else if (upload.status == UPLOAD_FILE_END) {
          if (Update.end(true)) { //true to set the size to the current progress
            Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
            String infoText = "Update Success: " + String(upload.totalSize);
            labelInfo.drawButton(false, infoText);
          } else {
            Update.printError(Serial);
            OTA_running = false;
          }
          Serial.setDebugOutput(false);
        } else {
          Serial.printf("Update Failed Unexpectedly (likely broken connection): status=%d\n", upload.status);
          OTA_running = false;
        }
      });
      server.begin();
      MDNS.addService("http", "tcp", 80);

      Serial.printf("Ready! Open http://%s.local in your browser\n", host);

      if (config.arduino_ota) {
        ArduinoOTA
        .onStart([]() {
          String type;
          if (ArduinoOTA.getCommand() == U_FLASH)
            type = "sketch";
          else // U_SPIFFS
            type = "filesystem";

          OTA_running = true;

          // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
          Serial.println("Start updating " + type);
          String infoText = "Start updating " + String(type);
          labelInfo.drawButton(false, infoText);
        })
        .onEnd([]() {
          Serial.println("\nEnd");
        })
        .onProgress([](unsigned int progress, unsigned int total) {
          Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
          String infoText = "Progress:" + String((progress / (total / 100))) + "%";
          labelInfo.drawButton(false, infoText);
        })
        .onError([](ota_error_t error) {
          Serial.printf("Error[%u]: ", error);
          String infoText = "Error: " + String(error);
          labelInfo.drawButton(false, infoText);
          if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
          else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
          else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
          else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
          else if (error == OTA_END_ERROR) Serial.println("End Failed");
        });

        ArduinoOTA.begin();
      }
      Serial.print("Default ESP32 MAC Address: ");
      Serial.println(WiFi.macAddress());

      while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi..");
      }

      String serverPath = "https://ripper121.com/roboTrickler/userTracker.php?mac=" + String(WiFi.macAddress());
      makeHttpsGetRequest(serverPath);

      WIFI_AKTIVE = true;
    } else {
      Serial.println("No Wifi");
    }
  } else {
    Serial.println("No Wifi");
  }
}
