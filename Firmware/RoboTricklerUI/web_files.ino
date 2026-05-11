File uploadFile;

struct MimeType
{
  const char *extension;
  const char *contentType;
};

static const MimeType mimeTypes[] = {
    {".html", "text/html"},
    {".htm", "text/html"},
    {".css", "text/css"},
    {".js", "application/javascript"},
    {".json", "application/json"},
    {".png", "image/png"},
    {".gif", "image/gif"},
    {".jpg", "image/jpeg"},
    {".ico", "image/x-icon"},
    {".xml", "text/xml"},
    {".pdf", "application/pdf"},
    {".zip", "application/zip"},
    {".gz", "application/x-gzip"},
};

static String contentTypeForPath(const String &path)
{
  for (size_t i = 0; i < ARRAY_COUNT(mimeTypes); i++)
  {
    if (path.endsWith(mimeTypes[i].extension))
    {
      return mimeTypes[i].contentType;
    }
  }
  return "text/plain";
}

void sendHtmlBackPage(const char *message)
{
  String html = String("<h3>") + message;
  html += "</h3><br><button onClick='javascript:history.back()'>Back</button>";
  server.send(200, "text/html", html);
}

void sendOkResponse()
{
  server.send(200, "text/plain", "");
}

void sendFailResponse(const String &msg)
{
  server.send(500, "text/plain", msg + "\r\n");
}

static bool sdPathExists(const String &path)
{
  return SD.exists((char *)path.c_str());
}

static bool failRequest(const char *message)
{
  sendFailResponse(message);
  return false;
}

static bool readEditPath(String &path, bool mustExist)
{
  if (server.args() == 0)
  {
    return failRequest("BAD ARGS");
  }

  path = server.arg(0);
  if (path == "/")
  {
    return failRequest("BAD PATH");
  }

  bool exists = sdPathExists(path);
  if ((mustExist && !exists) || (!mustExist && exists))
  {
    return failRequest("BAD PATH");
  }
  return true;
}

static bool readDirectoryPath(String &path)
{
  if (!server.hasArg("dir"))
  {
    return failRequest("BAD ARGS");
  }

  path = server.arg("dir");
  if (path != "/" && !sdPathExists(path))
  {
    return failRequest("BAD PATH");
  }
  return true;
}

bool serveFileFromSdCard(String path)
{
  if (path.endsWith("/"))
  {
    path += "system/index.html";
  }

  if (path.endsWith(".src"))
  {
    path = path.substring(0, path.lastIndexOf("."));
  }

  String dataType = contentTypeForPath(path);
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
  if (sdPathExists(pathWithGz))
  {
    path = pathWithGz;
  }
  else if (!sdPathExists(path))
  {
    return false;
  }

  dataFile = SD.open(path.c_str());
  if (!dataFile)
  {
    return false;
  }

  if (dataFile.isDirectory())
  {
    dataFile.close();
    return false;
  }

  DEBUG_PRINTLN(String("\tSent file: ") + path);
  if (server.streamFile(dataFile, dataType) != dataFile.size())
  {
    DEBUG_PRINTLN("Sent less data than expected!");
  }
  dataFile.close();
  return true;
}

static void handleUploadStart(HTTPUpload &upload)
{
  if (upload.filename.startsWith("/profiles/") && !SD.exists("/profiles"))
  {
    SD.mkdir("/profiles");
  }
  if (SD.exists((char *)upload.filename.c_str()))
  {
    SD.remove((char *)upload.filename.c_str());
  }
  uploadFile = SD.open(upload.filename.c_str(), FILE_WRITE);
  DEBUG_PRINT("Upload: START, filename: ");
  DEBUG_PRINTLN(upload.filename);
}

static void handleUploadWrite(HTTPUpload &upload)
{
  if (uploadFile)
  {
    uploadFile.write(upload.buf, upload.currentSize);
  }
  DEBUG_PRINT("Upload: WRITE, Bytes: ");
  DEBUG_PRINTLN(upload.currentSize);
}

static void handleUploadEnd(HTTPUpload &upload)
{
  if (uploadFile)
  {
    uploadFile.close();
  }
  DEBUG_PRINT("Upload: END, Size: ");
  DEBUG_PRINTLN(upload.totalSize);
}

void handleFileUpload()
{
  if (server.uri() != "/system/resources/edit")
  {
    return;
  }
  HTTPUpload &upload = server.upload();
  switch (upload.status)
  {
  case UPLOAD_FILE_START:
    handleUploadStart(upload);
    break;
  case UPLOAD_FILE_WRITE:
    handleUploadWrite(upload);
    break;
  case UPLOAD_FILE_END:
    handleUploadEnd(upload);
    break;
  default:
    break;
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
  String path;
  if (!readEditPath(path, true))
  {
    return;
  }
  deleteRecursive(path);
  sendOkResponse();
}

void handleCreate()
{
  String path;
  if (!readEditPath(path, false))
  {
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
  sendOkResponse();
}

static bool shouldListDirectoryEntry(File &entry)
{
  return strcmp(entry.path(), "/resources/css") != 0;
}

static void sendDirectoryEntry(File &entry, bool &firstEntry)
{
  String output = firstEntry ? "" : ",";
  firstEntry = false;

  output += "{\"type\":\"";
  output += entry.isDirectory() ? "dir" : "file";
  output += "\",\"name\":\"";
  output += jsonEscape(String(entry.path()));
  output += "\"}";
  server.sendContent(output);
}

void handleListDirectory()
{
  String path;
  if (!readDirectoryPath(path))
  {
    return;
  }
  File dir = SD.open((char *)path.c_str());
  path = String();
  if (!dir.isDirectory())
  {
    dir.close();
    sendFailResponse("NOT DIR");
    return;
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

    if (shouldListDirectoryEntry(entry))
    {
      sendDirectoryEntry(entry, firstEntry);
    }
    entry.close();
  }
  server.sendContent("]");
  dir.close();
}

void handleNotFound()
{
  if (serveFileFromSdCard(server.uri()))
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
