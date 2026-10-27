/**
 * @file sensor_manager.c
 * @brief Sensor Acquisition, DHT11 Integration, Plausibility Checks & Fault Diagnostics Implementation
 */

#include "sensor_manager.h"
#include "board_config.h"
#include "dht11.h"
#include <string.h>
#include <math.h>

#if defined(ESP_PLATFORM)
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
static const char *TAG = "sensor_mgr";
static adc_oneshot_unit_handle_t s_adc1_handle = NULL;
#endif

static SoilCalibrationConfig s_calibration;
static bool s_mock_mode = false;
static SensorReadings s_mock_data;
static uint32_t s_consecutive_fault_count = 0;

void sensor_manager_init(const SoilCalibrationConfig *calibration) {
    if (calibration) {
        s_calibration = *calibration;
    } else {
        sensor_calibration_init(&s_calibration);
    }

    const FirmwareConfig *fw_cfg = firmware_config_get();
    dht11_init(&fw_cfg->dht11);

#if defined(ESP_PLATFORM)
    if (!s_adc1_handle) {
        adc_oneshot_unit_init_cfg_t init_config1 = {
            .unit_id = ADC_UNIT_1,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };
        esp_err_t err = adc_oneshot_new_unit(&init_config1, &s_adc1_handle);
        if (err == ESP_OK) {
            adc_oneshot_chan_cfg_t config = {
                .bitwidth = ADC_BITWIDTH_DEFAULT,
                .atten = ADC_ATTEN_DB_12,
            };
            adc_oneshot_config_channel(s_adc1_handle, ADC_CHANNEL_0, &config);
            adc_oneshot_config_channel(s_adc1_handle, ADC_CHANNEL_1, &config);
            ESP_LOGI(TAG, "ADC1 initialized (Soil=GPIO%u/CH0, LDR=GPIO%u/CH1, 12dB attenuation)",
                     fw_cfg->sensors.pin_soil_adc, fw_cfg->sensors.pin_ldr_adc);
        } else {
            ESP_LOGE(TAG, "Failed to initialize ADC1 unit: %s", esp_err_to_name(err));
            s_adc1_handle = NULL;
        }
    }
#endif

    s_mock_mode = false;
    s_consecutive_fault_count = 0;
    memset(&s_mock_data, 0, sizeof(s_mock_data));
}

void sensor_manager_mock_inject(float soil_moisture, float temp, float hum, float light, bool valid) {
    s_mock_mode = true;
    s_mock_data.soil_moisture_pct = valid ? soil_moisture : NAN;
    s_mock_data.temperature_c = valid ? temp : NAN;
    s_mock_data.humidity_pct = valid ? hum : NAN;
    s_mock_data.light_level_pct = valid ? light : NAN;
    s_mock_data.soil_valid = valid;
    s_mock_data.dht_valid = valid;
    s_mock_data.light_valid = valid;
    s_mock_data.soil_raw_adc = valid ? 2000 : 4000;
    s_mock_data.light_raw_adc = 1800;
    s_mock_data.consecutive_fault_count = valid ? 0 : 1;
}

bool sensor_manager_read_all_ex(SensorReadings *out_readings, uint32_t now_ms) {
    if (!out_readings) return false;

    if (s_mock_mode) {
        *out_readings = s_mock_data;
        if (!sensor_calibration_is_adc_plausible(out_readings->soil_raw_adc)) {
            out_readings->soil_valid = false;
            out_readings->soil_moisture_pct = NAN;
        }
        out_readings->timestamp_ms = now_ms;
        if (!out_readings->soil_valid || !out_readings->dht_valid) {
            s_consecutive_fault_count++;
        } else {
            s_consecutive_fault_count = 0;
        }
        out_readings->consecutive_fault_count = s_consecutive_fault_count;
        return (out_readings->soil_valid && out_readings->dht_valid);
    }

#if defined(ESP_PLATFORM)
    const FirmwareConfig *fw_cfg = firmware_config_get();
    uint32_t current_ms = now_ms ? now_ms : (uint32_t)(esp_timer_get_time() / 1000ULL);
    out_readings->timestamp_ms = current_ms;

    /* 1. Read DHT11 Temperature & Humidity Sensor */
    float dht_temp = NAN;
    float dht_hum = NAN;
    bool dht_ok = dht11_read(&dht_temp, &dht_hum, current_ms);

    if (dht_ok) {
        out_readings->temperature_c = dht_temp;
        out_readings->humidity_pct = dht_hum;
        out_readings->dht_valid = true;
    } else {
        out_readings->temperature_c = NAN;
        out_readings->humidity_pct = NAN;
        out_readings->dht_valid = false;
    }

    /* 2. Read Capacitive Soil Moisture ADC (ADC1_CH0 / GPIO1) */
    int raw_soil = 0;
    esp_err_t soil_adc_err = ESP_FAIL;
    if (s_adc1_handle) {
        soil_adc_err = adc_oneshot_read(s_adc1_handle, ADC_CHANNEL_0, &raw_soil);
    }

    if (soil_adc_err == ESP_OK) {
        out_readings->soil_raw_adc = (uint16_t)raw_soil;
        bool plausible = sensor_calibration_is_adc_plausible((uint16_t)raw_soil);
        out_readings->soil_valid = plausible;
        if (plausible) {
            out_readings->soil_moisture_pct = sensor_calibration_raw_to_moisture_pct(&s_calibration, (uint16_t)raw_soil);
        } else {
            out_readings->soil_moisture_pct = NAN;
            if (s_consecutive_fault_count == 1 || s_consecutive_fault_count % 5 == 0) {
                ESP_LOGW(TAG, "Soil ADC raw %d unplausible (limits: %u - %u; check wiring)",
                         raw_soil, fw_cfg->sensors.soil_raw_min_plausible, fw_cfg->sensors.soil_raw_max_plausible);
            }
        }
    } else {
        out_readings->soil_raw_adc = 0;
        out_readings->soil_valid = false;
        out_readings->soil_moisture_pct = NAN;
        if (s_consecutive_fault_count == 1 || s_consecutive_fault_count % 5 == 0) {
            ESP_LOGW(TAG, "Soil ADC1 read failed: %s", esp_err_to_name(soil_adc_err));
        }
    }

    /* 3. Read Light LDR ADC (ADC1_CH1 / GPIO2) */
    int raw_light = 0;
    esp_err_t light_adc_err = ESP_FAIL;
    if (s_adc1_handle) {
        light_adc_err = adc_oneshot_read(s_adc1_handle, ADC_CHANNEL_1, &raw_light);
    }
    if (light_adc_err == ESP_OK) {
        out_readings->light_raw_adc = (uint16_t)raw_light;
        out_readings->light_valid = true;
        out_readings->light_level_pct = ((float)(4095 - raw_light) / 4095.0f) * 100.0f;
    } else {
        out_readings->light_raw_adc = 0;
        out_readings->light_valid = false;
        out_readings->light_level_pct = NAN;
    }

    /* 4. Update Fault Tracking */
    if (!out_readings->soil_valid || !out_readings->dht_valid) {
        s_consecutive_fault_count++;
        if (s_consecutive_fault_count == 1 || s_consecutive_fault_count % 5 == 0) {
            ESP_LOGW(TAG, "Sensor acquisition fault: Soil=%s, DHT11=%s (Consecutive faults: %lu)",
                     out_readings->soil_valid ? "OK" : "FAULT",
                     out_readings->dht_valid ? "OK" : "FAULT",
                     (unsigned long)s_consecutive_fault_count);
        }
    } else {
        s_consecutive_fault_count = 0;
    }
    out_readings->consecutive_fault_count = s_consecutive_fault_count;

    return (out_readings->soil_valid && out_readings->dht_valid);

#else
    /* Native desktop baseline fallback with DHT11 driver integration */
    float dht_temp = NAN;
    float dht_hum = NAN;
    bool dht_ok = dht11_read(&dht_temp, &dht_hum, now_ms);

    out_readings->timestamp_ms = now_ms;
    if (dht_ok) {
        out_readings->temperature_c = dht_temp;
        out_readings->humidity_pct = dht_hum;
        out_readings->dht_valid = true;
    } else {
        out_readings->temperature_c = NAN;
        out_readings->humidity_pct = NAN;
        out_readings->dht_valid = false;
    }

    out_readings->soil_raw_adc = 2200;
    out_readings->soil_valid = sensor_calibration_is_adc_plausible(out_readings->soil_raw_adc);
    if (out_readings->soil_valid) {
        out_readings->soil_moisture_pct = sensor_calibration_raw_to_moisture_pct(&s_calibration, out_readings->soil_raw_adc);
    } else {
        out_readings->soil_moisture_pct = NAN;
    }

    out_readings->light_raw_adc = 1500;
    out_readings->light_valid = true;
    out_readings->light_level_pct = 75.0f;

    if (!out_readings->soil_valid || !out_readings->dht_valid) {
        s_consecutive_fault_count++;
    } else {
        s_consecutive_fault_count = 0;
    }
    out_readings->consecutive_fault_count = s_consecutive_fault_count;

    return (out_readings->soil_valid && out_readings->dht_valid);
#endif
}

bool sensor_manager_read_all(SensorReadings *out_readings) {
    return sensor_manager_read_all_ex(out_readings, 0);
}

