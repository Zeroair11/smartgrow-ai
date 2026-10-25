/**
 * @file ota_manager.h
 * @brief Over-The-Air (OTA) Firmware Update Manager for ESP32-S3 Dual-Partitioning
 */

#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OTA_STATE_IDLE = 0,
    OTA_STATE_CHECKING = 1,
    OTA_STATE_DOWNLOADING = 2,
    OTA_STATE_VERIFYING = 3,
    OTA_STATE_FLASHING = 4,
    OTA_STATE_REBOOT_REQUIRED = 5,
    OTA_STATE_ERROR = 6
} OTAState;

typedef struct {
    OTAState state;
    uint8_t  progress_percent;
    char     current_version[32];
    char     target_version[32];
    char     target_sha256[65];
    int      error_code;
} OTAStatus;

/**
 * @brief Initialize OTA subsystem and check active partition
 */
void ota_manager_init(const char *current_firmware_version);

/**
 * @brief Get current OTA status
 */
const OTAStatus* ota_manager_get_status(void);

/**
 * @brief Check if target version is newer and valid
 */
bool ota_manager_is_upgrade_valid(const char *target_version, const char *target_sha256);

/**
 * @brief Simulate or perform OTA write chunk
 */
bool ota_manager_write_chunk(const uint8_t *data, size_t len, size_t total_size);

/**
 * @brief Verify SHA-256 and commit partition boot flag
 */
bool ota_manager_finalize_and_commit(const char *expected_sha256);

#ifdef __cplusplus
}
#endif

#endif /* OTA_MANAGER_H */
