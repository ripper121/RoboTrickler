#include "webserver.h"

#include <ctype.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "mdns.h"
#include "trickle.h"
#include "ui.h"

#define FILE_PATH_MAX           (ESP_VFS_PATH_MAX + 64)
#define QUERY_BUFSIZE           (CONFIG_HTTPD_MAX_URI_LEN + 1U)
#define WEB_HEADER_VALUE_SIZE   128
#define WEB_SCRATCH_SIZE        4096
#define WEB_MDNS_HOSTNAME       "robo-trickler"
#define WEB_MDNS_INSTANCE       "Robo Trickler Web Portal"
#define WEB_STATIC_CACHE_HEADER "public, no-cache"

typedef struct {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[WEB_SCRATCH_SIZE];
} webserver_ctx_t;

static const char *TAG = "webserver";

static httpd_handle_t s_server = NULL;
static webserver_ctx_t s_ctx;
static bool s_mdns_started = false;

static esp_err_t webserver_start_mdns(uint16_t port)
{
    mdns_txt_item_t service_txt[] = {
        {"path", "/"},
    };
    esp_err_t ret = mdns_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "mdns_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = mdns_hostname_set(WEB_MDNS_HOSTNAME);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "mdns_hostname_set failed: %s", esp_err_to_name(ret));
        mdns_free();
        return ret;
    }

    ret = mdns_instance_name_set(WEB_MDNS_INSTANCE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "mdns_instance_name_set failed: %s", esp_err_to_name(ret));
        mdns_free();
        return ret;
    }

    ret = mdns_service_add("RoboTrickler-Web", "_http", "_tcp", port, service_txt,
                           sizeof(service_txt) / sizeof(service_txt[0]));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "mdns_service_add failed: %s", esp_err_to_name(ret));
        mdns_free();
        return ret;
    }

    s_mdns_started = true;
    ESP_LOGI(TAG, "mDNS ready at http://%s.local:%u", WEB_MDNS_HOSTNAME, port);
    return ESP_OK;
}

static void webserver_stop_mdns(void)
{
    if (!s_mdns_started) {
        return;
    }

    mdns_free();
    s_mdns_started = false;
}

static bool has_file_ext(const char *filename, const char *ext)
{
    const char *dot = NULL;

    if (filename == NULL || ext == NULL) {
        return false;
    }

    dot = strrchr(filename, '.');
    return dot != NULL && strcasecmp(dot, ext) == 0;
}

static bool uri_has_parent_ref(const char *uri)
{
    return uri != NULL && strstr(uri, "..") != NULL;
}

static bool path_has_unsafe_ref(const char *path)
{
    return path != NULL &&
           (strstr(path, "..") != NULL || strchr(path, '\\') != NULL || strchr(path, ':') != NULL);
}

static int hex_to_int(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }

    c = (char)tolower((unsigned char)c);
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }

    return -1;
}

static bool url_decode_inplace(char *text)
{
    char *src = text;
    char *dst = text;

    if (text == NULL) {
        return false;
    }

    while (*src != '\0') {
        if (*src == '%') {
            int hi = -1;
            int lo = -1;

            if (src[1] == '\0' || src[2] == '\0') {
                return false;
            }

            hi = hex_to_int(src[1]);
            lo = hex_to_int(src[2]);
            if (hi < 0 || lo < 0) {
                return false;
            }

            *dst++ = (char)((hi << 4) | lo);
            src += 3;
            continue;
        }

        if (*src == '+') {
            *dst++ = ' ';
            src++;
            continue;
        }

        *dst++ = *src++;
    }

    *dst = '\0';
    return true;
}

static esp_err_t get_decoded_query_value(httpd_req_t *req,
                                         const char *key,
                                         bool required,
                                         char *value_out,
                                         size_t value_out_size)
{
    char query[QUERY_BUFSIZE];
    size_t query_len = 0U;

    if (req == NULL || key == NULL || value_out == NULL || value_out_size == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    value_out[0] = '\0';

    query_len = httpd_req_get_url_query_len(req);
    if (query_len == 0U) {
        return required ? ESP_ERR_NOT_FOUND : ESP_OK;
    }
    if (query_len >= sizeof(query)) {
        return ESP_ERR_INVALID_SIZE;
    }

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        return ESP_FAIL;
    }

    if (httpd_query_key_value(query, key, value_out, value_out_size) != ESP_OK) {
        value_out[0] = '\0';
        return required ? ESP_ERR_NOT_FOUND : ESP_OK;
    }

    if (!url_decode_inplace(value_out)) {
        value_out[0] = '\0';
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static esp_err_t sanitize_relative_path(const char *input, char *output, size_t output_size)
{
    const char *src = input;
    size_t out_len = 0U;

    if (src == NULL || output == NULL || output_size == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    output[0] = '\0';

    while (*src == '/') {
        src++;
    }

    if (*src == '\0') {
        return ESP_OK;
    }

    if (path_has_unsafe_ref(src)) {
        return ESP_ERR_INVALID_ARG;
    }

    while (*src != '\0' && out_len + 1U < output_size) {
        if (*src == '/' && out_len > 0U && output[out_len - 1U] == '/') {
            src++;
            continue;
        }

        output[out_len++] = *src++;
    }

    if (*src != '\0') {
        return ESP_ERR_INVALID_SIZE;
    }

    while (out_len > 0U && output[out_len - 1U] == '/') {
        out_len--;
    }

    output[out_len] = '\0';
    return ESP_OK;
}

static esp_err_t build_sd_path_from_relative(const char *relative_path,
                                             bool allow_empty_relative,
                                             char *path_out,
                                             size_t path_out_size)
{
    char cleaned[FILE_PATH_MAX];
    int written = 0;

    if (path_out == NULL || path_out_size == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    if (relative_path == NULL || relative_path[0] == '\0') {
        if (!allow_empty_relative) {
            return ESP_ERR_INVALID_ARG;
        }

        written = snprintf(path_out, path_out_size, "%s", s_ctx.base_path);
        return (written < 0 || (size_t)written >= path_out_size) ? ESP_ERR_INVALID_SIZE : ESP_OK;
    }

    if (sanitize_relative_path(relative_path, cleaned, sizeof(cleaned)) != ESP_OK) {
        return ESP_ERR_INVALID_ARG;
    }

    if (cleaned[0] == '\0') {
        if (!allow_empty_relative) {
            return ESP_ERR_INVALID_ARG;
        }

        written = snprintf(path_out, path_out_size, "%s", s_ctx.base_path);
    } else {
        written = snprintf(path_out, path_out_size, "%s/%s", s_ctx.base_path, cleaned);
    }

    return (written < 0 || (size_t)written >= path_out_size) ? ESP_ERR_INVALID_SIZE : ESP_OK;
}

static esp_err_t canonicalize_static_uri_path(char *uri_path, size_t uri_path_size)
{
    char original_path[FILE_PATH_MAX];
    int written = 0;

    if (uri_path == NULL || uri_path_size == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    if (strcmp(uri_path, "/") == 0 || strcmp(uri_path, "/index.html") == 0) {
        written = snprintf(uri_path, uri_path_size, "/web/index.html");
    } else if (strcmp(uri_path, "/explorer.html") == 0) {
        written = snprintf(uri_path, uri_path_size, "/web/explorer.html");
    } else if (strcmp(uri_path, "/editor.html") == 0) {
        written = snprintf(uri_path, uri_path_size, "/web/editor.html");
    } else if (strcmp(uri_path, "/trickle.html") == 0) {
        written = snprintf(uri_path, uri_path_size, "/web/trickle.html");
    } else if (strcmp(uri_path, "/i18n.js") == 0) {
        written = snprintf(uri_path, uri_path_size, "/web/i18n.js");
    } else if (strcmp(uri_path, "/favicon.ico") == 0) {
        written = snprintf(uri_path, uri_path_size, "/web/favicon.ico");
    } else if (strcmp(uri_path, "/ace") == 0 || strncmp(uri_path, "/ace/", 5) == 0 ||
               strcmp(uri_path, "/web-lang") == 0 || strncmp(uri_path, "/web-lang/", 10) == 0) {
        const size_t original_len = strlen(uri_path);

        if (original_len + 1U > sizeof(original_path)) {
            return ESP_ERR_INVALID_SIZE;
        }

        memcpy(original_path, uri_path, original_len + 1U);
        written = snprintf(uri_path, uri_path_size, "/web%s", original_path);
    } else {
        return ESP_OK;
    }

    return (written < 0 || (size_t)written >= uri_path_size) ? ESP_ERR_INVALID_SIZE : ESP_OK;
}

static esp_err_t build_fs_path(const char *uri,
                               char *filepath,
                               size_t filepath_size,
                               char *uri_path_out,
                               size_t uri_path_out_size)
{
    const char *qmark = NULL;
    const char *hash = NULL;
    size_t uri_len = 0U;
    esp_err_t ret = ESP_OK;
    int written = 0;

    if (uri == NULL || filepath == NULL || filepath_size == 0U ||
        uri_path_out == NULL || uri_path_out_size == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    uri_len = strlen(uri);
    qmark = strchr(uri, '?');
    hash = strchr(uri, '#');
    if (qmark != NULL) {
        uri_len = (size_t)(qmark - uri);
    }
    if (hash != NULL && (size_t)(hash - uri) < uri_len) {
        uri_len = (size_t)(hash - uri);
    }

    if (uri_len == 0U || uri[0] != '/' || uri_len + 1U > uri_path_out_size) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(uri_path_out, uri, uri_len);
    uri_path_out[uri_len] = '\0';

    if (uri_has_parent_ref(uri_path_out)) {
        return ESP_ERR_INVALID_ARG;
    }

    ret = canonicalize_static_uri_path(uri_path_out, uri_path_out_size);
    if (ret != ESP_OK) {
        return ret;
    }

    written = snprintf(filepath, filepath_size, "%s%s", s_ctx.base_path, uri_path_out);
    return (written < 0 || (size_t)written >= filepath_size) ? ESP_ERR_INVALID_SIZE : ESP_OK;
}

static bool request_accepts_gzip(httpd_req_t *req)
{
    char value[64] = {0};

    if (req == NULL) {
        return false;
    }

    if (httpd_req_get_hdr_value_str(req, "Accept-Encoding", value, sizeof(value)) != ESP_OK) {
        return false;
    }

    for (size_t i = 0; value[i] != '\0'; i++) {
        value[i] = (char)tolower((unsigned char)value[i]);
    }

    return strstr(value, "gzip") != NULL;
}

static const char *http_status_text(int status_code)
{
    switch (status_code) {
    case 200:
        return "200 OK";
    case 400:
        return "400 Bad Request";
    case 404:
        return "404 Not Found";
    case 409:
        return "409 Conflict";
    case 500:
        return "500 Internal Server Error";
    default:
        return "500 Internal Server Error";
    }
}

static esp_err_t build_gzip_variant_path(const char *full_path, char *gzip_path, size_t gzip_path_size)
{
    int written = 0;

    if (full_path == NULL || gzip_path == NULL || gzip_path_size == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    written = snprintf(gzip_path, gzip_path_size, "%s.gz", full_path);
    return (written < 0 || (size_t)written >= gzip_path_size) ? ESP_ERR_INVALID_SIZE : ESP_OK;
}

static esp_err_t build_file_etag(const struct stat *st, char *etag, size_t etag_size)
{
    int written = 0;

    if (st == NULL || etag == NULL || etag_size == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    written = snprintf(etag,
                       etag_size,
                       "\"%llx-%llx\"",
                       (unsigned long long)st->st_mtime,
                       (unsigned long long)st->st_size);
    return (written < 0 || (size_t)written >= etag_size) ? ESP_ERR_INVALID_SIZE : ESP_OK;
}

static bool request_etag_matches(httpd_req_t *req, const char *etag)
{
    char value[WEB_HEADER_VALUE_SIZE] = {0};

    if (req == NULL || etag == NULL) {
        return false;
    }

    if (httpd_req_get_hdr_value_str(req, "If-None-Match", value, sizeof(value)) != ESP_OK) {
        return false;
    }

    return strstr(value, etag) != NULL;
}

static esp_err_t send_json(httpd_req_t *req, int status_code, const char *json)
{
    httpd_resp_set_status(req, http_status_text(status_code));

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, json);
}

static esp_err_t send_json_object(httpd_req_t *req, int status_code, cJSON *root)
{
    char *json = NULL;
    esp_err_t ret = ESP_FAIL;

    if (root == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

    json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

    ret = send_json(req, status_code, json);
    cJSON_free(json);
    return ret;
}

static int web_status_from_err(esp_err_t ret)
{
    if (ret == ESP_ERR_NOT_FOUND) {
        return 404;
    }
    if (ret == ESP_ERR_INVALID_ARG) {
        return 400;
    }
    if (ret == ESP_ERR_INVALID_STATE) {
        return 409;
    }

    return 500;
}

static esp_err_t send_simple_status(httpd_req_t *req, int status_code, const char *status, const char *message)
{
    cJSON *root = NULL;
    char *json = NULL;
    esp_err_t ret = ESP_FAIL;

    root = cJSON_CreateObject();
    if (root == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

    if (!cJSON_AddStringToObject(root, "status", status) ||
        (message != NULL && !cJSON_AddStringToObject(root, "message", message))) {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

    json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

    ret = send_json(req, status_code, json);
    cJSON_free(json);
    return ret;
}

static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename)
{
    if (has_file_ext(filename, ".html") || has_file_ext(filename, ".htm")) {
        return httpd_resp_set_type(req, "text/html; charset=utf-8");
    }
    if (has_file_ext(filename, ".css")) {
        return httpd_resp_set_type(req, "text/css; charset=utf-8");
    }
    if (has_file_ext(filename, ".js") || has_file_ext(filename, ".mjs")) {
        return httpd_resp_set_type(req, "application/javascript; charset=utf-8");
    }
    if (has_file_ext(filename, ".json")) {
        return httpd_resp_set_type(req, "application/json; charset=utf-8");
    }
    if (has_file_ext(filename, ".xml")) {
        return httpd_resp_set_type(req, "application/xml; charset=utf-8");
    }
    if (has_file_ext(filename, ".txt") || has_file_ext(filename, ".md") || has_file_ext(filename, ".log")) {
        return httpd_resp_set_type(req, "text/plain; charset=utf-8");
    }
    if (has_file_ext(filename, ".svg")) {
        return httpd_resp_set_type(req, "image/svg+xml");
    }
    if (has_file_ext(filename, ".png")) {
        return httpd_resp_set_type(req, "image/png");
    }
    if (has_file_ext(filename, ".jpg") || has_file_ext(filename, ".jpeg")) {
        return httpd_resp_set_type(req, "image/jpeg");
    }
    if (has_file_ext(filename, ".ico")) {
        return httpd_resp_set_type(req, "image/x-icon");
    }

    return httpd_resp_set_type(req, "application/octet-stream");
}

static esp_err_t send_file_response(httpd_req_t *req,
                                    const char *filepath,
                                    const char *content_type_path,
                                    bool attachment,
                                    bool cacheable)
{
    FILE *fd = NULL;
    struct stat st = {0};
    const char *filename = NULL;
    char disposition[96];
    char etag[40];
    size_t chunksize = 0U;

    if (stat(filepath, &st) != 0 || !S_ISREG(st.st_mode)) {
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
    }

    ESP_LOGI(TAG, "HTTP URI request: %s -> responding from SD path: %s", req->uri, filepath);

    set_content_type_from_file(req, content_type_path != NULL ? content_type_path : filepath);

    if (attachment) {
        filename = strrchr(content_type_path != NULL ? content_type_path : filepath, '/');
        filename = (filename != NULL) ? (filename + 1) : (content_type_path != NULL ? content_type_path : filepath);
        snprintf(disposition, sizeof(disposition), "attachment; filename=\"%s\"", filename);
        httpd_resp_set_hdr(req, "Content-Disposition", disposition);
        httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    } else if (cacheable) {
        httpd_resp_set_hdr(req, "Cache-Control", WEB_STATIC_CACHE_HEADER);

        if (build_file_etag(&st, etag, sizeof(etag)) == ESP_OK) {
            httpd_resp_set_hdr(req, "ETag", etag);
            if (request_etag_matches(req, etag)) {
                httpd_resp_set_status(req, "304 Not Modified");
                return httpd_resp_send(req, NULL, 0);
            }
        }
    } else {
        httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    }

    fd = fopen(filepath, "rb");
    if (fd == NULL) {
        ESP_LOGE(TAG, "Failed to open %s", filepath);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open file");
    }

    do {
        chunksize = fread(s_ctx.scratch, 1, sizeof(s_ctx.scratch), fd);
        if (chunksize > 0U && httpd_resp_send_chunk(req, s_ctx.scratch, chunksize) != ESP_OK) {
            fclose(fd);
            httpd_resp_sendstr_chunk(req, NULL);
            return ESP_FAIL;
        }
    } while (chunksize > 0U);

    if (ferror(fd)) {
        fclose(fd);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read file");
    }

    fclose(fd);
    return httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t web_static_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    char uri_path[FILE_PATH_MAX];
    char gzip_path[FILE_PATH_MAX];
    struct stat st = {0};
    const bool accepts_gzip = request_accepts_gzip(req);
    esp_err_t ret = ESP_OK;

    ret = build_fs_path(req->uri, filepath, sizeof(filepath), uri_path, sizeof(uri_path));
    if (ret != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid path");
    }

    if (stat(filepath, &st) == 0 && S_ISDIR(st.st_mode)) {
        const size_t len = strlen(filepath);
        if (len + strlen("/index.html") + 1U > sizeof(filepath)) {
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Path too long");
        }
        snprintf(filepath + len, sizeof(filepath) - len, "/index.html");
    }

    if (accepts_gzip &&
        build_gzip_variant_path(filepath, gzip_path, sizeof(gzip_path)) == ESP_OK &&
        stat(gzip_path, &st) == 0 &&
        S_ISREG(st.st_mode)) {
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        httpd_resp_set_hdr(req, "Vary", "Accept-Encoding");
        return send_file_response(req, gzip_path, filepath, false, true);
    }

    return send_file_response(req, filepath, filepath, false, true);
}

static esp_err_t web_api_list_handler(httpd_req_t *req)
{
    char relative_path[FILE_PATH_MAX] = {0};
    char full_path[FILE_PATH_MAX];
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    cJSON *root = NULL;
    cJSON *files = NULL;
    char *json = NULL;
    esp_err_t ret = ESP_OK;

    ret = get_decoded_query_value(req, "path", false, relative_path, sizeof(relative_path));
    if (ret != ESP_OK) {
        return send_simple_status(req, 400, "error", "Invalid path");
    }

    ret = build_sd_path_from_relative(relative_path, true, full_path, sizeof(full_path));
    if (ret != ESP_OK) {
        return send_simple_status(req, ret == ESP_ERR_INVALID_ARG ? 400 : 500, "error", "Invalid path");
    }

    ESP_LOGD(TAG, "Requested list path: %s", full_path);
    dir = opendir(full_path);
    if (dir == NULL) {
        return send_simple_status(req, 404, "error", "Directory not found");
    }

    root = cJSON_CreateObject();
    files = cJSON_CreateArray();
    if (root == NULL || files == NULL ||
        !cJSON_AddStringToObject(root, "status", "ok") ||
        !cJSON_AddItemToObject(root, "files", files)) {
        closedir(dir);
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

    while ((entry = readdir(dir)) != NULL) {
        char child_rel[FILE_PATH_MAX];
        char child_full[FILE_PATH_MAX];
        struct stat st = {0};
        cJSON *item = NULL;
        const char *type = "file";

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (relative_path[0] == '\0') {
            const size_t name_len = strlen(entry->d_name);

            if (name_len + 1U > sizeof(child_rel)) {
                continue;
            }

            memcpy(child_rel, entry->d_name, name_len + 1U);
        } else {
            const size_t rel_len = strlen(relative_path);
            const size_t name_len = strlen(entry->d_name);

            if (rel_len + 1U + name_len + 1U > sizeof(child_rel)) {
                continue;
            }

            memcpy(child_rel, relative_path, rel_len);
            child_rel[rel_len] = '/';
            memcpy(child_rel + rel_len + 1U, entry->d_name, name_len + 1U);
        }

        if (build_sd_path_from_relative(child_rel, false, child_full, sizeof(child_full)) != ESP_OK) {
            continue;
        }
        if (stat(child_full, &st) != 0) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            type = "dir";
        }

        item = cJSON_CreateObject();
        if (item == NULL ||
            !cJSON_AddStringToObject(item, "name", entry->d_name) ||
            !cJSON_AddStringToObject(item, "path", child_rel) ||
            !cJSON_AddStringToObject(item, "type", type) ||
            !cJSON_AddNumberToObject(item, "size", S_ISDIR(st.st_mode) ? 0 : (double)st.st_size) ||
            !cJSON_AddItemToArray(files, item)) {
            cJSON_Delete(item);
            closedir(dir);
            cJSON_Delete(root);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        }
    }

    closedir(dir);
    json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json == NULL) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

    ret = send_json(req, 200, json);
    cJSON_free(json);
    return ret;
}

static esp_err_t web_api_profiles_handler(httpd_req_t *req)
{
    char active_profile[64] = {0};
    cJSON *root = NULL;
    cJSON *profiles = NULL;
    esp_err_t ret = ESP_OK;

    ret = ui_reload_profiles();
    if (ret != ESP_OK) {
        return send_simple_status(req, web_status_from_err(ret), "error", "Failed to reload profiles");
    }

    ret = ui_get_active_profile_name(active_profile, sizeof(active_profile));
    if (ret != ESP_OK) {
        return send_simple_status(req, web_status_from_err(ret), "error", "Failed to get active profile");
    }

    root = cJSON_CreateObject();
    profiles = cJSON_CreateArray();
    if (root == NULL || profiles == NULL ||
        !cJSON_AddStringToObject(root, "status", "ok") ||
        !cJSON_AddStringToObject(root, "activeProfile", active_profile) ||
        !cJSON_AddItemToObject(root, "profiles", profiles)) {
        cJSON_Delete(root);
        if (root == NULL) {
            cJSON_Delete(profiles);
        }
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

    for (size_t i = 0; i < ui_get_profile_count(); ++i) {
        const char *profile_name = ui_get_profile_name_by_index(i);
        cJSON *profile = cJSON_CreateObject();

        if (profile == NULL ||
            !cJSON_AddStringToObject(profile, "name", profile_name) ||
            !cJSON_AddBoolToObject(profile, "selected", strcmp(profile_name, active_profile) == 0) ||
            !cJSON_AddItemToArray(profiles, profile)) {
            cJSON_Delete(profile);
            cJSON_Delete(root);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        }
    }

    return send_json_object(req, 200, root);
}

static esp_err_t web_api_profile_select_handler(httpd_req_t *req)
{
    char profile_name[64] = {0};
    char active_profile[64] = {0};
    double target_weight = 0.0;
    cJSON *root = NULL;
    esp_err_t ret = ESP_OK;

    ret = get_decoded_query_value(req, "name", true, profile_name, sizeof(profile_name));
    if (ret != ESP_OK) {
        return send_simple_status(req, 400, "error", "Missing profile name");
    }

    ret = ui_select_profile(profile_name);
    if (ret != ESP_OK) {
        return send_simple_status(req, web_status_from_err(ret), "error", "Failed to select profile");
    }

    ret = ui_get_active_profile_name(active_profile, sizeof(active_profile));
    if (ret != ESP_OK) {
        return send_simple_status(req, web_status_from_err(ret), "error", "Failed to read active profile");
    }

    ret = ui_get_target_weight(&target_weight);
    if (ret != ESP_OK) {
        return send_simple_status(req, web_status_from_err(ret), "error", "Failed to read target weight");
    }

    root = cJSON_CreateObject();
    if (root == NULL ||
        !cJSON_AddStringToObject(root, "status", "ok") ||
        !cJSON_AddStringToObject(root, "profile", active_profile) ||
        !cJSON_AddNumberToObject(root, "targetWeight", target_weight)) {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

    return send_json_object(req, 200, root);
}

static esp_err_t web_api_trickle_start_handler(httpd_req_t *req)
{
    char active_profile[64] = {0};
    bool running = false;
    cJSON *root = NULL;
    esp_err_t ret = ESP_OK;

    ret = ui_start_trickle();
    if (ret != ESP_OK) {
        return send_simple_status(req, web_status_from_err(ret), "error", "Failed to start trickle");
    }

    ret = ui_get_trickle_running(&running);
    if (ret != ESP_OK) {
        return send_simple_status(req, web_status_from_err(ret), "error", "Failed to read trickle state");
    }

    ret = ui_get_active_profile_name(active_profile, sizeof(active_profile));
    if (ret != ESP_OK) {
        return send_simple_status(req, web_status_from_err(ret), "error", "Failed to read active profile");
    }

    root = cJSON_CreateObject();
    if (root == NULL ||
        !cJSON_AddStringToObject(root, "status", "ok") ||
        !cJSON_AddStringToObject(root, "profile", active_profile) ||
        !cJSON_AddBoolToObject(root, "trickleRunning", running)) {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

    return send_json_object(req, 200, root);
}

static esp_err_t web_api_trickle_stop_handler(httpd_req_t *req)
{
    bool running = false;
    cJSON *root = NULL;
    esp_err_t ret = ESP_OK;

    ret = ui_stop_trickle_request();
    if (ret != ESP_OK) {
        return send_simple_status(req, web_status_from_err(ret), "error", "Failed to stop trickle");
    }

    ret = ui_get_trickle_running(&running);
    if (ret != ESP_OK) {
        return send_simple_status(req, web_status_from_err(ret), "error", "Failed to read trickle state");
    }

    root = cJSON_CreateObject();
    if (root == NULL ||
        !cJSON_AddStringToObject(root, "status", "ok") ||
        !cJSON_AddBoolToObject(root, "trickleRunning", running)) {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

    return send_json_object(req, 200, root);
}

static esp_err_t web_api_target_weight_handler(httpd_req_t *req)
{
    char active_profile[64] = {0};
    bool running = false;
    double target_weight = 0.0;
    cJSON *root = NULL;
    esp_err_t ret = ESP_OK;

    ret = ui_get_target_weight(&target_weight);
    if (ret != ESP_OK) {
        return send_simple_status(req, web_status_from_err(ret), "error", "Failed to read target weight");
    }

    ret = ui_get_trickle_running(&running);
    if (ret != ESP_OK) {
        return send_simple_status(req, web_status_from_err(ret), "error", "Failed to read trickle state");
    }

    ret = ui_get_active_profile_name(active_profile, sizeof(active_profile));
    if (ret != ESP_OK) {
        return send_simple_status(req, web_status_from_err(ret), "error", "Failed to read active profile");
    }

    root = cJSON_CreateObject();
    if (root == NULL ||
        !cJSON_AddStringToObject(root, "status", "ok") ||
        !cJSON_AddStringToObject(root, "profile", active_profile) ||
        !cJSON_AddNumberToObject(root, "targetWeight", target_weight) ||
        !cJSON_AddBoolToObject(root, "trickleRunning", running)) {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

    return send_json_object(req, 200, root);
}

static esp_err_t web_api_target_weight_set_handler(httpd_req_t *req)
{
    char active_profile[64] = {0};
    char value_text[32] = {0};
    char *end_ptr = NULL;
    double target_weight = 0.0;
    cJSON *root = NULL;
    esp_err_t ret = ESP_OK;

    ret = get_decoded_query_value(req, "value", true, value_text, sizeof(value_text));
    if (ret != ESP_OK) {
        return send_simple_status(req, 400, "error", "Missing target value");
    }

    target_weight = strtod(value_text, &end_ptr);
    if (end_ptr == value_text || (end_ptr != NULL && *end_ptr != '\0')) {
        return send_simple_status(req, 400, "error", "Invalid target value");
    }

    ret = ui_set_target_weight(target_weight);
    if (ret != ESP_OK) {
        return send_simple_status(req, web_status_from_err(ret), "error", "Failed to set target weight");
    }

    ret = ui_get_target_weight(&target_weight);
    if (ret != ESP_OK) {
        return send_simple_status(req, web_status_from_err(ret), "error", "Failed to read target weight");
    }

    ret = ui_get_active_profile_name(active_profile, sizeof(active_profile));
    if (ret != ESP_OK) {
        return send_simple_status(req, web_status_from_err(ret), "error", "Failed to read active profile");
    }

    root = cJSON_CreateObject();
    if (root == NULL ||
        !cJSON_AddStringToObject(root, "status", "ok") ||
        !cJSON_AddStringToObject(root, "profile", active_profile) ||
        !cJSON_AddNumberToObject(root, "targetWeight", target_weight)) {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

    return send_json_object(req, 200, root);
}

static esp_err_t web_api_scale_tare_handler(httpd_req_t *req)
{
    bool running = false;
    cJSON *root = NULL;
    esp_err_t ret = ESP_OK;

    ret = ui_tare_scale();
    if (ret != ESP_OK) {
        return send_simple_status(req, web_status_from_err(ret), "error", "Failed to tare scale");
    }

    ret = ui_get_trickle_running(&running);
    if (ret != ESP_OK) {
        return send_simple_status(req, web_status_from_err(ret), "error", "Failed to read trickle state");
    }

    root = cJSON_CreateObject();
    if (root == NULL ||
        !cJSON_AddStringToObject(root, "status", "ok") ||
        !cJSON_AddBoolToObject(root, "trickleRunning", running)) {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

    return send_json_object(req, 200, root);
}

static esp_err_t web_api_scale_weight_handler(httpd_req_t *req)
{
    char active_profile[64] = {0};
    bool running = false;
    bool valid = false;
    float weight = 0.0f;
    cJSON *root = NULL;
    esp_err_t ret = ESP_OK;

    ret = trickle_get_last_scale_value(&weight, &valid);
    if (ret != ESP_OK) {
        return send_simple_status(req, web_status_from_err(ret), "error", "Failed to read scale weight");
    }

    ret = ui_get_trickle_running(&running);
    if (ret != ESP_OK) {
        return send_simple_status(req, web_status_from_err(ret), "error", "Failed to read trickle state");
    }

    ret = ui_get_active_profile_name(active_profile, sizeof(active_profile));
    if (ret != ESP_OK) {
        return send_simple_status(req, web_status_from_err(ret), "error", "Failed to read active profile");
    }

    root = cJSON_CreateObject();
    if (root == NULL ||
        !cJSON_AddStringToObject(root, "status", "ok") ||
        !cJSON_AddStringToObject(root, "profile", active_profile) ||
        !cJSON_AddBoolToObject(root, "available", valid) ||
        !cJSON_AddBoolToObject(root, "trickleRunning", running)) {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

    if (valid && !cJSON_AddNumberToObject(root, "weight", (double)weight)) {
        cJSON_Delete(root);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }

    return send_json_object(req, 200, root);
}

static esp_err_t web_api_download_handler(httpd_req_t *req)
{
    char relative_path[FILE_PATH_MAX] = {0};
    char full_path[FILE_PATH_MAX];
    esp_err_t ret = ESP_OK;

    ret = get_decoded_query_value(req, "path", true, relative_path, sizeof(relative_path));
    if (ret != ESP_OK) {
        return send_simple_status(req, 400, "error", "Missing path");
    }

    ret = build_sd_path_from_relative(relative_path, false, full_path, sizeof(full_path));
    if (ret != ESP_OK) {
        return send_simple_status(req, ret == ESP_ERR_INVALID_ARG ? 400 : 500, "error", "Invalid path");
    }

    return send_file_response(req, full_path, full_path, true, false);
}

static esp_err_t web_api_upload_handler(httpd_req_t *req)
{
    char relative_path[FILE_PATH_MAX] = {0};
    char full_path[FILE_PATH_MAX];
    FILE *file = NULL;
    int remaining = req->content_len;
    esp_err_t ret = ESP_OK;

    ret = get_decoded_query_value(req, "path", true, relative_path, sizeof(relative_path));
    if (ret != ESP_OK) {
        return send_simple_status(req, 400, "error", "Missing path");
    }

    ret = build_sd_path_from_relative(relative_path, false, full_path, sizeof(full_path));
    if (ret != ESP_OK) {
        return send_simple_status(req, ret == ESP_ERR_INVALID_ARG ? 400 : 500, "error", "Invalid path");
    }

    ESP_LOGD(TAG, "Requested upload path: %s", full_path);
    file = fopen(full_path, "wb");
    if (file == NULL) {
        return send_simple_status(req, 500, "error", "Failed to open file");
    }

    while (remaining > 0) {
        const int receive_len = httpd_req_recv(req,
                                               s_ctx.scratch,
                                               remaining > (int)sizeof(s_ctx.scratch) ? (int)sizeof(s_ctx.scratch) : remaining);
        if (receive_len == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (receive_len <= 0) {
            fclose(file);
            unlink(full_path);
            return send_simple_status(req, 500, "error", "Receive failed");
        }
        if ((size_t)fwrite(s_ctx.scratch, 1, (size_t)receive_len, file) != (size_t)receive_len) {
            fclose(file);
            unlink(full_path);
            return send_simple_status(req, 500, "error", "Write failed");
        }
        remaining -= receive_len;
    }

    fclose(file);
    return send_simple_status(req, 200, "ok", "saved");
}

static esp_err_t web_api_delete_handler(httpd_req_t *req)
{
    char relative_path[FILE_PATH_MAX] = {0};
    char full_path[FILE_PATH_MAX];
    struct stat st = {0};
    esp_err_t ret = ESP_OK;

    ret = get_decoded_query_value(req, "path", true, relative_path, sizeof(relative_path));
    if (ret != ESP_OK) {
        return send_simple_status(req, 400, "error", "Missing path");
    }

    ret = build_sd_path_from_relative(relative_path, false, full_path, sizeof(full_path));
    if (ret != ESP_OK) {
        return send_simple_status(req, ret == ESP_ERR_INVALID_ARG ? 400 : 500, "error", "Invalid path");
    }

    ESP_LOGD(TAG, "Requested delete path: %s", full_path);
    if (stat(full_path, &st) != 0 || !S_ISREG(st.st_mode)) {
        return send_simple_status(req, 404, "error", "File not found");
    }

    if (unlink(full_path) != 0) {
        return send_simple_status(req, 500, "error", "Delete failed");
    }

    return send_simple_status(req, 200, "ok", "deleted");
}

esp_err_t webserver_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_uri_t api_list = {
        .uri = "/api/files/list",
        .method = HTTP_GET,
        .handler = web_api_list_handler,
    };
    httpd_uri_t api_download = {
        .uri = "/api/files/download",
        .method = HTTP_GET,
        .handler = web_api_download_handler,
    };
    httpd_uri_t api_upload = {
        .uri = "/api/files/upload",
        .method = HTTP_POST,
        .handler = web_api_upload_handler,
    };
    httpd_uri_t api_delete = {
        .uri = "/api/files/delete",
        .method = HTTP_POST,
        .handler = web_api_delete_handler,
    };
    httpd_uri_t api_profiles = {
        .uri = "/api/profiles",
        .method = HTTP_GET,
        .handler = web_api_profiles_handler,
    };
    httpd_uri_t api_profile_select = {
        .uri = "/api/profile/select",
        .method = HTTP_POST,
        .handler = web_api_profile_select_handler,
    };
    httpd_uri_t api_trickle_start = {
        .uri = "/api/trickle/start",
        .method = HTTP_POST,
        .handler = web_api_trickle_start_handler,
    };
    httpd_uri_t api_trickle_stop = {
        .uri = "/api/trickle/stop",
        .method = HTTP_POST,
        .handler = web_api_trickle_stop_handler,
    };
    httpd_uri_t api_target_weight = {
        .uri = "/api/trickle/target",
        .method = HTTP_GET,
        .handler = web_api_target_weight_handler,
    };
    httpd_uri_t api_target_weight_set = {
        .uri = "/api/trickle/target",
        .method = HTTP_POST,
        .handler = web_api_target_weight_set_handler,
    };
    httpd_uri_t api_scale_tare = {
        .uri = "/api/scale/tare",
        .method = HTTP_POST,
        .handler = web_api_scale_tare_handler,
    };
    httpd_uri_t api_scale_weight = {
        .uri = "/api/scale/weight",
        .method = HTTP_GET,
        .handler = web_api_scale_weight_handler,
    };
    httpd_uri_t static_files = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = web_static_get_handler,
    };

    if (s_server != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(&s_ctx, 0, sizeof(s_ctx));
    snprintf(s_ctx.base_path, sizeof(s_ctx.base_path), "%s", SERVER_DIR);

    config.server_port = 80;
    config.max_open_sockets = 7;
    config.backlog_conn = CONFIG_LWIP_MAX_SOCKETS;
    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;
    config.stack_size = 8192;
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 16;

    ESP_LOGI(TAG, "Starting HTTP server from %s on port %u", s_ctx.base_path, config.server_port);
    if (httpd_start(&s_server, &config) != ESP_OK) {
        s_server = NULL;
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return ESP_FAIL;
    }

    if (httpd_register_uri_handler(s_server, &api_list) != ESP_OK ||
        httpd_register_uri_handler(s_server, &api_download) != ESP_OK ||
        httpd_register_uri_handler(s_server, &api_upload) != ESP_OK ||
        httpd_register_uri_handler(s_server, &api_delete) != ESP_OK ||
        httpd_register_uri_handler(s_server, &api_profiles) != ESP_OK ||
        httpd_register_uri_handler(s_server, &api_profile_select) != ESP_OK ||
        httpd_register_uri_handler(s_server, &api_trickle_start) != ESP_OK ||
        httpd_register_uri_handler(s_server, &api_trickle_stop) != ESP_OK ||
        httpd_register_uri_handler(s_server, &api_target_weight) != ESP_OK ||
        httpd_register_uri_handler(s_server, &api_target_weight_set) != ESP_OK ||
        httpd_register_uri_handler(s_server, &api_scale_tare) != ESP_OK ||
        httpd_register_uri_handler(s_server, &api_scale_weight) != ESP_OK ||
        httpd_register_uri_handler(s_server, &static_files) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register one or more URI handlers");
        httpd_stop(s_server);
        s_server = NULL;
        memset(&s_ctx, 0, sizeof(s_ctx));
        return ESP_FAIL;
    }

    if (webserver_start_mdns(config.server_port) != ESP_OK) {
        httpd_stop(s_server);
        s_server = NULL;
        memset(&s_ctx, 0, sizeof(s_ctx));
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t webserver_stop(void)
{
    if (s_server == NULL) {
        return ESP_OK;
    }

    httpd_stop(s_server);
    s_server = NULL;
    webserver_stop_mdns();
    memset(&s_ctx, 0, sizeof(s_ctx));
    return ESP_OK;
}
