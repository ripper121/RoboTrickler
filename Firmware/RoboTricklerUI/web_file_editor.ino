File uploadFile;
static String uploadTargetPath;
static String uploadTemporaryPath;
static bool uploadFailed = false;
static bool uploadFilesystemLocked = false;
static const size_t WEB_EDITOR_MAX_UPLOAD = 1024 * 1024;
static const size_t WEB_EDITOR_MAX_PATH = 80;

static bool validEditorPath(const String &path, bool allowRoot = false, bool allowTrailingSlash = false)
{
  if ((path.length() == 0) || (path.length() > WEB_EDITOR_MAX_PATH) ||
      (path[0] != '/') || (!allowRoot && path == "/")) return false;
  for (size_t i = 0; i < path.length(); i++)
  {
    unsigned char c = path[i];
    if ((c < 0x20) || (c == 0x7f) || (c == '\\')) return false;
  }
  int segmentStart = 1;
  while (segmentStart < (int)path.length())
  {
    int slash = path.indexOf('/', segmentStart);
    if (slash < 0) slash = path.length();
    String segment = path.substring(segmentStart, slash);
    if ((segment.length() == 0) || (segment == ".") || (segment == "..")) return false;
    segmentStart = slash + 1;
  }
  return path == "/" || allowTrailingSlash || !path.endsWith("/");
}

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
  if (!webMutationAllowed()) uploadFailed = true;
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
  if (!validEditorPath(path, true, true)) return false;
  FilesystemLockGuard filesystemGuard;
  if (!filesystemGuard) return false;
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
  if (!recoverInterruptedFileReplacement(path) ||
      !recoverInterruptedFileReplacement(pathWithGz)) return false;
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
    if (!webMutationAllowed() || !webFilesystemMutationAllowed() ||
        !validEditorPath(upload.filename) ||
        (server.clientContentLength() < 0) ||
        (server.clientContentLength() > (int)(WEB_EDITOR_MAX_UPLOAD + 4096)))
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
    if (!uploadFailed && uploadFile && webFilesystemMutationAllowed() &&
        (upload.totalSize <= WEB_EDITOR_MAX_UPLOAD) &&
        (upload.currentSize <= WEB_EDITOR_MAX_UPLOAD - upload.totalSize))
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
  if (!webMutationAllowed()) return;
  FilesystemLockGuard filesystemGuard;
  if (!filesystemGuard || !webFilesystemMutationAllowed())
  {
    server.send(409, "text/plain", "Filesystem unavailable or device busy");
    return;
  }
  if ((server.args() != 1) || (server.clientContentLength() > 128))
  {
    return returnFail("BAD ARGS");
  }
  String path = server.arg(0);
  if (!validEditorPath(path) || !ACTIVE_FS.exists((char *)path.c_str()))
  {
    returnFail("BAD PATH");
    return;
  }
  deleteRecursive(path);
  returnOk();
}

void handleCreate()
{
  if (!webMutationAllowed()) return;
  FilesystemLockGuard filesystemGuard;
  if (!filesystemGuard || !webFilesystemMutationAllowed())
  {
    server.send(409, "text/plain", "Filesystem unavailable or device busy");
    return;
  }
  if ((server.args() != 1) || (server.clientContentLength() > 128))
  {
    return returnFail("BAD ARGS");
  }
  String path = server.arg(0);
  if (!validEditorPath(path) || ACTIVE_FS.exists((char *)path.c_str()))
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
  if (isTricklerRunning())
  {
    server.send(409, "text/plain", "Device busy");
    return;
  }
  FilesystemLockGuard filesystemGuard;
  if (!filesystemGuard)
  {
    server.send(503, "text/plain", "Filesystem busy");
    return;
  }
  if (!webFilesystemAvailable())
  {
    server.send(503, "text/plain", "Filesystem unavailable");
    return;
  }
  if ((server.args() != 1) || !server.hasArg("dir"))
  {
    return returnFail("BAD ARGS");
  }
  String path = server.arg("dir");
  if (!validEditorPath(path, true) || (path != "/" && !ACTIVE_FS.exists((char *)path.c_str())))
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
  if (server.method() != HTTP_GET)
  {
    server.send(405, "text/plain", "Method not allowed");
    return;
  }
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

  server.send(404, "text/plain", "File not found");
}
