void ceateCSVFile(fs::FS &fs, const char * folderName, const char * fileName, String message) {
  int fileCount = 0;
  String path = "";

  //cehck if folder exists, if not creat one
  if (!fs.exists(folderName)) {
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
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path.c_str(), FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  String withHeader = "Count,Weight\r\n";
  withHeader += message;
  if (file.print(withHeader.c_str())) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();

}

void writeCSVFile(fs::FS &fs, const char * folderName, const char * fileName, float weight, int count) {
  String path = String(folderName) + "/" + String(fileName);
  Serial.printf("Appending to file: %s\n", path);

  String message = String(count) + String(weight, 4);
  message += "\r\n";

  Serial.printf("Message: %s\n", message);

  if (fs.exists(path.c_str())) {
    File file = fs.open(path.c_str(), FILE_APPEND);
    if (!file) {
      Serial.println("Failed to open file for appending");
      return;
    }

    if (file.print(message.c_str())) {
      Serial.println("Message appended");
    } else {
      Serial.println("Append failed");
    }
    file.close();
  } else {
    ceateCSVFile(fs, folderName, fileName, message);
  }
}
