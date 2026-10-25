#include "sensor_manager.h"
#include "board_config.h"
#include <string.h>

static SoilCalibrationConfig s_calibration;
static bool s_mock_mode = false;
static SensorReadings s_mock_data;

void sensor_manager_init(const SoilCalibrationConfig *calibration) {
    if (calibration) {
        s_calibration = *calibration;
    } else {
        sensor_calibration_init(&s_calibration);
    }
    s_mock_mode = false;
    memset(&s_mock_data, 0, sizeof(s_mock_data));
}

void sensor_manager_mock_inject(float soil_moisture, float temp, float hum, float light, bool valid) {
    s_mock_mode = true;
    s_mock_data.soil_moisture_pct = soil_moisture;
    s_mock_data.temperature_c = temp;
    s_mock_data.humidity_pct = hum;
    s_mock_data.light_level_pct = light;
    s_mock_data.soil_valid = valid;
    s_mock_data.dht_valid = valid;
    s_mock_data.light_valid = valid;
    s_mock_data.soil_raw_adc = 2000;
    s_mock_data.light_raw_adc = 1800;
}

bool sensor_manager_read_all(SensorReadings *out_readings) {
    if (!out_readings) return false;

    if (s_mock_mode) {
        *out_readings = s_mock_data;
        return (out_readings->soil_valid && out_readings->dht_valid);
    }

#if defined(ESP_PLATFORM)
    /* ESP-IDF ADC / One-Wire driver calls here */
    // Reading ADC channels and DHT22
    out_readings->soil_valid = true;
    out_readings->dht_valid = true;
    out_readings->light_valid = true;
    return true;
#else
    /* Native desktop baseline fallback */
    out_readings->soil_moisture_pct = 42.5f;
    out_readings->temperature_c = 26.8f;
    out_readings->humidity_pct = 68.0f;
    out_readings->light_level_pct = 75.0f;
    out_readings->soil_valid = true;
    out_readings->dht_valid = true;
    out_readings->light_valid = true;
    out_readings->soil_raw_adc = 2200;
    out_readings->light_raw_adc = 1500;
    return true;
#endif
}
