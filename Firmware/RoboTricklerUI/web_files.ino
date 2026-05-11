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

void sendHtmlBackPage(const char *message)
{
  server.send(200, "text/html", String("<h3>") + message + "</h3><br><button onClick='javascript:history.back()'>Back</button>");
}

void sendOkResponse()
{
  server.send(200, "text/plain", "");
}

void sendFailResponse(String msg)
{
  server.send(500, "text/plain", msg + "\r\n");
}

bool serveFileFromSdCard(String path)
{
  String dataType = "text/plain";
  if (path.endsWith("/"))
  {
    path += "system/index.html";
  }

  if (path.endsWith(".src"))
  {
    path = path.substring(0, path.lastIndexOf("."));
  }

  for (size_t i = 0; i < ARRAY_COUNT(mimeTypes); i++)
  {
    if (path.endsWith(mimeTypes[i].extension))
    {
      dataType = mimeTypes[i].contentType;
      break;
    }
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
  if (SD.exists(pathWithGz))
  {
    path = pathWithGz;
  }
  else if (!SD.exists(path))
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

void handleFileUpload()
{
  if (server.uri() != "/system/resources/edit")
  {
    return;
  }
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START)
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
    return sendFailResponse("BAD ARGS");
  }
  String path = server.arg(0);
  if (path == "/" || !SD.exists((char *)path.c_str()))
  {
    sendFailResponse("BAD PATH");
    return;
  }
  deleteRecursive(path);
  sendOkResponse();
}

void handleCreate()
{
  if (server.args() == 0)
  {
    return sendFailResponse("BAD ARGS");
  }
  String path = server.arg(0);
  if (path == "/" || SD.exists((char *)path.c_str()))
  {
    sendFailResponse("BAD PATH");
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

void handleListDirectory()
{
  if (!server.hasArg("dir"))
  {
    return sendFailResponse("BAD ARGS");
  }
  String path = server.arg("dir");
  if (path != "/" && !SD.exists((char *)path.c_str()))
  {
    return sendFailResponse("BAD PATH");
  }
  File dir = SD.open((char *)path.c_str());
  path = String();
  if (!dir.isDirectory())
  {
    dir.close();
    return sendFailResponse("NOT DIR");
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
