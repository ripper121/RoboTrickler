String ceateCSVFile(fs::FS &fs, const char * folderName, const char * fileName) {
  int fileCount = 0;
  String path = "";
  Serial.println("ceateCSVFile");
  //cehck if folder exists, if not creat one
  if (!fs.exists(folderName)) {
    Serial.println("Log Folder Created.");
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
    Serial.println("Failed to open file for writing");
    return "";
  }
  Serial.printf("Create file: %s\n", path);
  String header = "Count,Weight\r\n";
  if (file.print(header.c_str())) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
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
    Serial.println("Failed to open file for appending");
    ceateCSVFile(SD, "/log", "log");
    return;
  }
  Serial.printf("Appending to file: %s\n", path);
  if (file.print(message.c_str())) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void writeFile(fs::FS &fs, const char * path, String message){
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
    if(file.print(message.c_str())){
        Serial.println("File written");
    } else {
        Serial.println("Write failed");
    }
    file.close();
}

void debugLog(fs::FS &fs, String message){
    Serial.printf("Appending to file: %s\n", path);

    File file = fs.open("/debugLog.txt", FILE_APPEND);
    if(!file){
        Serial.println("Failed to open file for appending");
        writeFile(fs, "/debugLog.txt", message);
        return;
    }
    if(file.print(message.c_str())){
        Serial.println("Message appended");
    } else {
        Serial.println("Append failed");
    }
    file.close();
}
