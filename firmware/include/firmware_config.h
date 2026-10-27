/**
 * @file firmware_config.h
 * @brief Centralized Firmware Configuration System for SmartGrow AI
 *
 * Provides a unified, type-safe, validated single source of truth for:
 * - Device identity
 * - Wi-Fi parameters & backoff policy
 * - Backend host, port, endpoints, tokens & intervals
 * - DHT11 temperature & humidity sensor parameters & timing
 * - ADC sensor calibration & pinout mapping
 * - Actuator pinout, polarity & hardware safety limits
 * - Edge AI / TinyML parameters
 * - OTA and diagnostic logging
 */

#ifndef FIRMWARE_CONFIG_H
#define FIRMWARE_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "project_config.h"
#include "config_defaults.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
   TYPEDEF SUB-CONFIG STRUCTURES
   ========================================================================= */

typedef struct {
    char device_id[48];             /* e.g. "esp32s3-b81f3f" */
    char device_name[48];           /* e.g. "SmartGrow ESP32-S3 Node" */
    char firmware_version[16];      /* e.g. "1.0.0" */
    char board_name[32];            /* e.g. "ESP32-S3-N16R8" */
    char schema_version[16];        /* e.g. "1.0.0" */
} DeviceConfig;

typedef struct {
    char ssid[33];                  /* 32 chars + null */
    char password[65];              /* 64 chars + null (Masked in logs) */
    uint32_t max_retry;             /* 0 = infinite retry */
    uint32_t initial_backoff_ms;    /* e.g. 1000ms */
    uint32_t max_backoff_ms;        /* e.g. 30000ms */
    uint32_t connect_timeout_ms;    /* e.g. 15000ms */
} WifiConfig;

typedef struct {
    char backend_host[64];          /* Hostname or IPv4, e.g. "192.168.1.2" */
    uint16_t backend_port;          /* e.g. 8000 */
    char api_base_path[32];         /* e.g. "/api/v1" */
    char base_url[128];             /* Computed: "http://<host>:<port><api_base_path>" */
    char registration_token[64];    /* Enrollment secret token (Masked in logs) */
    uint32_t http_timeout_ms;       /* HTTP request timeout (e.g. 5000ms) */
    uint32_t telemetry_interval_ms; /* Telemetry upload interval (e.g. 5000ms) */
    uint32_t command_poll_interval_ms; /* Command downlink polling interval (e.g. 3000ms) */
    uint32_t max_payload_size;      /* Max HTTP buffer size (e.g. 2048) */
} NetworkConfig;

typedef struct {
    uint8_t pin;                    /* GPIO for DHT11 single-wire bus (default 4) */
    uint32_t sample_interval_ms;    /* Minimum sampling interval (>= 1000ms; default 2000ms) */
    uint32_t read_timeout_ms;       /* Read operation timeout (default 100ms) */
    float temp_min_c;               /* Physical sensor plausible min temp (0.0°C) */
    float temp_max_c;               /* Physical sensor plausible max temp (50.0°C) */
    float humidity_min_pct;         /* Physical sensor plausible min humidity (20.0% RH) */
    float humidity_max_pct;         /* Physical sensor plausible max humidity (90.0% RH) */
    uint8_t max_consecutive_errors; /* Error threshold to declare persistent fault (default 3) */
} Dht11Config;

typedef struct {
    uint8_t pin_soil_adc;           /* GPIO 1 (ADC1_CH0) */
    uint8_t pin_ldr_adc;            /* GPIO 2 (ADC1_CH1) */
    uint8_t pin_i2c_sda;            /* GPIO 8 */
    uint8_t pin_i2c_scl;            /* GPIO 9 */
    uint16_t soil_adc_dry_raw;      /* Raw ADC dry air calibration (default 3200) */
    uint16_t soil_adc_wet_raw;      /* Raw ADC submerged water calibration (default 1200) */
    float adc_vref_mv;              /* ADC reference millivolts (3300.0) */
    uint16_t soil_raw_min_plausible;/* Below this is short-circuit to GND (150) */
    uint16_t soil_raw_max_plausible;/* Above this is open-circuit wire (3850) */
    uint32_t sensor_sample_interval_ms; /* e.g. 2000ms */
} SensorConfig;

typedef struct {
    uint8_t pin_relay_pump;         /* GPIO 10 */
    uint8_t relay_pump_active_level;/* 0 = Active Low, 1 = Active High */
    uint8_t pin_status_led;         /* GPIO 21 */
    uint8_t status_led_active_level;/* 1 = Active High */
    uint8_t pin_buzzer;             /* GPIO 12 */
    uint8_t pin_servo_vent;         /* GPIO 11 */
} ActuatorConfig;

typedef struct {
    uint32_t max_pump_duration_ms;  /* Hard safety cutoff limit (default 120000ms) */
    uint32_t min_pump_cooldown_ms;  /* Minimum cooldown before restart (default 180000ms) */
    uint32_t offline_failsafe_ms;   /* Offline fallback timeout (default 300000ms) */
    float min_safe_temp_c;          /* Safety envelope lower temperature limit (0.0°C) */
    float max_safe_temp_c;          /* Safety envelope upper temperature limit (50.0°C) */
} SafetyConfig;

typedef struct {
    char model_version[16];         /* e.g. "v1.0.0-dt" */
    float confidence_threshold;     /* e.g. 0.50 */
    uint32_t inference_interval_ms; /* e.g. 2000ms */
} EdgeAiConfig;

typedef struct {
    bool enabled;                   /* true = OTA check enabled */
    uint32_t check_interval_ms;     /* e.g. 60000ms */
    uint32_t http_timeout_ms;       /* e.g. 10000ms */
} OtaConfig;

typedef enum {
    CONTROL_MODE_AUTO     = 0,  /* Edge AI + Guardrails */
    CONTROL_MODE_MANUAL   = 1,  /* Manual commands only */
    CONTROL_MODE_RULE     = 2,  /* Local autonomous threshold rules */
    CONTROL_MODE_HYBRID   = 3,  /* Hybrid: Rules + Edge AI */
    CONTROL_MODE_SAFE     = 4   /* Failsafe lock: pump inhibited */
} ControlMode;

typedef struct {
    bool     enabled;
    float    soil_moisture_threshold_pct; /* Trigger below this % */
    uint32_t pump_duration_s;             /* Planned run duration */
    uint32_t cooldown_s;                  /* Cooldown period */
} LocalRuleConfig;

typedef struct {
    ControlMode     mode;
    bool            offline_autonomous_enabled;
    LocalRuleConfig local_rule;
} ControlConfig;

/**
 * @brief Master Firmware Configuration Object
 */
typedef struct {
    DeviceConfig   device;
    WifiConfig     wifi;
    NetworkConfig  network;
    Dht11Config    dht11;
    SensorConfig   sensors;
    ActuatorConfig actuators;
    SafetyConfig   safety;
    EdgeAiConfig   ai;
    OtaConfig      ota;
    ControlConfig  control;
} FirmwareConfig;

/* =========================================================================
   PUBLIC CONFIGURATION API
   ========================================================================= */

/**
 * @brief Initialize a FirmwareConfig structure with compile-time defaults
 */
void firmware_config_init_defaults(FirmwareConfig *cfg);

/**
 * @brief Validate all fields in a FirmwareConfig structure
 * @param cfg Pointer to config to validate
 * @param err_msg Output buffer for validation error message
 * @param err_size Size of output error buffer
 * @return true if configuration is strictly valid, false if invalid
 */
bool firmware_config_validate(const FirmwareConfig *cfg, char *err_msg, size_t err_size);

/**
 * @brief Initialize the global centralized firmware configuration singleton
 * @param initial_cfg Optional custom configuration (if NULL, defaults are loaded)
 * @return true if successfully initialized and validated
 */
bool firmware_config_init(const FirmwareConfig *initial_cfg);

/**
 * @brief Get read-only pointer to the active centralized firmware configuration
 */
const FirmwareConfig* firmware_config_get(void);

/**
 * @brief Atomically set / update the centralized firmware configuration
 * @param new_cfg New configuration to apply
 * @return true if valid and applied, false if validation failed
 */
bool firmware_config_set(const FirmwareConfig *new_cfg);

/**
 * @brief Helper to build clean REST Base URL ("http://<host>:<port><base_path>")
 */
void firmware_config_build_base_url(const FirmwareConfig *cfg, char *out_url, size_t size);

/**
 * @brief Log configuration to serial/stdout with secrets safely masked (never outputs passwords)
 */
void firmware_config_log_safe(const FirmwareConfig *cfg);

/* Field-specific convenience setters */
bool firmware_config_set_device_id(const char *device_id);
bool firmware_config_set_backend(const char *host, uint16_t port);
bool firmware_config_set_wifi(const char *ssid, const char *password);
bool firmware_config_set_soil_calibration(uint16_t dry_raw, uint16_t wet_raw);
bool firmware_config_set_safety_limits(uint32_t max_runtime_ms, uint32_t min_cooldown_ms);
bool firmware_config_set_control_mode(ControlMode mode);
bool firmware_config_set_local_rule(bool enabled, float threshold_pct, uint32_t duration_s);

/* NVS Non-Volatile Persistence API */
bool firmware_config_nvs_init(void);
bool firmware_config_load_nvs(void);
bool firmware_config_save_nvs(void);

#if !defined(ESP_PLATFORM)
void firmware_config_mock_nvs_clear(void);
void firmware_config_mock_nvs_corrupt(void);
void firmware_config_mock_nvs_set_write_fail(bool fail);
#endif

#ifdef __cplusplus
}
#endif

#endif /* FIRMWARE_CONFIG_H */
