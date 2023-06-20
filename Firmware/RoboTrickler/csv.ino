void ceateCSVFile(fs::FS &fs, const char * path, String message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  String withHeader = "Time,Weight\r\n";
  withHeader += message;
  withHeader += "\r\n";
  if (file.print(withHeader.c_str())) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void writeCSVFile(fs::FS &fs, const char * path, String message) {
  Serial.printf("Appending to file: %s\n", path);
  if (fs.exists(path)) {
    File file = fs.open(path, FILE_APPEND);
    if (!file) {
      Serial.println("Failed to open file for appending");
      return;
    }
    message += "\r\n";
    if (file.print(message)) {
      Serial.println("Message appended");
    } else {
      Serial.println("Append failed");
    }
    file.close();
  } else {
    ceateCSVFile(fs, path, message);
  }
}
