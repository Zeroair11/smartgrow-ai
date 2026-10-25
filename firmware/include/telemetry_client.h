/**
 * @file telemetry_client.h
 * @brief Telemetry Serialization & Uplink Client
 */

#ifndef TELEMETRY_CLIENT_H
#define TELEMETRY_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include "sensor_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* schema_version;
    const char* device_id;
    const char* boot_id;
    uint32_t    sequence;
    uint32_t    timestamp_ms;
    float       soil_moisture_pct;
    float       temperature_c;
    float       humidity_pct;
    float       light_level_pct;
    int         ai_class;
    float       ai_confidence;
    const char* model_version;
    const char* firmware_version;
    bool        pump_state;
    const char* operating_mode;
    const char* health_status;
} TelemetryPayload;

/**
 * @brief Initialize telemetry client with device identity
 */
void telemetry_client_init(const char *device_id, const char *boot_id);

/**
 * @brief Build JSON telemetry string into provided buffer
 */
int telemetry_client_serialize_json(const TelemetryPayload *payload, char *out_buf, size_t buf_size);

/**
 * @brief Get next monotonic sequence number
 */
uint32_t telemetry_client_next_sequence(void);

#ifdef __cplusplus
}
#endif

#endif /* TELEMETRY_CLIENT_H */
