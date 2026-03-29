#include "config.h"
#include "arduino_compat.h"
#include "pindef.h"
#include "ui.h"

#include <string>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <dirent.h>
#include <errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "mdns.h"
#include "lwip/ip4_addr.h"

static const char *TAG = "webserver";

// ============================================================
// Forward declarations
// ============================================================
extern void updateDisplayLog(const std::string &msg, bool noLog);
extern void messageBox(const std::string &msg, const lv_font_t *font, lv_color_t color, bool wait);
extern void setProfile(int num);
extern void saveTargetWeight(float w);
extern void startTrickler();
extern void stopTrickler();
extern float weight;

// ============================================================
// WiFi event group
// ============================================================
#include "freertos/event_groups.h"
static EventGroupHandle_t s_wifi_event_group = NULL;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static httpd_handle_t s_httpd = NULL;

static void delayed_restart_task(void *arg) {
    const TickType_t delay_ticks = (TickType_t)(uintptr_t)arg;
    vTaskDelay(delay_ticks);
    esp_restart();
}

static void schedule_restart(TickType_t delay_ticks) {
    xTaskCreate(delayed_restart_task, "http_restart", 2048, (void *)delay_ticks, 5, NULL);
}

// ============================================================
// SD path helper
// ============================================================
static std::string sd_path(const std::string &rel) {
    if (!rel.empty() && rel[0] == '/') return std::string(SD_MOUNT_POINT) + rel;
    return std::string(SD_MOUNT_POINT) + "/" + rel;
}

static bool normalize_editor_path(const char *raw_path, std::string *normalized_path) {
    if (raw_path == NULL || normalized_path == NULL) {
        return false;
    }

    std::string path(raw_path);
    if (path.empty()) {
        return false;
    }

    if (path[0] != '/') {
        path.insert(path.begin(), '/');
    }

    if (path == "/") {
        return false;
    }

    *normalized_path = path;
    return true;
}

// ============================================================
// WiFi event handler
// ============================================================
static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        WIFI_AKTIVE = false;
        ESP_LOGW(TAG, "WiFi disconnected");
        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        char ip_str[16];
        snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Got IP: %s", ip_str);
        WIFI_AKTIVE = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        xEventGroupClearBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
}

// ============================================================
// Content-type from path
// ============================================================
static const char *content_type(const std::string &path) {
    if (str_ends_with(path, ".html") || str_ends_with(path, ".htm")) return "text/html";
    if (str_ends_with(path, ".css"))  return "text/css";
    if (str_ends_with(path, ".js"))   return "application/javascript";
    if (str_ends_with(path, ".png"))  return "image/png";
    if (str_ends_with(path, ".jpg"))  return "image/jpeg";
    if (str_ends_with(path, ".gif"))  return "image/gif";
    if (str_ends_with(path, ".ico"))  return "image/x-icon";
    if (str_ends_with(path, ".xml"))  return "text/xml";
    if (str_ends_with(path, ".pdf"))  return "application/pdf";
    if (str_ends_with(path, ".zip"))  return "application/zip";
    if (str_ends_with(path, ".gz"))   return "application/x-gzip";
    return "text/plain";
}

// ============================================================
// Query string helper
// ============================================================
static bool get_query_param(httpd_req_t *req, const char *key, char *val, size_t val_len) {
    size_t qlen = httpd_req_get_url_query_len(req) + 1;
    if (qlen <= 1) return false;
    char *qs = (char *)malloc(qlen);
    if (!qs) return false;
    bool found = false;
    if (httpd_req_get_url_query_str(req, qs, qlen) == ESP_OK) {
        found = (httpd_query_key_value(qs, key, val, val_len) == ESP_OK);
    }
    free(qs);
    return found;
}

// ============================================================
// Stream a file from SD card to the HTTP response
// ============================================================
static esp_err_t send_file(httpd_req_t *req, const std::string &rel_path) {
    std::string path = rel_path;
    if (!path.empty() && path.back() == '/') {
        path += "system/index.html";
    }
    if (str_ends_with(path, ".src")) {
        path.resize(path.size() - 4);
    }

    std::string full = sd_path(path);
    struct stat st = {};
    if (stat(full.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        if (!path.empty() && path.back() != '/') {
            path += "/";
        }
        path += "index.html";
        full = sd_path(path);
    }

    std::string gz = full + ".gz";
    bool is_gz = (access(gz.c_str(), F_OK) == 0);
    std::string actual = is_gz ? gz : full;

    if (access(actual.c_str(), F_OK) != 0) return ESP_FAIL;

    if (is_gz) httpd_resp_set_hdr(req, "Content-Encoding", "gzip");

    // Check if download was requested
    size_t qlen = httpd_req_get_url_query_len(req) + 1;
    bool download = false;
    if (qlen > 1) {
        char *qs = (char *)malloc(qlen);
        if (qs) {
            httpd_req_get_url_query_str(req, qs, qlen);
            char dl[8] = {};
            download = (httpd_query_key_value(qs, "download", dl, sizeof(dl)) == ESP_OK);
            free(qs);
        }
    }

    httpd_resp_set_type(req, download ? "application/octet-stream" : content_type(rel_path));

    FILE *f = fopen(actual.c_str(), "rb");
    if (!f) return ESP_FAIL;

    char buf[512];
    size_t n;
    esp_err_t result = ESP_OK;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        result = httpd_resp_send_chunk(req, buf, (ssize_t)n);
        if (result != ESP_OK) {
            break;
        }
    }
    fclose(f);
    if (result != ESP_OK) {
        return result;
    }
    return httpd_resp_send_chunk(req, NULL, 0);
}

// ============================================================
// deleteRecursive
// ============================================================
static void delete_recursive(const std::string &full_path) {
    struct stat st;
    if (stat(full_path.c_str(), &st) != 0) return;

    if (!S_ISDIR(st.st_mode)) {
        unlink(full_path.c_str());
        return;
    }

    DIR *dir = opendir(full_path.c_str());
    if (!dir) { rmdir(full_path.c_str()); return; }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        delete_recursive(full_path + "/" + entry->d_name);
    }
    closedir(dir);
    rmdir(full_path.c_str());
}

// ============================================================
// HTTP Handlers
// ============================================================

static esp_err_t handle_not_found(httpd_req_t *req) {
    std::string uri(req->uri);
    if (uri == "/") uri = "/system/index.html";

    if (send_file(req, uri) == ESP_OK) return ESP_OK;

    std::string msg = "SDCARD Not Detected\n\nURI: " + uri;
    httpd_resp_set_status(req, "404 Not Found");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, msg.c_str());
    return ESP_OK;
}

static esp_err_t handle_list(httpd_req_t *req) {
    char dir_arg[64] = {};
    if (!get_query_param(req, "dir", dir_arg, sizeof(dir_arg))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "BAD ARGS");
        return ESP_OK;
    }

    std::string full = sd_path(dir_arg);
    DIR *d = opendir(full.c_str());
    if (!d) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "BAD PATH"); return ESP_OK; }

    httpd_resp_set_type(req, "text/json");
    httpd_resp_sendstr_chunk(req, "[");

    struct dirent *entry;
    bool first = true;
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        // Skip resources/css like Arduino version
        std::string name(entry->d_name);
        if (name == "css") continue;

        std::string entry_path;
        if (std::string(dir_arg) == "/") {
            entry_path = "/" + name;
        } else {
            entry_path = std::string(dir_arg);
            if (!entry_path.empty() && entry_path.back() != '/') {
                entry_path += "/";
            }
            entry_path += name;
        }

        std::string item = std::string(first ? "" : ",") +
            "{\"type\":\"" + (entry->d_type == DT_DIR ? "dir" : "file") +
            "\",\"name\":\"" + entry_path + "\"}";
        httpd_resp_sendstr_chunk(req, item.c_str());
        first = false;
    }
    closedir(d);
    httpd_resp_sendstr_chunk(req, "]");
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t handle_delete(httpd_req_t *req) {
    size_t qlen = httpd_req_get_url_query_len(req) + 1;
    if (qlen <= 1) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "BAD ARGS"); return ESP_OK; }
    char *qs = (char *)malloc(qlen);
    char path_arg[128] = {};
    if (qs) {
        httpd_req_get_url_query_str(req, qs, qlen);
        // First arg (index 0) is the path
        httpd_query_key_value(qs, "path", path_arg, sizeof(path_arg));
        free(qs);
    }
    if (strlen(path_arg) == 0) {
        // Fall back to raw query as path
        char tmp[128] = {};
        if (httpd_req_get_url_query_str(req, tmp, sizeof(tmp)) == ESP_OK) {
            strlcpy(path_arg, tmp, sizeof(path_arg));
        }
    }

    std::string normalized_path;
    if (!normalize_editor_path(path_arg, &normalized_path)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "BAD PATH");
        return ESP_OK;
    }
    std::string full = sd_path(normalized_path);
    delete_recursive(full);
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_sendstr(req, "");
    return ESP_OK;
}

static esp_err_t handle_create(httpd_req_t *req) {
    char path_arg[128] = {};
    get_query_param(req, "path", path_arg, sizeof(path_arg));
    std::string normalized_path;
    if (!normalize_editor_path(path_arg, &normalized_path)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "BAD PATH");
        return ESP_OK;
    }
    std::string full = sd_path(normalized_path);
    // If it has a dot → create file, else mkdir
    if (normalized_path.find('.') != std::string::npos) {
        FILE *f = fopen(full.c_str(), "w");
        if (f) { fputc(0, f); fclose(f); }
    } else {
        mkdir(full.c_str(), 0755);
    }
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_sendstr(req, "");
    return ESP_OK;
}

// File upload (multipart/form-data)
static esp_err_t handle_upload(httpd_req_t *req) {
    char filename[128] = {};
    // Extract filename from Content-Disposition header
    if (httpd_req_get_hdr_value_str(req, "Content-Disposition", filename, sizeof(filename)) != ESP_OK) {
        strlcpy(filename, "/upload.bin", sizeof(filename));
    }

    std::string full = sd_path(filename);
    FILE *f = fopen(full.c_str(), "w");
    if (!f) { httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OPEN FAILED"); return ESP_OK; }

    char buf[512];
    int remaining = req->content_len;
    int received;
    while (remaining > 0) {
        int to_recv = (remaining > (int)sizeof(buf)) ? (int)sizeof(buf) : remaining;
        received = httpd_req_recv(req, buf, to_recv);
        if (received <= 0) break;
        fwrite(buf, 1, received, f);
        remaining -= received;
    }
    fclose(f);

    httpd_resp_set_status(req, "200 OK");
    httpd_resp_sendstr(req, "");
    return ESP_OK;
}

static esp_err_t handle_reboot(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_sendstr(req, "<h3>Reboot now.</h3><br><button onClick='javascript:history.back()'>Back</button>");
    schedule_restart(pdMS_TO_TICKS(250));
    return ESP_OK;
}

static esp_err_t handle_get_weight(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, str_float(weight, 3).c_str());
    return ESP_OK;
}

static esp_err_t handle_get_target(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, str_float(config.targetWeight, 3).c_str());
    return ESP_OK;
}

static esp_err_t handle_set_target(httpd_req_t *req) {
    char val[32] = {};
    if (get_query_param(req, "targetWeight", val, sizeof(val))) {
        float tw = str_to_float(val);
        if (tw > 0 && tw < MAX_TARGET_WEIGHT && config.targetWeight != tw) {
            saveTargetWeight(tw);
        }
    }
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req, "<h3>Value Set.</h3><br><button onClick='javascript:history.back()'>Back</button>");
    return ESP_OK;
}

static esp_err_t handle_get_profile(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, config.profile);
    return ESP_OK;
}

static esp_err_t handle_set_profile(httpd_req_t *req) {
    char val[16] = {};
    if (get_query_param(req, "profileNumber", val, sizeof(val))) {
        int num = str_to_int(val);
        if (num >= 0) setProfile(num);
    }
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req, "<h3>Value Set.</h3><br><button onClick='javascript:history.back()'>Back</button>");
    return ESP_OK;
}

static esp_err_t handle_get_profile_list(httpd_req_t *req) {
    std::string msg = "{";
    for (int i = 0; i < (int)profileListCount; i++) {
        msg += "\"" + std::to_string(i) + "\":\"" + profileListBuff[i] + "\"";
        if (i < (int)profileListCount - 1) msg += ",";
    }
    msg += "}";
    httpd_resp_set_type(req, "text/json");
    httpd_resp_sendstr(req, msg.c_str());
    return ESP_OK;
}

static esp_err_t handle_system_start(httpd_req_t *req) {
    startTrickler();
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req, "<h3>Running...</h3><br><button onClick='javascript:history.back()'>Back</button>");
    return ESP_OK;
}

static esp_err_t handle_system_stop(httpd_req_t *req) {
    stopTrickler();
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req, "<h3>Stopped...</h3><br><button onClick='javascript:history.back()'>Back</button>");
    return ESP_OK;
}

static esp_err_t handle_fw_update_page(httpd_req_t *req) {
    std::string page = "<form method='POST' action='/update' enctype='multipart/form-data'>"
                       "<p>FW-Version: " + str_float(FW_VERSION, 2) + "</p><br>"
                       "<input type='file' name='update'>"
                       "<input type='submit' value='Update'></form><br>"
                       "<button onClick='javascript:history.back()'>Back</button>";
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req, page.c_str());
    return ESP_OK;
}

static esp_err_t handle_edit_page(httpd_req_t *req) {
    if (send_file(req, "/system/resources/edit") == ESP_OK) {
        return ESP_OK;
    }

    static const char page[] =
        "<!DOCTYPE html>"
        "<html><head><meta charset='utf-8'><title>SD Editor</title>"
        "<style>"
        "body{font-family:sans-serif;max-width:900px;margin:24px auto;padding:0 16px;}"
        "button,input{font:inherit;margin:4px 0;padding:8px 10px;}"
        "code,pre{background:#f3f3f3;padding:2px 4px;border-radius:4px;}"
        "#files{margin-top:16px;padding-left:20px;}"
        ".row{display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin:8px 0;}"
        "</style></head><body>"
        "<h2>SD Card Editor</h2>"
        "<p>Browse with <code>?dir=/</code>, create files or folders, upload files, and delete entries.</p>"
        "<div class='row'><label for='dir'>Directory</label><input id='dir' value='/'><button onclick='loadList()'>Load</button></div>"
        "<div class='row'><label for='createPath'>Create path</label><input id='createPath' placeholder='/new.txt'><button onclick='createPath()'>Create</button></div>"
        "<div class='row'><label for='deletePath'>Delete path</label><input id='deletePath' placeholder='/old.txt'><button onclick='deletePath()'>Delete</button></div>"
        "<div class='row'><input id='uploadFile' type='file'><button onclick='uploadFile()'>Upload</button></div>"
        "<pre id='status'>Ready</pre><ul id='files'></ul>"
        "<script>"
        "const statusEl=document.getElementById('status');"
        "const filesEl=document.getElementById('files');"
        "function setStatus(msg){statusEl.textContent=msg;}"
        "async function loadList(){"
        " const dir=document.getElementById('dir').value||'/';"
        " setStatus('Loading '+dir+' ...');"
        " const res=await fetch('/list?dir='+encodeURIComponent(dir));"
        " const text=await res.text();"
        " if(!res.ok){setStatus(text);return;}"
        " const items=JSON.parse(text);"
        " filesEl.innerHTML='';"
        " for(const item of items){"
        "  const li=document.createElement('li');"
        "  li.textContent=item.type+': '+item.name;"
        "  filesEl.appendChild(li);"
        " }"
        " setStatus('Loaded '+items.length+' entries from '+dir);"
        "}"
        "function normalizePath(value){"
        " let path=(value||'').trim();"
        " if(!path){return '';}"
        " if(!path.startsWith('/')){path='/'+path;}"
        " return path;"
        "}"
        "async function createPath(){"
        " const path=normalizePath(document.getElementById('createPath').value);"
        " if(!path || path==='/'){setStatus('Enter a valid path to create');return;}"
        " const res=await fetch('/system/resources/edit?path='+encodeURIComponent(path),{method:'PUT'});"
        " setStatus(res.ok?'Created '+path:await res.text());"
        "}"
        "async function deletePath(){"
        " const path=normalizePath(document.getElementById('deletePath').value);"
        " if(!path || path==='/'){setStatus('Enter a valid path to delete');return;}"
        " const res=await fetch('/system/resources/edit?path='+encodeURIComponent(path),{method:'DELETE'});"
        " setStatus(res.ok?'Deleted '+path:await res.text());"
        "}"
        "async function uploadFile(){"
        " const fileInput=document.getElementById('uploadFile');"
        " if(!fileInput.files.length){setStatus('Choose a file first');return;}"
        " const file=fileInput.files[0];"
        " const res=await fetch('/system/resources/edit',{method:'POST',headers:{'Content-Disposition': file.name},body:file});"
        " setStatus(res.ok?'Uploaded '+file.name:await res.text());"
        "}"
        "loadList();"
        "</script></body></html>";

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// OTA upload handler — receives the firmware binary
#include "esp_ota_ops.h"
static esp_err_t handle_fw_update_post(httpd_req_t *req) {
    const esp_partition_t *update_part = esp_ota_get_next_update_partition(NULL);
    if (!update_part) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No OTA partition");
        return ESP_OK;
    }

    esp_ota_handle_t ota_handle = 0;
    esp_err_t err = esp_ota_begin(update_part, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        return ESP_OK;
    }

    char buf[512];
    int remaining = req->content_len;
    bool ok = true;
    while (remaining > 0) {
        int to_recv = (remaining > (int)sizeof(buf)) ? (int)sizeof(buf) : remaining;
        int received = httpd_req_recv(req, buf, to_recv);
        if (received <= 0) { ok = false; break; }
        if (esp_ota_write(ota_handle, buf, received) != ESP_OK) { ok = false; break; }
        remaining -= received;
    }

    if (!ok || esp_ota_end(ota_handle) != ESP_OK) {
        httpd_resp_set_type(req, "text/html");
        httpd_resp_sendstr(req, "<h3>FAIL</h3><br><input type='button' value='Back' onClick='javascript:history.back()'>");
        return ESP_OK;
    }

    esp_ota_set_boot_partition(update_part);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_sendstr(req, "<h3>OK</h3><br><input type='button' value='Back' onClick='javascript:history.back()'>");
    schedule_restart(pdMS_TO_TICKS(500));
    return ESP_OK;
}

// Catch-all for unregistered URIs
static esp_err_t handle_default(httpd_req_t *req) {
    return handle_not_found(req);
}

// ============================================================
// makeHttpsGetRequest — firmware version check
// ============================================================
static void makeHttpsGetRequest(const std::string &url) {
    esp_http_client_config_t cfg = {};
    cfg.url            = url.c_str();
    cfg.transport_type = HTTP_TRANSPORT_OVER_SSL;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.buffer_size    = 512;

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return;

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int code = esp_http_client_get_status_code(client);
        if (code == 200) {
            char body[32] = {};
            esp_http_client_read(client, body, sizeof(body) - 1);
            float remote_ver = str_to_float(body);
            if (FW_VERSION < remote_ver) {
                messageBox("New firmware available:\n\nv" + std::string(body) +
                           "\n\nCheck: https://robo-trickler.de",
                           &lv_font_montserrat_14, lv_color_hex(0x00FF00), true);
            }
        }
    }
    esp_http_client_cleanup(client);
}

// ============================================================
// Register all URI handlers
// ============================================================
#define REGISTER_URI(svr, path_value, method_value, handler_fn) do { \
    httpd_uri_t u = {}; \
    u.uri     = (path_value); \
    u.method  = (method_value); \
    u.handler = (handler_fn); \
    httpd_register_uri_handler((svr), &u); \
} while(0)

static void register_handlers(httpd_handle_t svr) {
    REGISTER_URI(svr, "/list",                      HTTP_GET,    handle_list);
    REGISTER_URI(svr, "/system/resources/edit",     HTTP_GET,    handle_edit_page);
    REGISTER_URI(svr, "/system/resources/edit",     HTTP_DELETE, handle_delete);
    REGISTER_URI(svr, "/system/resources/edit",     HTTP_PUT,    handle_create);
    REGISTER_URI(svr, "/system/resources/edit",     HTTP_POST,   handle_upload);
    REGISTER_URI(svr, "/reboot",                    HTTP_GET,    handle_reboot);
    REGISTER_URI(svr, "/getWeight",                 HTTP_GET,    handle_get_weight);
    REGISTER_URI(svr, "/getTarget",                 HTTP_GET,    handle_get_target);
    REGISTER_URI(svr, "/setTarget",                 HTTP_GET,    handle_set_target);
    REGISTER_URI(svr, "/getProfile",                HTTP_GET,    handle_get_profile);
    REGISTER_URI(svr, "/setProfile",                HTTP_GET,    handle_set_profile);
    REGISTER_URI(svr, "/getProfileList",            HTTP_GET,    handle_get_profile_list);
    REGISTER_URI(svr, "/system/start",              HTTP_GET,    handle_system_start);
    REGISTER_URI(svr, "/system/stop",               HTTP_GET,    handle_system_stop);
    REGISTER_URI(svr, "/fwupdate",                  HTTP_GET,    handle_fw_update_page);
    REGISTER_URI(svr, "/update",                    HTTP_POST,   handle_fw_update_post);
}

// ============================================================
// initWebServer — connect WiFi, start mDNS, start httpd
// ============================================================
void initWebServer() {
    WIFI_AKTIVE = false;
    if (strlen(config.wifi_ssid) == 0) return;

    updateDisplayLog("Connect to Wifi: ", false);
    updateDisplayLog(config.wifi_ssid, false);

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();

    // Static IP / DNS
    if (strlen(config.IPStatic) > 0) {
        esp_netif_dhcpc_stop(sta_netif);
        esp_netif_ip_info_t ip_info = {};

        auto parse_ip = [](const char *s) -> uint32_t {
            esp_ip4_addr_t addr;
            addr.addr = esp_ip4addr_aton(s);
            return addr.addr;
        };

        ip_info.ip.addr      = parse_ip(config.IPStatic);
        ip_info.gw.addr      = parse_ip(config.IPGateway);
        ip_info.netmask.addr = parse_ip(config.IPSubnet);
        esp_netif_set_ip_info(sta_netif, &ip_info);

        if (strlen(config.IPDns) > 0) {
            esp_netif_dns_info_t dns = {};
            dns.ip.u_addr.ip4.addr = parse_ip(config.IPDns);
            dns.ip.type = ESP_IPADDR_TYPE_V4;
            esp_netif_set_dns_info(sta_netif, ESP_NETIF_DNS_MAIN, &dns);
        }
    }

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,   &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT,   IP_EVENT_STA_GOT_IP,&wifi_event_handler, NULL);

    wifi_config_t wifi_cfg = {};
    strlcpy((char *)wifi_cfg.sta.ssid,     config.wifi_ssid, sizeof(wifi_cfg.sta.ssid));
    strlcpy((char *)wifi_cfg.sta.password, config.wifi_psk,  sizeof(wifi_cfg.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    messageBox("Connect to Wifi: " + std::string(config.wifi_ssid) + "\n\nPlease wait...",
               &lv_font_montserrat_14, lv_color_hex(0xFFFFFF), false);

    ESP_ERROR_CHECK(esp_wifi_connect());

    // Wait up to 10 seconds for connection
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(10000));

    if (bits & WIFI_CONNECTED_BIT) {
        updateDisplayLog("Wifi Connected", false);

        // mDNS
        mdns_init();
        mdns_hostname_set("robo-trickler");
        mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);

        // HTTP server
        httpd_config_t httpd_cfg = HTTPD_DEFAULT_CONFIG();
        httpd_cfg.max_uri_handlers  = 20;
        httpd_cfg.stack_size        = 8192;
        httpd_cfg.uri_match_fn      = httpd_uri_match_wildcard;

        if (httpd_start(&s_httpd, &httpd_cfg) == ESP_OK) {
            register_handlers(s_httpd);
            // Wildcard catch-all for SD file serving
            httpd_uri_t wildcard = {};
            wildcard.uri     = "/*";
            wildcard.method  = HTTP_GET;
            wildcard.handler = handle_default;
            httpd_register_uri_handler(s_httpd, &wildcard);
        }

        updateDisplayLog("Open http://robo-trickler.local in your browser", false);

        if (config.fwCheck) {
            // Get MAC address for tracking
            uint8_t mac[6];
            esp_wifi_get_mac(WIFI_IF_STA, mac);
            char mac_str[18];
            snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            std::string url = "https://strenuous.dev/roboTrickler/userTracker.php?mac=" + std::string(mac_str);
            makeHttpsGetRequest(url);
        }

        lv_obj_add_flag(ui_PanelMessages, LV_OBJ_FLAG_HIDDEN);

    } else {
        updateDisplayLog("No Wifi", false);
        messageBox("No Wifi", &lv_font_montserrat_14, lv_color_hex(0xFFFF00), true);
    }
}
