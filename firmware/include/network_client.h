/**
 * @file network_client.h
 * @brief REST HTTP/HTTPS Uplink Client for Backend Integration
 */

#ifndef NETWORK_CLIENT_H
#define NETWORK_CLIENT_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "telemetry_client.h"
#include "command_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char base_url[128];             /* e.g. http://192.168.1.100:8000/api/v1 */
    char device_id[48];             /* e.g. esp32s3-greenhouse-01 */
    char registration_token[64];    /* smartgrow-device-enrollment-secret-2026 */
    char firmware_version[16];      /* 1.0.0 */
    char model_version[16];         /* v1.0.0-dt */
    char mac_address[18];           /* b8:1f:3f:bb:d0:c4 */
    uint32_t timeout_ms;            /* HTTP request timeout (default 5000ms) */
} NetworkClientConfig;

typedef struct {
    bool is_registered;
    uint32_t telemetry_sent_count;
    uint32_t telemetry_fail_count;
    uint32_t commands_polled_count;
    uint32_t acks_sent_count;
    int last_http_status;
} NetworkClientStats;

/**
 * @brief Initialize the network client with backend endpoint and credentials
 */
bool network_client_init(const NetworkClientConfig *config);

/**
 * @brief Register device with backend if not already enrolled
 */
bool network_client_register_device(void);

/**
 * @brief Transmit serialized telemetry payload to backend
 */
bool network_client_send_telemetry(const TelemetryPayload *payload);

/**
 * @brief Poll backend for pending commands targeted at this device
 */
bool network_client_poll_commands(DeviceCommand *out_commands, size_t max_commands, size_t *out_count);

/**
 * @brief Send command execution ACK back to backend
 */
bool network_client_send_command_ack(const CommandExecutionResult *result);

/**
 * @brief Get diagnostic statistics for network operations
 */
const NetworkClientStats* network_client_get_stats(void);

#if !defined(ESP_PLATFORM)
/**
 * @brief Native testing helper: Set mock HTTP response code and response body
 */
void network_client_mock_set_response(int http_code, const char *body);
const char* network_client_mock_get_last_post_body(void);
const char* network_client_mock_get_last_url(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_CLIENT_H */
