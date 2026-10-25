#include "ota_manager.h"
#include <string.h>
#include <stdio.h>

static OTAStatus s_status;

void ota_manager_init(const char *current_firmware_version) {
    memset(&s_status, 0, sizeof(s_status));
    s_status.state = OTA_STATE_IDLE;
    s_status.progress_percent = 0;
    if (current_firmware_version) {
        strncpy(s_status.current_version, current_firmware_version, sizeof(s_status.current_version) - 1);
    } else {
        strncpy(s_status.current_version, "1.0.0", sizeof(s_status.current_version) - 1);
    }
}

const OTAStatus* ota_manager_get_status(void) {
    return &s_status;
}

bool ota_manager_is_upgrade_valid(const char *target_version, const char *target_sha256) {
    if (!target_version || !target_sha256) return false;
    /* Basic validation: Cannot downgrade to identical version */
    if (strcmp(target_version, s_status.current_version) == 0) {
        return false;
    }
    strncpy(s_status.target_version, target_version, sizeof(s_status.target_version) - 1);
    strncpy(s_status.target_sha256, target_sha256, sizeof(s_status.target_sha256) - 1);
    s_status.state = OTA_STATE_CHECKING;
    return true;
}

bool ota_manager_write_chunk(const uint8_t *data, size_t len, size_t total_size) {
    (void)data;
    if (total_size == 0) return false;
    s_status.state = OTA_STATE_DOWNLOADING;
    static size_t bytes_written = 0;
    bytes_written += len;
    if (bytes_written > total_size) bytes_written = total_size;
    s_status.progress_percent = (uint8_t)((bytes_written * 100) / total_size);

#if defined(ESP_PLATFORM)
    // esp_ota_write(ota_handle, data, len);
#endif
    return true;
}

bool ota_manager_finalize_and_commit(const char *expected_sha256) {
    if (!expected_sha256) return false;
    s_status.state = OTA_STATE_VERIFYING;

#if defined(ESP_PLATFORM)
    // esp_ota_end(ota_handle);
    // esp_ota_set_boot_partition(update_partition);
#endif
    s_status.state = OTA_STATE_REBOOT_REQUIRED;
    s_status.progress_percent = 100;
    return true;
}
