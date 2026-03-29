#include "config.h"
#include "arduino_compat.h"
#include "pindef.h"

#include <string>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#include "esp_log.h"
#include "esp_ota_ops.h"

static const char *TAG = "update";

extern void updateDisplayLog(const std::string &msg, bool noLog);

// ============================================================
// SD path helper
// ============================================================
static std::string sd_path(const char *rel) {
    if (rel[0] == '/') return std::string(SD_MOUNT_POINT) + rel;
    return std::string(SD_MOUNT_POINT) + "/" + rel;
}

// ============================================================
// performUpdate — write a binary stream to the OTA partition
// ============================================================
static bool performUpdate(FILE *f, size_t update_size) {
    const esp_partition_t *update_part = esp_ota_get_next_update_partition(NULL);
    if (!update_part) {
        updateDisplayLog("No OTA update partition found!", false);
        return false;
    }

    esp_ota_handle_t ota_handle = 0;
    if (esp_ota_begin(update_part, update_size, &ota_handle) != ESP_OK) {
        updateDisplayLog("esp_ota_begin failed!", false);
        return false;
    }

    char buf[512];
    size_t written = 0;
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (esp_ota_write(ota_handle, buf, n) != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed at offset %u", (unsigned)written);
            esp_ota_end(ota_handle);
            return false;
        }
        written += n;
    }

    updateDisplayLog("Written: " + std::to_string(written) + " bytes", false);

    if (esp_ota_end(ota_handle) != ESP_OK) {
        updateDisplayLog("esp_ota_end failed!", false);
        return false;
    }

    if (esp_ota_set_boot_partition(update_part) != ESP_OK) {
        updateDisplayLog("esp_ota_set_boot_partition failed!", false);
        return false;
    }

    updateDisplayLog("FW Update done!", false);
    updateDisplayLog("Update successfully completed. Rebooting.", false);
    return true;
}

// ============================================================
// updateFromFS — check for /update.bin on SD and apply
// ============================================================
static void updateFromFS() {
    std::string full = sd_path("/update.bin");

    if (access(full.c_str(), F_OK) != 0) {
        updateDisplayLog("Could not load update.bin from sd root", false);
        return;
    }

    struct stat st;
    stat(full.c_str(), &st);

    if (!S_ISREG(st.st_mode)) {
        updateDisplayLog("Error, update.bin is not a file", false);
        return;
    }

    size_t update_size = (size_t)st.st_size;
    if (update_size == 0) {
        updateDisplayLog("Error, file is empty", false);
        return;
    }

    updateDisplayLog("Try to start update", false);

    FILE *f = fopen(full.c_str(), "rb");
    if (!f) {
        updateDisplayLog("Failed to open update.bin", false);
        return;
    }

    bool ok = performUpdate(f, update_size);
    fclose(f);

    if (ok) {
        // Remove the binary so we don't re-apply on next boot
        unlink(full.c_str());
        delay(1000);
        esp_restart();
    }
}

// ============================================================
// initUpdate — called from initSetup
// ============================================================
void initUpdate() {
    updateDisplayLog("Check for Firmware Update...", false);
    updateFromFS();
}
