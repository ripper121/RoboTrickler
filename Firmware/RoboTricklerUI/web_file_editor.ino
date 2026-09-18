File uploadFile;
static String uploadTargetPath;
static String uploadTemporaryPath;
static bool uploadFailed = false;
static bool uploadFilesystemLocked = false;

bool isWebFileUploadActive()
{
  return uploadFilesystemLocked;
}

static void releaseUploadFilesystemLock()
{
  if (uploadFilesystemLocked)
  {
    uploadFilesystemLocked = false;
    filesystemUnlock();
  }
}

static bool webFilesystemAvailable()
{
  return filesystemActive && (activeFs != NULL);
}

static bool webFilesystemMutationAllowed()
{
  return webFilesystemAvailable() &&
         !isTricklerRunning() &&
         !isCalibrationProfilePromptPending() &&
         !isProfileTuneTestActive();
}

void finishFileUploadRequest()
{
  releaseUploadFilesystemLock();
  bool failed = uploadFailed;
  uploadFailed = false;
  uploadTargetPath = "";
  uploadTemporaryPath = "";
  if (failed)
  {
    server.send(409, "text/plain", "Upload rejected or failed");
    return;
  }
  returnOk();
}

bool loadFromFilesystem(fs::FS &fs, const char *sourceName, String path)
{
  // Serve UI files with a small extension-to-content-type map. If a
  // compressed copy exists, prefer it transparently.
  String dataType = "text/plain";
  if (path.endsWith("/"))
  {
    path += "system/index.html";
  }

  if (path.endsWith(".html") || path.endsWith(".htm"))
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
  {
    FilesystemLockGuard filesystemGuard;
    if (!filesystemGuard ||
        !recoverInterruptedFileReplacement(path) ||
        !recoverInterruptedFileReplacement(pathWithGz))
    {
      return false;
    }
  }
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
  return webFilesystemAvailable() && loadFromFilesystem(ACTIVE_FS, activeFsIsSd ? "SD" : "LittleFS", path);
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
    if (uploadFile)
    {
      uploadFile.close();
    }
    if (uploadFilesystemLocked && webFilesystemAvailable() &&
        (uploadTemporaryPath.length() > 0))
    {
      ACTIVE_FS.remove(uploadTemporaryPath.c_str());
    }
    releaseUploadFilesystemLock();
    uploadFailed = true;
    uploadTargetPath = "";
    uploadTemporaryPath = "";
    if (!webFilesystemMutationAllowed() || !upload.filename.startsWith("/") ||
        (upload.filename == "/"))
    {
      return;
    }
    if (!filesystemLock())
    {
      return;
    }
    uploadFilesystemLocked = true;
    if (upload.filename.startsWith("/profiles/") && !ACTIVE_FS.exists("/profiles"))
    {
      if (!ACTIVE_FS.mkdir("/profiles"))
      {
        return;
      }
    }
    uploadTargetPath = upload.filename;
    uploadTemporaryPath = uploadTargetPath + ".upload.tmp";
    ACTIVE_FS.remove(uploadTemporaryPath.c_str());
    uploadFile = ACTIVE_FS.open(uploadTemporaryPath.c_str(), FILE_WRITE);
    uploadFailed = !uploadFile;
    DEBUG_PRINT("Upload: START, filename: ");
    DEBUG_PRINTLN(upload.filename);
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    if (!uploadFailed && uploadFile && webFilesystemMutationAllowed())
    {
      if (uploadFile.write(upload.buf, upload.currentSize) != upload.currentSize)
      {
        uploadFailed = true;
      }
    }
    else
    {
      uploadFailed = true;
    }
    DEBUG_PRINT("Upload: WRITE, Bytes: ");
    DEBUG_PRINTLN(upload.currentSize);
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (uploadFile)
    {
      uploadFile.flush();
      uploadFile.close();
    }
    if (!uploadFailed && webFilesystemMutationAllowed())
    {
      uploadFailed = !replaceFileWithTemp(uploadTargetPath, uploadTemporaryPath);
    }
    if (uploadFailed && webFilesystemAvailable() && (uploadTemporaryPath.length() > 0))
    {
      ACTIVE_FS.remove(uploadTemporaryPath.c_str());
    }
    releaseUploadFilesystemLock();
    DEBUG_PRINT("Upload: END, Size: ");
    DEBUG_PRINTLN(upload.totalSize);
  }
  else
  {
    uploadFailed = true;
    if (uploadFile)
    {
      uploadFile.close();
    }
    if (webFilesystemAvailable() && (uploadTemporaryPath.length() > 0))
    {
      ACTIVE_FS.remove(uploadTemporaryPath.c_str());
    }
    releaseUploadFilesystemLock();
  }
}

void deleteRecursive(String path)
{
  // Used by the web editor; callers guard against deleting the filesystem root.
  File file = ACTIVE_FS.open((char *)path.c_str());
  if (!file.isDirectory())
  {
    file.close();
    ACTIVE_FS.remove((char *)path.c_str());
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
      ACTIVE_FS.remove((char *)entryPath.c_str());
    }
    yield();
  }

  ACTIVE_FS.rmdir((char *)path.c_str());
  file.close();
}

void handleDelete()
{
  FilesystemLockGuard filesystemGuard;
  if (!filesystemGuard || !webFilesystemMutationAllowed())
  {
    server.send(409, "text/plain", "Filesystem unavailable or device busy");
    return;
  }
  if (server.args() == 0)
  {
    return returnFail("BAD ARGS");
  }
  String path = server.arg(0);
  if (path == "/" || !ACTIVE_FS.exists((char *)path.c_str()))
  {
    returnFail("BAD PATH");
    return;
  }
  deleteRecursive(path);
  returnOk();
}

void handleCreate()
{
  FilesystemLockGuard filesystemGuard;
  if (!filesystemGuard || !webFilesystemMutationAllowed())
  {
    server.send(409, "text/plain", "Filesystem unavailable or device busy");
    return;
  }
  if (server.args() == 0)
  {
    return returnFail("BAD ARGS");
  }
  String path = server.arg(0);
  if (path == "/" || ACTIVE_FS.exists((char *)path.c_str()))
  {
    returnFail("BAD PATH");
    return;
  }

  if (path.indexOf('.') > 0)
  {
    // Create an empty file. (The upstream SDWebServer example wrote a stray
    // NUL byte here, which broke newly created files parsed as JSON/text.)
    File file = ACTIVE_FS.open((char *)path.c_str(), FILE_WRITE);
    if (!file)
    {
      returnFail("CREATE FAILED");
      return;
    }
    else
    {
      file.close();
    }
  }
  else
  {
    if (!ACTIVE_FS.mkdir((char *)path.c_str()))
    {
      returnFail("CREATE FAILED");
      return;
    }
  }
  returnOk();
}

void printDirectory()
{
  if (!webFilesystemAvailable())
  {
    server.send(503, "text/plain", "Filesystem unavailable");
    return;
  }
  if (!server.hasArg("dir"))
  {
    return returnFail("BAD ARGS");
  }
  String path = server.arg("dir");
  if (path != "/" && !ACTIVE_FS.exists((char *)path.c_str()))
  {
    return returnFail("BAD PATH");
  }
  File dir = ACTIVE_FS.open((char *)path.c_str());
  path = String();
  if (!dir.isDirectory())
  {
    dir.close();
    return returnFail("NOT DIR");
  }
  dir.rewindDirectory();
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  bool firstEntry = true;

  server.sendContent("[");
  while (true)
  {
    File entry = dir.openNextFile();
    if (!entry)
    {
      break;
    }

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
  if (wifiSetupApActive)
  {
    server.sendHeader("Location", "/system/ap");
    server.send(302, "text/plain", "");
    return;
  }

  String message = "Filesystem file not found\n\n";
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
