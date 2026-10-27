/**
 * @file telemetry_client.c
 * @brief Telemetry Serialization & Uplink Client Implementation
 */

#include "telemetry_client.h"
#include "board_config.h"
#include "firmware_config.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static char s_device_id[48] = DEFAULT_DEVICE_ID;
static char s_boot_id[48]   = "boot-0001";
static uint32_t s_sequence  = 0;

void telemetry_client_init(const char *device_id, const char *boot_id) {
    if (device_id && device_id[0] != '\0') {
        strncpy(s_device_id, device_id, sizeof(s_device_id) - 1);
        s_device_id[sizeof(s_device_id) - 1] = '\0';
    } else {
        const FirmwareConfig *fw_cfg = firmware_config_get();
        strncpy(s_device_id, fw_cfg->device.device_id, sizeof(s_device_id) - 1);
        s_device_id[sizeof(s_device_id) - 1] = '\0';
    }

    if (boot_id && boot_id[0] != '\0') {
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

    const FirmwareConfig *fw_cfg = firmware_config_get();

    char soil_str[32];
    char temp_str[32];
    char hum_str[32];
    char light_str[32];

    if (payload->soil_valid && !isnan(payload->soil_moisture_pct)) {
        snprintf(soil_str, sizeof(soil_str), "%.2f", payload->soil_moisture_pct);
    } else {
        snprintf(soil_str, sizeof(soil_str), "null");
    }

    if (payload->dht_valid && !isnan(payload->temperature_c)) {
        snprintf(temp_str, sizeof(temp_str), "%.2f", payload->temperature_c);
    } else {
        snprintf(temp_str, sizeof(temp_str), "null");
    }

    if (payload->dht_valid && !isnan(payload->humidity_pct)) {
        snprintf(hum_str, sizeof(hum_str), "%.2f", payload->humidity_pct);
    } else {
        snprintf(hum_str, sizeof(hum_str), "null");
    }

    if (payload->light_valid && !isnan(payload->light_level_pct)) {
        snprintf(light_str, sizeof(light_str), "%.2f", payload->light_level_pct);
    } else {
        snprintf(light_str, sizeof(light_str), "null");
    }

    return snprintf(out_buf, buf_size,
        "{"
        "\"schema_version\":\"%s\","
        "\"device_id\":\"%s\","
        "\"boot_id\":\"%s\","
        "\"sequence\":%lu,"
        "\"timestamp_ms\":%lu,"
        "\"soil_moisture_pct\":%s,"
        "\"temperature_c\":%s,"
        "\"humidity_pct\":%s,"
        "\"light_level_pct\":%s,"
        "\"soil_valid\":%s,"
        "\"dht_valid\":%s,"
        "\"light_valid\":%s,"
        "\"ai_class\":%d,"
        "\"ai_confidence\":%.3f,"
        "\"model_version\":\"%s\","
        "\"firmware_version\":\"%s\","
        "\"pump_state\":%s,"
        "\"operating_mode\":\"%s\","
        "\"health_status\":\"%s\""
        "}",
        payload->schema_version ? payload->schema_version : fw_cfg->device.schema_version,
        payload->device_id ? payload->device_id : s_device_id,
        payload->boot_id ? payload->boot_id : s_boot_id,
        (unsigned long)payload->sequence,
        (unsigned long)payload->timestamp_ms,
        soil_str,
        temp_str,
        hum_str,
        light_str,
        payload->soil_valid ? "true" : "false",
        payload->dht_valid ? "true" : "false",
        payload->light_valid ? "true" : "false",
        payload->ai_class,
        payload->ai_confidence,
        payload->model_version ? payload->model_version : fw_cfg->ai.model_version,
        payload->firmware_version ? payload->firmware_version : fw_cfg->device.firmware_version,
        payload->pump_state ? "true" : "false",
        payload->operating_mode ? payload->operating_mode : "AUTO",
        payload->health_status ? payload->health_status : "HEALTHY"
    );
}
