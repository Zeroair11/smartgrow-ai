/**
 * @file firmware_config.c
 * @brief Centralized Firmware Configuration Implementation
 */

#include "firmware_config.h"
#include <stdio.h>
#include <string.h>

#if defined(ESP_PLATFORM)
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
static const char *TAG = "firmware_cfg";
#define NVS_NAMESPACE "smartgrow_cfg"
#endif

static FirmwareConfig s_global_config;
static bool s_is_initialized = false;

void firmware_config_build_base_url(const FirmwareConfig *cfg, char *out_url, size_t size) {
    if (!cfg || !out_url || size == 0) return;

    const char *host = cfg->network.backend_host;
    uint16_t port = cfg->network.backend_port;
    const char *path = cfg->network.api_base_path;

    /* Ensure path starts with single '/' if non-empty */
    if (path[0] == '/') {
        snprintf(out_url, size, "http://%s:%u%s", host, (unsigned)port, path);
    } else if (path[0] != '\0') {
        snprintf(out_url, size, "http://%s:%u/%s", host, (unsigned)port, path);
    } else {
        snprintf(out_url, size, "http://%s:%u", host, (unsigned)port);
    }
}

void firmware_config_init_defaults(FirmwareConfig *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));

    /* Device identity */
    strncpy(cfg->device.device_id, DEFAULT_DEVICE_ID, sizeof(cfg->device.device_id) - 1);
    strncpy(cfg->device.device_name, DEFAULT_DEVICE_NAME, sizeof(cfg->device.device_name) - 1);
    strncpy(cfg->device.firmware_version, DEFAULT_FIRMWARE_VERSION, sizeof(cfg->device.firmware_version) - 1);
    strncpy(cfg->device.board_name, DEFAULT_BOARD_NAME, sizeof(cfg->device.board_name) - 1);
    strncpy(cfg->device.schema_version, DEFAULT_SCHEMA_VERSION, sizeof(cfg->device.schema_version) - 1);

    /* Wi-Fi */
    strncpy(cfg->wifi.ssid, DEFAULT_WIFI_SSID, sizeof(cfg->wifi.ssid) - 1);
    strncpy(cfg->wifi.password, DEFAULT_WIFI_PASSWORD, sizeof(cfg->wifi.password) - 1);
    cfg->wifi.max_retry = DEFAULT_WIFI_MAX_RETRY;
    cfg->wifi.initial_backoff_ms = DEFAULT_WIFI_INITIAL_BACKOFF_MS;
    cfg->wifi.max_backoff_ms = DEFAULT_WIFI_MAX_BACKOFF_MS;
    cfg->wifi.connect_timeout_ms = DEFAULT_WIFI_CONNECT_TIMEOUT_MS;

    /* Network / Backend */
    strncpy(cfg->network.backend_host, DEFAULT_BACKEND_HOST, sizeof(cfg->network.backend_host) - 1);
    cfg->network.backend_port = DEFAULT_BACKEND_PORT;
    strncpy(cfg->network.api_base_path, DEFAULT_API_BASE_PATH, sizeof(cfg->network.api_base_path) - 1);
    strncpy(cfg->network.registration_token, DEFAULT_REGISTRATION_TOKEN, sizeof(cfg->network.registration_token) - 1);
    cfg->network.http_timeout_ms = DEFAULT_HTTP_TIMEOUT_MS;
    cfg->network.telemetry_interval_ms = DEFAULT_TELEMETRY_INTERVAL_MS;
    cfg->network.command_poll_interval_ms = DEFAULT_COMMAND_POLL_INTERVAL_MS;
    cfg->network.max_payload_size = DEFAULT_PAYLOAD_SIZE_LIMIT;
    firmware_config_build_base_url(cfg, cfg->network.base_url, sizeof(cfg->network.base_url));

    /* DHT11 Sensor */
    cfg->dht11.pin = DEFAULT_PIN_DHT11;
    cfg->dht11.sample_interval_ms = DEFAULT_DHT11_SAMPLE_INTERVAL_MS;
    cfg->dht11.read_timeout_ms = DEFAULT_DHT11_READ_TIMEOUT_MS;
    cfg->dht11.temp_min_c = DEFAULT_DHT11_TEMP_MIN_C;
    cfg->dht11.temp_max_c = DEFAULT_DHT11_TEMP_MAX_C;
    cfg->dht11.humidity_min_pct = DEFAULT_DHT11_HUMIDITY_MIN_PCT;
    cfg->dht11.humidity_max_pct = DEFAULT_DHT11_HUMIDITY_MAX_PCT;
    cfg->dht11.max_consecutive_errors = DEFAULT_DHT11_MAX_CONSECUTIVE_ERRORS;

    /* Sensors & Calibration */
    cfg->sensors.pin_soil_adc = DEFAULT_PIN_SOIL_ADC;
    cfg->sensors.pin_ldr_adc = DEFAULT_PIN_LDR_ADC;
    cfg->sensors.pin_i2c_sda = DEFAULT_PIN_I2C_SDA;
    cfg->sensors.pin_i2c_scl = DEFAULT_PIN_I2C_SCL;
    cfg->sensors.soil_adc_dry_raw = DEFAULT_SOIL_ADC_DRY_RAW;
    cfg->sensors.soil_adc_wet_raw = DEFAULT_SOIL_ADC_WET_RAW;
    cfg->sensors.adc_vref_mv = DEFAULT_ADC_VREF_MV;
    cfg->sensors.soil_raw_min_plausible = DEFAULT_SOIL_RAW_MIN_PLAUSIBLE;
    cfg->sensors.soil_raw_max_plausible = DEFAULT_SOIL_RAW_MAX_PLAUSIBLE;
    cfg->sensors.sensor_sample_interval_ms = DEFAULT_SENSOR_SAMPLE_INTERVAL_MS;

    /* Actuators */
    cfg->actuators.pin_relay_pump = DEFAULT_PIN_RELAY_PUMP;
    cfg->actuators.relay_pump_active_level = DEFAULT_RELAY_PUMP_ACTIVE_LEVEL;
    cfg->actuators.pin_status_led = DEFAULT_PIN_STATUS_LED;
    cfg->actuators.status_led_active_level = DEFAULT_STATUS_LED_ACTIVE_LEVEL;
    cfg->actuators.pin_buzzer = DEFAULT_PIN_BUZZER;
    cfg->actuators.pin_servo_vent = DEFAULT_PIN_SERVO_VENT;

    /* Safety */
    cfg->safety.max_pump_duration_ms = DEFAULT_SAFETY_MAX_PUMP_DURATION_MS;
    cfg->safety.min_pump_cooldown_ms = DEFAULT_SAFETY_MIN_PUMP_COOLDOWN_MS;
    cfg->safety.offline_failsafe_ms = DEFAULT_SAFETY_OFFLINE_FAILSAFE_MS;
    cfg->safety.min_safe_temp_c = DEFAULT_SAFETY_MIN_SAFE_TEMP_C;
    cfg->safety.max_safe_temp_c = DEFAULT_SAFETY_MAX_SAFE_TEMP_C;

    /* Edge AI */
    strncpy(cfg->ai.model_version, DEFAULT_AI_MODEL_VERSION, sizeof(cfg->ai.model_version) - 1);
    cfg->ai.confidence_threshold = DEFAULT_AI_CONFIDENCE_THRESHOLD;
    cfg->ai.inference_interval_ms = DEFAULT_AI_INFERENCE_INTERVAL_MS;

    /* OTA */
    cfg->ota.enabled = DEFAULT_OTA_ENABLED;
    cfg->ota.check_interval_ms = DEFAULT_OTA_CHECK_INTERVAL_MS;
    cfg->ota.http_timeout_ms = DEFAULT_OTA_TIMEOUT_MS;

    /* Control Mode & Autonomous Local Rules */
    cfg->control.mode = (ControlMode)DEFAULT_CONTROL_MODE;
    cfg->control.offline_autonomous_enabled = DEFAULT_OFFLINE_AUTONOMOUS_ENABLED;
    cfg->control.local_rule.enabled = DEFAULT_LOCAL_RULE_ENABLED;
    cfg->control.local_rule.soil_moisture_threshold_pct = DEFAULT_LOCAL_RULE_SOIL_THRESHOLD_PCT;
    cfg->control.local_rule.pump_duration_s = DEFAULT_LOCAL_RULE_PUMP_DURATION_S;
    cfg->control.local_rule.cooldown_s = DEFAULT_LOCAL_RULE_COOLDOWN_S;
}

bool firmware_config_validate(const FirmwareConfig *cfg, char *err_msg, size_t err_size) {
    if (!cfg) {
        if (err_msg && err_size > 0) snprintf(err_msg, err_size, "Config pointer is NULL");
        return false;
    }

    /* 1. Device identity validation */
    if (cfg->device.device_id[0] == '\0') {
        if (err_msg && err_size > 0) snprintf(err_msg, err_size, "Device ID cannot be empty");
        return false;
    }

    /* 2. Wi-Fi validation */
    if (cfg->wifi.ssid[0] == '\0') {
        if (err_msg && err_size > 0) snprintf(err_msg, err_size, "Wi-Fi SSID cannot be empty");
        return false;
    }

    /* 3. Backend network validation */
    if (cfg->network.backend_host[0] == '\0') {
        if (err_msg && err_size > 0) snprintf(err_msg, err_size, "Backend host cannot be empty");
        return false;
    }
    if (cfg->network.backend_port == 0) {
        if (err_msg && err_size > 0) snprintf(err_msg, err_size, "Backend port cannot be 0");
        return false;
    }
    if (cfg->network.http_timeout_ms < 500 || cfg->network.http_timeout_ms > 60000) {
        if (err_msg && err_size > 0) snprintf(err_msg, err_size, "HTTP timeout %u ms out of bounds (500-60000ms)", (unsigned)cfg->network.http_timeout_ms);
        return false;
    }

    /* 4. DHT11 timing & boundaries validation */
    if (cfg->dht11.sample_interval_ms < 1000) {
        if (err_msg && err_size > 0) snprintf(err_msg, err_size, "DHT11 sample interval %u ms is too fast (< 1000ms)", (unsigned)cfg->dht11.sample_interval_ms);
        return false;
    }
    if (cfg->dht11.temp_min_c >= cfg->dht11.temp_max_c) {
        if (err_msg && err_size > 0) snprintf(err_msg, err_size, "DHT11 temp_min (%.1f) must be less than temp_max (%.1f)", cfg->dht11.temp_min_c, cfg->dht11.temp_max_c);
        return false;
    }
    if (cfg->dht11.humidity_min_pct >= cfg->dht11.humidity_max_pct) {
        if (err_msg && err_size > 0) snprintf(err_msg, err_size, "DHT11 hum_min (%.1f) must be less than hum_max (%.1f)", cfg->dht11.humidity_min_pct, cfg->dht11.humidity_max_pct);
        return false;
    }

    /* 5. Soil moisture calibration validation */
    if (cfg->sensors.soil_adc_dry_raw <= cfg->sensors.soil_adc_wet_raw + 200) {
        if (err_msg && err_size > 0) snprintf(err_msg, err_size, "Soil dry ADC (%u) must exceed wet ADC (%u) by at least 200", cfg->sensors.soil_adc_dry_raw, cfg->sensors.soil_adc_wet_raw);
        return false;
    }

    /* 6. Hardware safety limits validation */
    if (cfg->safety.max_pump_duration_ms == 0 || cfg->safety.max_pump_duration_ms > 300000) {
        if (err_msg && err_size > 0) snprintf(err_msg, err_size, "Pump max runtime %u ms out of bounds (1 - 300000ms)", (unsigned)cfg->safety.max_pump_duration_ms);
        return false;
    }
    if (cfg->safety.min_pump_cooldown_ms == 0) {
        if (err_msg && err_size > 0) snprintf(err_msg, err_size, "Pump cooldown cannot be 0");
        return false;
    }

    /* 7. GPIO Pin collision validation (Ensuring physical pins don't overlap) */
    uint8_t pins[] = {
        cfg->sensors.pin_soil_adc,
        cfg->sensors.pin_ldr_adc,
        cfg->dht11.pin,
        cfg->actuators.pin_relay_pump,
        cfg->actuators.pin_servo_vent,
        cfg->actuators.pin_buzzer,
        cfg->actuators.pin_status_led,
        cfg->sensors.pin_i2c_sda,
        cfg->sensors.pin_i2c_scl
    };
    size_t pin_count = sizeof(pins) / sizeof(pins[0]);
    for (size_t i = 0; i < pin_count; i++) {
        for (size_t j = i + 1; j < pin_count; j++) {
            if (pins[i] == pins[j]) {
                if (err_msg && err_size > 0) snprintf(err_msg, err_size, "GPIO Pin conflict: pin %u assigned to multiple peripherals", pins[i]);
                return false;
            }
        }
    }

    if (err_msg && err_size > 0) err_msg[0] = '\0';
    return true;
}

bool firmware_config_init(const FirmwareConfig *initial_cfg) {
    if (initial_cfg) {
        char err[128] = {0};
        if (!firmware_config_validate(initial_cfg, err, sizeof(err))) {
#if defined(ESP_PLATFORM)
            ESP_LOGE(TAG, "Config initialization failed: %s", err);
#else
            printf("[CONFIG ERROR] %s\n", err);
#endif
            return false;
        }
        s_global_config = *initial_cfg;
    } else {
        firmware_config_init_defaults(&s_global_config);
        firmware_config_load_nvs();
    }
    s_is_initialized = true;
    return true;
}

const FirmwareConfig* firmware_config_get(void) {
    if (!s_is_initialized) {
        firmware_config_init_defaults(&s_global_config);
        s_is_initialized = true;
    }
    return &s_global_config;
}

bool firmware_config_set(const FirmwareConfig *new_cfg) {
    if (!new_cfg) return false;
    char err[128] = {0};
    if (!firmware_config_validate(new_cfg, err, sizeof(err))) {
#if defined(ESP_PLATFORM)
        ESP_LOGE(TAG, "Rejecting invalid config update: %s", err);
#else
        printf("[CONFIG REJECT] %s\n", err);
#endif
        return false;
    }
    s_global_config = *new_cfg;
    firmware_config_build_base_url(&s_global_config, s_global_config.network.base_url, sizeof(s_global_config.network.base_url));
    return true;
}

void firmware_config_log_safe(const FirmwareConfig *cfg) {
    if (!cfg) cfg = firmware_config_get();

    /* Build masked token string (e.g. "smart***") */
    char masked_token[16] = "****";
    if (strlen(cfg->network.registration_token) > 6) {
        snprintf(masked_token, sizeof(masked_token), "%.4s****", cfg->network.registration_token);
    }

#if defined(ESP_PLATFORM)
    ESP_LOGI(TAG, "=== Centralized Firmware Configuration ===");
    ESP_LOGI(TAG, "  Device ID   : %s (Board: %s, FW: %s)", cfg->device.device_id, cfg->device.board_name, cfg->device.firmware_version);
    ESP_LOGI(TAG, "  Wi-Fi SSID  : %s (Password: ********)", cfg->wifi.ssid);
    ESP_LOGI(TAG, "  Backend URL : %s (Token: %s)", cfg->network.base_url, masked_token);
    ESP_LOGI(TAG, "  DHT11 Sensor: GPIO %u (Interval: %u ms, Range: %.0f-%.0f C, %.0f-%.0f %%)",
             cfg->dht11.pin, (unsigned)cfg->dht11.sample_interval_ms,
             cfg->dht11.temp_min_c, cfg->dht11.temp_max_c,
             cfg->dht11.humidity_min_pct, cfg->dht11.humidity_max_pct);
    ESP_LOGI(TAG, "  Pump Relay  : GPIO %u (Active-%s, MaxDuration: %u s, Cooldown: %u s)",
             cfg->actuators.pin_relay_pump,
             cfg->actuators.relay_pump_active_level ? "HIGH" : "LOW",
             (unsigned)(cfg->safety.max_pump_duration_ms / 1000),
             (unsigned)(cfg->safety.min_pump_cooldown_ms / 1000));
#else
    printf("=== Centralized Firmware Configuration ===\n");
    printf("  Device ID   : %s (Board: %s, FW: %s)\n", cfg->device.device_id, cfg->device.board_name, cfg->device.firmware_version);
    printf("  Wi-Fi SSID  : %s (Password: ********)\n", cfg->wifi.ssid);
    printf("  Backend URL : %s (Token: %s)\n", cfg->network.base_url, masked_token);
    printf("  DHT11 Sensor: GPIO %u (Interval: %u ms, Range: %.0f-%.0f C, %.0f-%.0f %%)\n",
           cfg->dht11.pin, (unsigned)cfg->dht11.sample_interval_ms,
           cfg->dht11.temp_min_c, cfg->dht11.temp_max_c,
           cfg->dht11.humidity_min_pct, cfg->dht11.humidity_max_pct);
    printf("  Pump Relay  : GPIO %u (Active-%s, MaxDuration: %u s, Cooldown: %u s)\n",
           cfg->actuators.pin_relay_pump,
           cfg->actuators.relay_pump_active_level ? "HIGH" : "LOW",
           (unsigned)(cfg->safety.max_pump_duration_ms / 1000),
           (unsigned)(cfg->safety.min_pump_cooldown_ms / 1000));
#endif
}

bool firmware_config_set_device_id(const char *device_id) {
    if (!device_id || device_id[0] == '\0') return false;
    strncpy(s_global_config.device.device_id, device_id, sizeof(s_global_config.device.device_id) - 1);
    s_global_config.device.device_id[sizeof(s_global_config.device.device_id) - 1] = '\0';
    return true;
}

bool firmware_config_set_backend(const char *host, uint16_t port) {
    if (!host || host[0] == '\0' || port == 0) return false;
    strncpy(s_global_config.network.backend_host, host, sizeof(s_global_config.network.backend_host) - 1);
    s_global_config.network.backend_port = port;
    firmware_config_build_base_url(&s_global_config, s_global_config.network.base_url, sizeof(s_global_config.network.base_url));
    return true;
}

bool firmware_config_set_wifi(const char *ssid, const char *password) {
    if (!ssid || ssid[0] == '\0') return false;
    strncpy(s_global_config.wifi.ssid, ssid, sizeof(s_global_config.wifi.ssid) - 1);
    if (password) {
        strncpy(s_global_config.wifi.password, password, sizeof(s_global_config.wifi.password) - 1);
    } else {
        s_global_config.wifi.password[0] = '\0';
    }
    return true;
}

bool firmware_config_set_soil_calibration(uint16_t dry_raw, uint16_t wet_raw) {
    if (dry_raw <= wet_raw + 200) return false;
    s_global_config.sensors.soil_adc_dry_raw = dry_raw;
    s_global_config.sensors.soil_adc_wet_raw = wet_raw;
    return true;
}

bool firmware_config_set_safety_limits(uint32_t max_runtime_ms, uint32_t min_cooldown_ms) {
    if (max_runtime_ms == 0 || max_runtime_ms > 300000 || min_cooldown_ms == 0) return false;
    s_global_config.safety.max_pump_duration_ms = max_runtime_ms;
    s_global_config.safety.min_pump_cooldown_ms = min_cooldown_ms;
    return true;
}

bool firmware_config_set_control_mode(ControlMode mode) {
    if (mode < CONTROL_MODE_AUTO || mode > CONTROL_MODE_SAFE) return false;
    s_global_config.control.mode = mode;
    return firmware_config_save_nvs();
}

bool firmware_config_set_local_rule(bool enabled, float threshold_pct, uint32_t duration_s) {
    if (threshold_pct < 0.0f || threshold_pct > 100.0f || duration_s == 0 || duration_s > 120) return false;
    s_global_config.control.local_rule.enabled = enabled;
    s_global_config.control.local_rule.soil_moisture_threshold_pct = threshold_pct;
    s_global_config.control.local_rule.pump_duration_s = duration_s;
    return firmware_config_save_nvs();
}

#if defined(ESP_PLATFORM)
bool firmware_config_nvs_init(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGE(TAG, "[NVS_SAFETY_STOP] NVS partition error (%s). Enforcing SAFE mode without erasing to prevent data loss.", esp_err_to_name(err));
        s_global_config.control.mode = CONTROL_MODE_SAFE;
        return false;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[NVS_SAFETY_STOP] NVS initialization error (%s). Enforcing SAFE mode.", esp_err_to_name(err));
        s_global_config.control.mode = CONTROL_MODE_SAFE;
        return false;
    }
    return true;
}

bool firmware_config_load_nvs(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        if (err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGE(TAG, "NVS open error (%s); enforcing SAFE mode for hardware protection", esp_err_to_name(err));
            s_global_config.control.mode = CONTROL_MODE_SAFE;
        }
        return false;
    }

    uint8_t mode_val = 0;
    err = nvs_get_u8(handle, "ctrl_mode", &mode_val);
    if (err == ESP_OK) {
        if (mode_val <= (uint8_t)CONTROL_MODE_SAFE) {
            s_global_config.control.mode = (ControlMode)mode_val;
            ESP_LOGI(TAG, "Restored ControlMode from NVS: %d", (int)mode_val);
        } else {
            ESP_LOGW(TAG, "Corrupted ControlMode in NVS (%u); defaulting to SAFE mode for safety", mode_val);
            s_global_config.control.mode = CONTROL_MODE_SAFE;
        }
    }

    uint8_t rule_en = 0;
    if (nvs_get_u8(handle, "rule_en", &rule_en) == ESP_OK) {
        s_global_config.control.local_rule.enabled = (rule_en != 0);
    }

    uint32_t rule_thresh_x100 = 0;
    if (nvs_get_u32(handle, "rule_thresh", &rule_thresh_x100) == ESP_OK) {
        s_global_config.control.local_rule.soil_moisture_threshold_pct = (float)rule_thresh_x100 / 100.0f;
    }

    uint32_t rule_dur = 0;
    if (nvs_get_u32(handle, "rule_dur", &rule_dur) == ESP_OK) {
        s_global_config.control.local_rule.pump_duration_s = rule_dur;
    }

    nvs_close(handle);
    return true;
}

bool firmware_config_save_nvs(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for writing: %s", esp_err_to_name(err));
        return false;
    }

    nvs_set_u8(handle, "ctrl_mode", (uint8_t)s_global_config.control.mode);
    nvs_set_u8(handle, "rule_en", s_global_config.control.local_rule.enabled ? 1 : 0);
    nvs_set_u32(handle, "rule_thresh", (uint32_t)(s_global_config.control.local_rule.soil_moisture_threshold_pct * 100.0f));
    nvs_set_u32(handle, "rule_dur", s_global_config.control.local_rule.pump_duration_s);

    err = nvs_commit(handle);
    nvs_close(handle);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Persisted ControlMode=%d and LocalRule to NVS", (int)s_global_config.control.mode);
        return true;
    }
    return false;
}

#else
/* Desktop mock implementation of NVS */
static struct {
    bool present;
    bool corrupted;
    bool write_fail;
    uint8_t ctrl_mode;
    uint8_t rule_en;
    uint32_t rule_thresh;
    uint32_t rule_dur;
} s_mock_nvs = {0};

bool firmware_config_nvs_init(void) {
    return true;
}

bool firmware_config_load_nvs(void) {
    if (!s_mock_nvs.present) return false;
    if (s_mock_nvs.corrupted) {
        s_global_config.control.mode = CONTROL_MODE_SAFE;
        return true;
    }
    if (s_mock_nvs.ctrl_mode <= (uint8_t)CONTROL_MODE_SAFE) {
        s_global_config.control.mode = (ControlMode)s_mock_nvs.ctrl_mode;
    } else {
        s_global_config.control.mode = CONTROL_MODE_SAFE;
    }
    s_global_config.control.local_rule.enabled = (s_mock_nvs.rule_en != 0);
    s_global_config.control.local_rule.soil_moisture_threshold_pct = (float)s_mock_nvs.rule_thresh / 100.0f;
    s_global_config.control.local_rule.pump_duration_s = s_mock_nvs.rule_dur;
    return true;
}

bool firmware_config_save_nvs(void) {
    if (s_mock_nvs.write_fail) {
        return false;
    }
    s_mock_nvs.present = true;
    s_mock_nvs.corrupted = false;
    s_mock_nvs.ctrl_mode = (uint8_t)s_global_config.control.mode;
    s_mock_nvs.rule_en = s_global_config.control.local_rule.enabled ? 1 : 0;
    s_mock_nvs.rule_thresh = (uint32_t)(s_global_config.control.local_rule.soil_moisture_threshold_pct * 100.0f);
    s_mock_nvs.rule_dur = s_global_config.control.local_rule.pump_duration_s;
    return true;
}

void firmware_config_mock_nvs_clear(void) {
    memset(&s_mock_nvs, 0, sizeof(s_mock_nvs));
}

void firmware_config_mock_nvs_corrupt(void) {
    s_mock_nvs.present = true;
    s_mock_nvs.corrupted = true;
    s_mock_nvs.ctrl_mode = 0xFE;
}

void firmware_config_mock_nvs_set_write_fail(bool fail) {
    s_mock_nvs.write_fail = fail;
}
#endif

