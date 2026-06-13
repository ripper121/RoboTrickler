File uploadFile;

bool loadFromFilesystem(fs::FS &fs, const char *sourceName, String path)
{
  // Serve UI files with a small extension-to-content-type map. If a
  // compressed copy exists, prefer it transparently.
  String dataType = "text/plain";
  if (path.endsWith("/"))
  {
    path += "system/index.html";
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
  else if (path.endsWith(".json"))
  {
    dataType = "application/json";
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
  }
  else
  {
    dataType = "text/plain";
  }

  if (server.hasArg("download"))
  {
    dataType = "application/octet-stream";
  }

  File dataFile = fs.open(path.c_str());
  if (dataFile.isDirectory())
  {
    path += "/index.html";
    dataType = "text/html";
  }
  dataFile.close();

  String pathWithGz = path + ".gz";
  if (fs.exists(pathWithGz))
  {
    path = pathWithGz;
  }
  else if (!fs.exists(path))
  {
    return false;
  }

  dataFile = fs.open(path.c_str());
  if (!dataFile)
  {
    return false;
  }

  if (dataFile.isDirectory())
  {
    dataFile.close();
    return false;
  }

  DEBUG_PRINTLN(String("\tSent ") + sourceName + " file: " + path);
  if (server.streamFile(dataFile, dataType) != dataFile.size())
  {
    DEBUG_PRINTLN("Sent less data than expected!");
  }
  dataFile.close();
  return true;
}

bool loadWebFile(String path)
{
  return FILESYSTEM_ACTIVE && loadFromFilesystem(LittleFS, "LittleFS", path);
}

void handleFileUpload()
{
  if (server.uri() != "/system/resources/edit")
  {
    return;
  }
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START)
  {
    if (upload.filename.startsWith("/profiles/") && !LittleFS.exists("/profiles"))
    {
      LittleFS.mkdir("/profiles");
    }
    if (LittleFS.exists((char *)upload.filename.c_str()))
    {
      LittleFS.remove((char *)upload.filename.c_str());
    }
    uploadFile = LittleFS.open(upload.filename.c_str(), FILE_WRITE);
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
  }
}

void deleteRecursive(String path)
{
  // Used by the web editor; callers guard against deleting the filesystem root.
  File file = LittleFS.open((char *)path.c_str());
  if (!file.isDirectory())
  {
    file.close();
    LittleFS.remove((char *)path.c_str());
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
      LittleFS.remove((char *)entryPath.c_str());
    }
    yield();
  }

  LittleFS.rmdir((char *)path.c_str());
  file.close();
}

void handleDelete()
{
  if (server.args() == 0)
  {
    return returnFail("BAD ARGS");
  }
  String path = server.arg(0);
  if (path == "/" || !LittleFS.exists((char *)path.c_str()))
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
  if (path == "/" || LittleFS.exists((char *)path.c_str()))
  {
    returnFail("BAD PATH");
    return;
  }

  if (path.indexOf('.') > 0)
  {
    File file = LittleFS.open((char *)path.c_str(), FILE_WRITE);
    if (file)
    {
      file.write(0);
      file.close();
    }
  }
  else
  {
    LittleFS.mkdir((char *)path.c_str());
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
  if (path != "/" && !LittleFS.exists((char *)path.c_str()))
  {
    return returnFail("BAD PATH");
  }
  File dir = LittleFS.open((char *)path.c_str());
  path = String();
  if (!dir.isDirectory())
  {
    dir.close();
    return returnFail("NOT DIR");
  }
  dir.rewindDirectory();
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/json", "");
  bool firstEntry = true;

  server.sendContent("[");
  while (true)
  {
    File entry = dir.openNextFile();
    if (!entry)
    {
      break;
    }

    if (strcmp(entry.path(), "/resources/css") != 0)
    {

      String output;
      if (!firstEntry)
      {
        output = ',';
      }
      firstEntry = false;

      output += "{\"type\":\"";
      output += (entry.isDirectory()) ? "dir" : "file";
      output += "\",\"name\":\"";
      output += jsonEscape(String(entry.path()));
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
  if (loadWebFile(server.uri()))
  {
    return;
  }
  if (WIFI_SETUP_AP_ACTIVE)
  {
    server.sendHeader("Location", "/system/ap");
    server.send(302, "text/plain", "");
    return;
  }

  String message = "LittleFS file not found\n\n";
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

