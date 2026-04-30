#include "web_server.h"

#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "app_api.h"
#include "board.h"
#include "display.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "mdns.h"
#include "nvs_flash.h"
#include "ota.h"
#include "storage.h"

static const char *TAG = "rt_web";
static const size_t RT_WEB_FILE_CHUNK_SIZE = 1024;
static httpd_handle_t s_server;
static bool s_wifi_active;
static int64_t s_last_reconnect_us;

static void url_decode(char *s)
{
    char *src = s;
    char *dst = s;
    while (*src) {
        if (src[0] == '%' && src[1] && src[2]) {
            char hex[3] = {src[1], src[2], 0};
            *dst++ = (char)strtol(hex, NULL, 16);
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

static bool is_safe_sd_path(const char *path)
{
    return path && path[0] == '/' && !strstr(path, "..");
}

static bool is_safe_uri_path(const char *path)
{
    return path && path[0] == '/' && !strstr(path, "..");
}

static esp_err_t sd_full_path(const char *sd_path, char *out, size_t out_size)
{
    if (!is_safe_sd_path(sd_path)) {
        return ESP_ERR_INVALID_ARG;
    }
    int written = snprintf(out, out_size, "%s%s", rt_storage_mount_point(), sd_path);
    return written < 0 || written >= (int)out_size ? ESP_ERR_NO_MEM : ESP_OK;
}

static bool first_query_value(httpd_req_t *req, const char *key, char *out, size_t out_size)
{
    out[0] = '\0';
    size_t qlen = httpd_req_get_url_query_len(req) + 1;
    if (qlen <= 1) {
        return false;
    }
    char *query = malloc(qlen);
    if (!query) {
        return false;
    }
    bool found = false;
    if (httpd_req_get_url_query_str(req, query, qlen) == ESP_OK) {
        if (key && httpd_query_key_value(query, key, out, out_size) == ESP_OK) {
            found = true;
        } else if (!key) {
            char *eq = strchr(query, '=');
            const char *value = eq ? eq + 1 : query;
            strlcpy(out, value, out_size);
            found = true;
        }
    }
    free(query);
    if (found) {
        url_decode(out);
    }
    return found;
}

static bool path_query_value(httpd_req_t *req, char *out, size_t out_size)
{
    return first_query_value(req, "path", out, out_size) || first_query_value(req, NULL, out, out_size);
}

static void json_escape_chunk(httpd_req_t *req, const char *text)
{
    char esc[8];
    for (const char *p = text; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') {
            esc[0] = '\\';
            esc[1] = (char)c;
            esc[2] = '\0';
            httpd_resp_sendstr_chunk(req, esc);
        } else if (c == '\n') {
            httpd_resp_sendstr_chunk(req, "\\n");
        } else if (c == '\r') {
            httpd_resp_sendstr_chunk(req, "\\r");
        } else if (c == '\t') {
            httpd_resp_sendstr_chunk(req, "\\t");
        } else if (c < 0x20) {
            snprintf(esc, sizeof(esc), "\\u%04x", c);
            httpd_resp_sendstr_chunk(req, esc);
        } else {
            char one[2] = {(char)c, 0};
            httpd_resp_sendstr_chunk(req, one);
        }
    }
}

static esp_err_t delete_recursive(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        return ESP_FAIL;
    }
    if (!S_ISDIR(st.st_mode)) {
        return unlink(path) == 0 ? ESP_OK : ESP_FAIL;
    }
    DIR *dir = opendir(path);
    if (!dir) {
        return ESP_FAIL;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char child[768];
        int written = snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        if (written < 0 || written >= (int)sizeof(child)) {
            closedir(dir);
            return ESP_ERR_NO_MEM;
        }
        esp_err_t ret = delete_recursive(child);
        if (ret != ESP_OK) {
            closedir(dir);
            return ret;
        }
    }
    closedir(dir);
    return rmdir(path) == 0 ? ESP_OK : ESP_FAIL;
}

static esp_err_t send_text(httpd_req_t *req, const char *type, const char *text)
{
    httpd_resp_set_type(req, type);
    return httpd_resp_sendstr(req, text);
}

static void query_value(httpd_req_t *req, const char *key, char *out, size_t out_size)
{
    out[0] = '\0';
    size_t qlen = httpd_req_get_url_query_len(req) + 1;
    if (qlen <= 1) {
        return;
    }
    char *query = malloc(qlen);
    if (!query) {
        return;
    }
    if (httpd_req_get_url_query_str(req, query, qlen) == ESP_OK) {
        httpd_query_key_value(query, key, out, out_size);
    }
    free(query);
}

static esp_err_t get_weight_handler(httpd_req_t *req)
{
    char body[32];
    snprintf(body, sizeof(body), "%.3f", rt_app_get_weight());
    return send_text(req, "text/plain", body);
}

static esp_err_t get_target_handler(httpd_req_t *req)
{
    char body[32];
    snprintf(body, sizeof(body), "%.3f", rt_app_get_target());
    return send_text(req, "text/plain", body);
}

static esp_err_t set_target_handler(httpd_req_t *req)
{
    char value[32];
    query_value(req, "targetWeight", value, sizeof(value));
    float target = strtof(value, NULL);
    if (target > 0.0f && target < RT_MAX_TARGET_WEIGHT) {
        rt_app_set_target(target);
    }
    return send_text(req, "text/html", "<h3>Value Set.</h3><br><button onClick='javascript:history.back()'>Back</button>");
}

static esp_err_t set_profile_handler(httpd_req_t *req)
{
    char value[16];
    query_value(req, "profileNumber", value, sizeof(value));
    rt_app_set_profile_index(atoi(value));
    return send_text(req, "text/html", "<h3>Value Set.</h3><br><button onClick='javascript:history.back()'>Back</button>");
}

static esp_err_t get_profile_handler(httpd_req_t *req)
{
    return send_text(req, "text/plain", rt_app_get_profile());
}

static esp_err_t get_language_handler(httpd_req_t *req)
{
    return send_text(req, "text/plain", rt_app_get_language());
}

static esp_err_t get_profile_list_handler(httpd_req_t *req)
{
    char names[RT_MAX_PROFILE_NAMES][32];
    uint8_t count = 0;
    rt_app_get_profile_list(names, &count);
    char body[1200] = "{";
    for (uint8_t i = 0; i < count; i++) {
        char item[48];
        snprintf(item, sizeof(item), "\"%u\":\"%s\"%s", i, names[i], i + 1 < count ? "," : "");
        strlcat(body, item, sizeof(body));
    }
    strlcat(body, "}", sizeof(body));
    return send_text(req, "application/json", body);
}

static esp_err_t start_handler(httpd_req_t *req)
{
    rt_app_start();
    return send_text(req, "text/html", "<h3>Running...</h3><br><button onClick='javascript:history.back()'>Back</button>");
}

static esp_err_t stop_handler(httpd_req_t *req)
{
    rt_app_stop();
    return send_text(req, "text/html", "<h3>Stopped...</h3><br><button onClick='javascript:history.back()'>Back</button>");
}

static esp_err_t reboot_handler(httpd_req_t *req)
{
    send_text(req, "text/html", "<h3>Reboot now.</h3><br><button onClick='javascript:history.back()'>Back</button>");
    esp_restart();
    return ESP_OK;
}

static const char *content_type(const char *path)
{
    const char *dot = strrchr(path, '.');
    if (!dot) {
        return "text/plain";
    }
    if (!strcasecmp(dot, ".html") || !strcasecmp(dot, ".htm")) return "text/html";
    if (!strcasecmp(dot, ".css")) return "text/css";
    if (!strcasecmp(dot, ".js")) return "application/javascript";
    if (!strcasecmp(dot, ".json")) return "application/json";
    if (!strcasecmp(dot, ".png")) return "image/png";
    if (!strcasecmp(dot, ".jpg") || !strcasecmp(dot, ".jpeg")) return "image/jpeg";
    if (!strcasecmp(dot, ".gif")) return "image/gif";
    if (!strcasecmp(dot, ".ico")) return "image/x-icon";
    if (!strcasecmp(dot, ".xml")) return "text/xml";
    if (!strcasecmp(dot, ".pdf")) return "application/pdf";
    if (!strcasecmp(dot, ".zip")) return "application/zip";
    if (!strcasecmp(dot, ".gz")) return "application/x-gzip";
    return "text/plain";
}

static esp_err_t list_handler(httpd_req_t *req)
{
    char sd_path[192];
    if (!first_query_value(req, "dir", sd_path, sizeof(sd_path))) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "BAD ARGS");
        return ESP_FAIL;
    }
    char full[768];
    if (sd_full_path(sd_path, full, sizeof(full)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "BAD PATH");
        return ESP_FAIL;
    }
    DIR *dir = opendir(full);
    if (!dir) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, errno == ENOTDIR ? "NOT DIR" : "BAD PATH");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/json");
    httpd_resp_sendstr_chunk(req, "[");
    bool first = true;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char child_sd[256];
        int written = strcmp(sd_path, "/") == 0
                          ? snprintf(child_sd, sizeof(child_sd), "/%s", entry->d_name)
                          : snprintf(child_sd, sizeof(child_sd), "%s/%s", sd_path, entry->d_name);
        if (written < 0 || written >= (int)sizeof(child_sd) || strcmp(child_sd, "/resources/css") == 0) {
            continue;
        }
        char child_full[768];
        struct stat st;
        if (sd_full_path(child_sd, child_full, sizeof(child_full)) != ESP_OK || stat(child_full, &st) != 0) {
            continue;
        }
        if (!first) {
            httpd_resp_sendstr_chunk(req, ",");
        }
        first = false;
        httpd_resp_sendstr_chunk(req, "{\"type\":\"");
        httpd_resp_sendstr_chunk(req, S_ISDIR(st.st_mode) ? "dir" : "file");
        httpd_resp_sendstr_chunk(req, "\",\"name\":\"");
        json_escape_chunk(req, child_sd);
        httpd_resp_sendstr_chunk(req, "\"}");
    }
    closedir(dir);
    httpd_resp_sendstr_chunk(req, "]");
    return httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t create_handler(httpd_req_t *req)
{
    char sd_path[192];
    if (!path_query_value(req, sd_path, sizeof(sd_path))) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "BAD ARGS");
        return ESP_FAIL;
    }
    char full[768];
    if (strcmp(sd_path, "/") == 0 || sd_full_path(sd_path, full, sizeof(full)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "BAD PATH");
        return ESP_FAIL;
    }
    struct stat st;
    if (stat(full, &st) == 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "BAD PATH");
        return ESP_FAIL;
    }
    if (strchr(sd_path, '.')) {
        int fd = open(full, O_CREAT | O_EXCL | O_WRONLY, 0666);
        if (fd < 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "CREATE FAILED");
            return ESP_FAIL;
        }
        close(fd);
    } else if (mkdir(full, 0777) != 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "CREATE FAILED");
        return ESP_FAIL;
    }
    return send_text(req, "text/plain", "");
}

static esp_err_t delete_handler(httpd_req_t *req)
{
    char sd_path[192];
    if (!path_query_value(req, sd_path, sizeof(sd_path))) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "BAD ARGS");
        return ESP_FAIL;
    }
    char full[768];
    if (strcmp(sd_path, "/") == 0 || sd_full_path(sd_path, full, sizeof(full)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "BAD PATH");
        return ESP_FAIL;
    }
    if (delete_recursive(full) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "BAD PATH");
        return ESP_FAIL;
    }
    return send_text(req, "text/plain", "");
}

static bool multipart_filename(const char *buf, int len, char *out, size_t out_size)
{
    const char *key = "filename=\"";
    char *start = memmem(buf, len, key, strlen(key));
    if (!start) {
        return false;
    }
    start += strlen(key);
    char *end = memchr(start, '"', (buf + len) - start);
    if (!end) {
        return false;
    }
    size_t copy = (size_t)(end - start);
    if (copy >= out_size) {
        copy = out_size - 1;
    }
    memcpy(out, start, copy);
    out[copy] = '\0';
    url_decode(out);
    return true;
}

static esp_err_t upload_file_handler(httpd_req_t *req)
{
    char sd_path[192];
    bool have_path = path_query_value(req, sd_path, sizeof(sd_path));
    char full[768] = {0};
    int fd = -1;
    char buf[2048];
    int remaining = req->content_len;
    bool skipped_header = false;

    while (remaining > 0) {
        int recv = httpd_req_recv(req, buf, remaining > (int)sizeof(buf) ? sizeof(buf) : remaining);
        if (recv <= 0) {
            if (fd >= 0) {
                close(fd);
                unlink(full);
            }
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Upload receive failed");
            return ESP_FAIL;
        }
        remaining -= recv;
        char *data = buf;
        int len = recv;
        if (!skipped_header && memmem(buf, recv, "Content-Disposition:", 20)) {
            if (!have_path) {
                have_path = multipart_filename(buf, recv, sd_path, sizeof(sd_path));
            }
            char *body = memmem(buf, recv, "\r\n\r\n", 4);
            if (!body) {
                continue;
            }
            body += 4;
            data = body;
            len -= (int)(body - buf);
            skipped_header = true;
        } else {
            skipped_header = true;
        }
        if (!have_path || !is_safe_sd_path(sd_path) || strcmp(sd_path, "/") == 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "BAD PATH");
            return ESP_FAIL;
        }
        if (fd < 0) {
            if (strncmp(sd_path, "/profiles/", 10) == 0) {
                char profiles[768];
                if (sd_full_path("/profiles", profiles, sizeof(profiles)) == ESP_OK) {
                    mkdir(profiles, 0777);
                }
            }
            if (sd_full_path(sd_path, full, sizeof(full)) != ESP_OK) {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "BAD PATH");
                return ESP_FAIL;
            }
            unlink(full);
            fd = open(full, O_CREAT | O_TRUNC | O_WRONLY, 0666);
            if (fd < 0) {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Open failed");
                return ESP_FAIL;
            }
        }
        if (remaining == 0) {
            char *tail = memmem(data, len, "\r\n--", 4);
            if (tail) {
                len = (int)(tail - data);
            }
        }
        if (len > 0 && write(fd, data, len) != len) {
            close(fd);
            unlink(full);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Write failed");
            return ESP_FAIL;
        }
    }
    if (fd >= 0) {
        close(fd);
    }
    return send_text(req, "text/plain", "");
}

static esp_err_t file_get_handler(httpd_req_t *req)
{
    char path[768];
    const char *uri = req->uri;
    if (strcmp(uri, "/") == 0) {
        uri = "/system/index.html";
    }
    char uri_path[256];
    const char *query = strchr(uri, '?');
    size_t uri_path_len = query ? (size_t)(query - uri) : strlen(uri);
    if (uri_path_len >= sizeof(uri_path)) {
        httpd_resp_send_err(req, HTTPD_414_URI_TOO_LONG, "Path too long");
        return ESP_FAIL;
    }
    memcpy(uri_path, uri, uri_path_len);
    uri_path[uri_path_len] = '\0';
    if (!is_safe_uri_path(uri_path)) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
        return ESP_FAIL;
    }
    int written = snprintf(path, sizeof(path), "%s%s", rt_storage_mount_point(), uri_path);
    if (written < 0 || written >= (int)sizeof(path)) {
        httpd_resp_send_err(req, HTTPD_414_URI_TOO_LONG, "Path too long");
        return ESP_FAIL;
    }
    struct stat st;
    bool gzipped = false;
    if (stat(path, &st) != 0) {
        size_t path_len = strlen(path);
        if (path_len + 3 >= sizeof(path)) {
            httpd_resp_send_err(req, HTTPD_414_URI_TOO_LONG, "Path too long");
            return ESP_FAIL;
        }
        memcpy(path + path_len, ".gz", 4);
        if (stat(path, &st) == 0) {
            gzipped = true;
        } else {
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
            return ESP_FAIL;
        }
    }
    if (S_ISDIR(st.st_mode)) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
        return ESP_FAIL;
    }
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Open failed");
        return ESP_FAIL;
    }
    esp_err_t ret = httpd_resp_set_type(req, content_type(gzipped ? uri_path : path));
    if (ret == ESP_OK && gzipped) {
        ret = httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    }
    if (ret != ESP_OK) {
        close(fd);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Response setup failed");
        return ret;
    }

    char *buf = malloc(RT_WEB_FILE_CHUNK_SIZE);
    if (!buf) {
        close(fd);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No memory");
        return ESP_ERR_NO_MEM;
    }
    ssize_t n;
    while ((n = read(fd, buf, RT_WEB_FILE_CHUNK_SIZE)) > 0) {
        ret = httpd_resp_send_chunk(req, buf, n);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "File send failed: %s", path);
            break;
        }
    }
    if (n < 0) {
        ESP_LOGE(TAG, "File read failed: %s", path);
        ret = ESP_FAIL;
    }
    free(buf);
    close(fd);
    if (ret != ESP_OK) {
        return ret;
    }
    return httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t fwupdate_page_handler(httpd_req_t *req)
{
    char page[256];
    snprintf(page, sizeof(page), "<form method='POST' action='/update' enctype='application/octet-stream'><p>FW-Version: %s</p><br><input type='file' name='update'><input type='submit' value='Update'></form><br><button onClick='javascript:history.back()'>Back</button>", RT_FW_VERSION);
    return send_text(req, "text/html", page);
}

static esp_err_t update_post_handler(httpd_req_t *req)
{
    esp_err_t ret = rt_ota_upload_begin(req->content_len);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, rt_ota_last_error());
        return ret;
    }
    char buf[2048];
    int remaining = req->content_len;
    bool skipped_multipart_header = false;
    while (remaining > 0) {
        int recv = httpd_req_recv(req, buf, remaining > (int)sizeof(buf) ? sizeof(buf) : remaining);
        if (recv <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Upload receive failed");
            return ESP_FAIL;
        }
        remaining -= recv;
        char *data = buf;
        int len = recv;
        if (!skipped_multipart_header) {
            char *body = memmem(buf, recv, "\r\n\r\n", 4);
            if (body) {
                body += 4;
                len -= (int)(body - buf);
                data = body;
                skipped_multipart_header = true;
            } else if (memmem(buf, recv, "Content-Disposition:", 20)) {
                continue;
            } else {
                skipped_multipart_header = true;
            }
        }
        if (remaining == 0) {
            char *tail = memmem(data, len, "\r\n--", 4);
            if (tail) {
                len = (int)(tail - data);
            }
        }
        if (len > 0) {
            ret = rt_ota_upload_write(data, len);
            if (ret != ESP_OK) {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, rt_ota_last_error());
                return ret;
            }
        }
    }
    ret = rt_ota_upload_finish();
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, rt_ota_last_error());
        return ret;
    }
    send_text(req, "text/html", "<h3>OK</h3><br><br><input type='button' value='Back' onClick='javascript:history.back()'>");
    esp_restart();
    return ESP_OK;
}

static esp_err_t register_uri(httpd_handle_t server, const char *uri, httpd_method_t method, esp_err_t (*handler)(httpd_req_t *))
{
    httpd_uri_t h = {
        .uri = uri,
        .method = method,
        .handler = handler,
    };
    return httpd_register_uri_handler(server, &h);
}

static esp_err_t wifi_connect(const rt_config_t *config)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "nvs erase failed");
        ret = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "nvs init failed");
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta = esp_netif_create_default_wifi_sta();
    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&wifi_cfg), TAG, "wifi init failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "wifi mode failed");
    if (config->ip_static[0]) {
        esp_netif_dhcpc_stop(sta);
        esp_netif_ip_info_t ip = {0};
        ip.ip.addr = esp_ip4addr_aton(config->ip_static);
        ip.gw.addr = esp_ip4addr_aton(config->ip_gateway);
        ip.netmask.addr = esp_ip4addr_aton(config->ip_subnet);
        esp_netif_set_ip_info(sta, &ip);
    }
    uint32_t dns_addr = esp_ip4addr_aton(config->ip_dns[0] ? config->ip_dns : "8.8.8.8");
    if (dns_addr) {
        esp_netif_dns_info_t dns = {0};
        dns.ip.type = ESP_IPADDR_TYPE_V4;
        dns.ip.u_addr.ip4.addr = dns_addr;
        esp_netif_set_dns_info(sta, ESP_NETIF_DNS_MAIN, &dns);
    }
    wifi_config_t sta_cfg = {0};
    strlcpy((char *)sta_cfg.sta.ssid, config->wifi_ssid, sizeof(sta_cfg.sta.ssid));
    strlcpy((char *)sta_cfg.sta.password, config->wifi_psk, sizeof(sta_cfg.sta.password));
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg), TAG, "wifi config failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start failed");
    ESP_RETURN_ON_ERROR(esp_wifi_connect(), TAG, "wifi connect failed");
    return ESP_OK;
}

esp_err_t rt_web_start(const rt_config_t *config)
{
    if (!config->wifi_ssid[0]) {
        return ESP_ERR_INVALID_STATE;
    }
    ESP_RETURN_ON_ERROR(wifi_connect(config), TAG, "wifi setup failed");

    httpd_config_t http_cfg = HTTPD_DEFAULT_CONFIG();
    http_cfg.uri_match_fn = httpd_uri_match_wildcard;
    http_cfg.max_uri_handlers = 24;
    ESP_RETURN_ON_ERROR(httpd_start(&s_server, &http_cfg), TAG, "httpd start failed");
    register_uri(s_server, "/getWeight", HTTP_GET, get_weight_handler);
    register_uri(s_server, "/getTarget", HTTP_GET, get_target_handler);
    register_uri(s_server, "/setTarget", HTTP_GET, set_target_handler);
    register_uri(s_server, "/setProfile", HTTP_GET, set_profile_handler);
    register_uri(s_server, "/getProfile", HTTP_GET, get_profile_handler);
    register_uri(s_server, "/getLanguage", HTTP_GET, get_language_handler);
    register_uri(s_server, "/getProfileList", HTTP_GET, get_profile_list_handler);
    register_uri(s_server, "/system/start", HTTP_GET, start_handler);
    register_uri(s_server, "/system/stop", HTTP_GET, stop_handler);
    register_uri(s_server, "/reboot", HTTP_GET, reboot_handler);
    register_uri(s_server, "/fwupdate", HTTP_GET, fwupdate_page_handler);
    register_uri(s_server, "/update", HTTP_POST, update_post_handler);
    register_uri(s_server, "/list", HTTP_GET, list_handler);
    register_uri(s_server, "/system/resources/edit", HTTP_DELETE, delete_handler);
    register_uri(s_server, "/system/resources/edit", HTTP_PUT, create_handler);
    register_uri(s_server, "/system/resources/edit", HTTP_POST, upload_file_handler);
    register_uri(s_server, "/*", HTTP_GET, file_get_handler);

    mdns_init();
    mdns_hostname_set("robo-trickler");
    mdns_instance_name_set("Robo-Trickler");
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    s_wifi_active = true;
    return ESP_OK;
}

bool rt_web_is_active(void)
{
    return s_wifi_active;
}

void rt_web_poll_reconnect(const rt_config_t *config)
{
    (void)config;
    if (!s_wifi_active) {
        return;
    }
    int64_t now = esp_timer_get_time();
    if (now - s_last_reconnect_us > 10000000LL) {
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK) {
            esp_wifi_connect();
        }
        s_last_reconnect_us = now;
    }
}
