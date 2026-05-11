void handleFirmwareUpdatePage()
{
  server.sendHeader("Connection", "close");
  String updatePage = "<form method='POST' action='/update' enctype='multipart/form-data'><p>FW-Version: ";
  updatePage += String(FW_VERSION);
  updatePage += "</p><br><input type='file' name='update'>";
  updatePage += "<input type='submit' value='Update'></form><br>";
  updatePage += "<button onClick='javascript:history.back()'>Back</button>";
  server.send(200, "text/html", updatePage);
}

void handleFirmwareUpdatePostResult()
{
  server.sendHeader("Connection", "close");
  sendHtmlBackPage(Update.hasError() ? "FAIL" : "OK");
  ESP.restart();
}

void handleFirmwareUpdateUpload()
{
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START)
  {
    Serial.setDebugOutput(true);
    Serial.printf("Update: %s\n", upload.filename.c_str());
    String infoText = String(languageText("status_update_upload")) + String(upload.filename);
    updateDisplayLog(infoText);
    if (!Update.begin(UPDATE_SIZE_UNKNOWN))
    {
      Update.printError(Serial);
      updateDisplayLog(String(languageText("status_update_failed")) + Update.errorString());
    }
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
    {
      Update.printError(Serial);
      updateDisplayLog(String(languageText("status_update_write_failed")) + Update.errorString());
    }
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (Update.end(true))
    {
      Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      String infoText = String(languageText("status_update_success")) + String(upload.totalSize);
      updateDisplayLog(infoText);
    }
    else
    {
      Update.printError(Serial);
      updateDisplayLog(String(languageText("status_update_end_failed")) + Update.errorString());
    }
    Serial.setDebugOutput(false);
  }
  else
  {
    Serial.printf("Update Failed Unexpectedly (likely broken connection): status=%d\n", upload.status);
    updateDisplayLog(languageText("status_update_unexpected"));
  }
}
