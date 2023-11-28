String ceateCSVFile(fs::FS &fs, const char * folderName, const char * fileName) {
  int fileCount = 0;
  String path = "";
  DEBUG_PRINT("ceateCSVFile\n");
  //cehck if folder exists, if not creat one
  if (!fs.exists(folderName)) {
    DEBUG_PRINT("Log Folder Created.\n");
    fs.mkdir(folderName);
  }
  //check if file exists, if not count up and creat new one
  while (true) {
    path = String(folderName) + "/" + String(fileName) + "_" + String(fileCount) + ".csv";
    if (fs.exists(path.c_str())) {
      fileCount++;
    } else {
      break;
    }
  }

  File file = fs.open(path.c_str(), FILE_WRITE);
  if (!file) {
    DEBUG_PRINT("Failed to open file for writing\n");
    return "";
  }
  Serial.printf("Create file: %s\n", path);
  String header = "Count,Weight\r\n";
  if (file.print(header.c_str())) {
    DEBUG_PRINT("File written\n");
  } else {
    DEBUG_PRINT("Write failed\n");
  }
  file.close();

  return path;
}

void writeCSVFile(fs::FS &fs, const char * path, float weight, int count) {
  Serial.printf("Try to open file: %s\n", path);

  String message = String(count) + "," + String(weight, 4) + "\r\n";

  Serial.printf("Message: %s", message);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    DEBUG_PRINT("Failed to open file for appending\n");
    ceateCSVFile(SD, "/log", "log");
    return;
  }
  Serial.printf("Appending to file: %s\n", path);
  if (file.print(message.c_str())) {
    DEBUG_PRINT("Message appended\n");
  } else {
    DEBUG_PRINT("Append failed\n");
  }
  file.close();
}

void writeFile(fs::FS &fs, const char * path, String message){
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        DEBUG_PRINTLN("Failed to open file for writing");
        return;
    }
    if(file.print(message.c_str())){
        DEBUG_PRINTLN("File written");
    } else {
        DEBUG_PRINTLN("Write failed");
    }
    file.close();
}

void logToFile(fs::FS &fs, String message){
    Serial.printf("Appending to file: %s\n", path);

    File file = fs.open("/debugLog.txt", FILE_APPEND);
    if(!file){
        DEBUG_PRINTLN("Failed to open file for appending");
        writeFile(fs, "/debugLog.txt", message);
        return;
    }
    if(file.print(message.c_str())){
        DEBUG_PRINTLN("Message appended");
    } else {
        DEBUG_PRINTLN("Append failed");
    }
    file.close();
}
