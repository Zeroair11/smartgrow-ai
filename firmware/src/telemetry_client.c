#include "telemetry_client.h"
#include "board_config.h"
#include <stdio.h>
#include <string.h>

static char s_device_id[32] = "esp32s3-default";
static char s_boot_id[32]   = "boot-0001";
static uint32_t s_sequence  = 0;

void telemetry_client_init(const char *device_id, const char *boot_id) {
    if (device_id) {
        strncpy(s_device_id, device_id, sizeof(s_device_id) - 1);
        s_device_id[sizeof(s_device_id) - 1] = '\0';
    }
    if (boot_id) {
        strncpy(s_boot_id, boot_id, sizeof(s_boot_id) - 1);
        s_boot_id[sizeof(s_boot_id) - 1] = '\0';
    }
    s_sequence = 0;
}

uint32_t telemetry_client_next_sequence(void) {
    return ++s_sequence;
}

int telemetry_client_serialize_json(const TelemetryPayload *payload, char *out_buf, size_t buf_size) {
    if (!payload || !out_buf || buf_size == 0) return -1;

    return snprintf(out_buf, buf_size,
        "{"
        "\"schema_version\":\"%s\","
        "\"device_id\":\"%s\","
        "\"boot_id\":\"%s\","
        "\"sequence\":%u,"
        "\"timestamp_ms\":%u,"
        "\"soil_moisture_pct\":%.2f,"
        "\"temperature_c\":%.2f,"
        "\"humidity_pct\":%.2f,"
        "\"light_level_pct\":%.2f,"
        "\"ai_class\":%d,"
        "\"ai_confidence\":%.3f,"
        "\"model_version\":\"%s\","
        "\"firmware_version\":\"%s\","
        "\"pump_state\":%s,"
        "\"operating_mode\":\"%s\","
        "\"health_status\":\"%s\""
        "}",
        payload->schema_version ? payload->schema_version : SCHEMA_VERSION,
        payload->device_id ? payload->device_id : s_device_id,
        payload->boot_id ? payload->boot_id : s_boot_id,
        payload->sequence,
        payload->timestamp_ms,
        payload->soil_moisture_pct,
        payload->temperature_c,
        payload->humidity_pct,
        payload->light_level_pct,
        payload->ai_class,
        payload->ai_confidence,
        payload->model_version ? payload->model_version : "v1.0-baseline",
        payload->firmware_version ? payload->firmware_version : FIRMWARE_VERSION,
        payload->pump_state ? "true" : "false",
        payload->operating_mode ? payload->operating_mode : "AUTO",
        payload->health_status ? payload->health_status : "HEALTHY"
    );
}
