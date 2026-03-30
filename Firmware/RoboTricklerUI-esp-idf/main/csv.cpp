#include "config.h"
#include "arduino_compat.h"
#include "pindef.h"

#include <string>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <dirent.h>
#include <errno.h>

#include "esp_log.h"

static const char *TAG = "csv";

// ============================================================
// SD path helper — prefix /sdcard to relative paths
// ============================================================
static std::string sd_path(const char *rel) {
    if (rel[0] == '/') return std::string(SD_MOUNT_POINT) + rel;
    return std::string(SD_MOUNT_POINT) + "/" + rel;
}

// ============================================================
// ceateCSVFile — create a new numbered CSV file in folderName
// ============================================================
std::string ceateCSVFile(const char *folderName, const char *fileName) {
    std::string folder_path = sd_path(folderName);

    // Create folder if it doesn't exist
    struct stat st;
    if (stat(folder_path.c_str(), &st) != 0) {
        mkdir(folder_path.c_str(), 0755);
        ESP_LOGD(TAG, "Log folder created: %s", folder_path.c_str());
    }

    // Find next available file number
    int fileCount = 0;
    std::string rel_path;
    while (true) {
        rel_path = std::string(folderName) + "/" + std::string(fileName) + "_" + std::to_string(fileCount) + ".csv";
        std::string full = sd_path(rel_path.c_str());
        if (access(full.c_str(), F_OK) != 0) break;  // file does not exist — use this name
        fileCount++;
    }

    std::string full_path = sd_path(rel_path.c_str());
    FILE *f = fopen(full_path.c_str(), "w");
    if (!f) {
        ESP_LOGE(TAG, "Failed to create CSV: %s", full_path.c_str());
        return "";
    }
    fprintf(f, "Count,Weight\r\n");
    fclose(f);

    ESP_LOGD(TAG, "Created CSV: %s", full_path.c_str());
    return rel_path;  // return relative path (used as 'path' global)
}

// ============================================================
// writeCSVFile — append one row to an existing CSV
// ============================================================
void writeCSVFile(const char *rel_path, float w, int count) {
    std::string full_path = sd_path(rel_path);
    FILE *f = fopen(full_path.c_str(), "a");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open CSV for append: %s", full_path.c_str());
        // Recreate
        ceateCSVFile("/log", "log");
        return;
    }
    if (fprintf(f, "%d,%.4f\r\n", count, w) < 0) {
        ESP_LOGE(TAG, "CSV write failed: %s", full_path.c_str());
    } else {
        ESP_LOGD(TAG, "CSV row appended: %d %.4f", count, w);
    }
    fclose(f);
}

// ============================================================
// writeFile — write (overwrite) a text file
// ============================================================
void writeFile(const char *rel_path, const std::string &message) {
    std::string full = sd_path(rel_path);
    FILE *f = fopen(full.c_str(), "w");
    if (!f) { ESP_LOGE(TAG, "writeFile failed: %s", full.c_str()); return; }
    fputs(message.c_str(), f);
    fclose(f);
}

// ============================================================
// logToFile — append to /debugLog.txt
// ============================================================
void logToFile(const std::string &message) {
    std::string full = sd_path("/debugLog.txt");
    FILE *f = fopen(full.c_str(), "a");
    if (!f) {
        writeFile("/debugLog.txt", message);
        return;
    }
    fputs(message.c_str(), f);
    fclose(f);
}
