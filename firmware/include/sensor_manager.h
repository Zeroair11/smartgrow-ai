/**
 * @file sensor_manager.h
 * @brief Sensor Acquisition, Plausibility Checks & Fault Diagnostics
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "sensor_calibration.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float    soil_moisture_pct;
    float    temperature_c;
    float    humidity_pct;
    float    light_level_pct;
    uint16_t soil_raw_adc;
    uint16_t light_raw_adc;
    bool     soil_valid;
    bool     dht_valid;
    bool     light_valid;
    uint32_t timestamp_ms;
    uint32_t consecutive_fault_count;
} SensorReadings;

/**
 * @brief Initialize all sensor peripherals and ADC configuration
 */
void sensor_manager_init(const SoilCalibrationConfig *calibration);

/**
 * @brief Acquire a full set of sensor readings and perform plausibility checks
 */
bool sensor_manager_read_all(SensorReadings *out_readings);

/**
 * @brief Inject mocked sensor readings (used for native unit testing and HIL simulation)
 */
void sensor_manager_mock_inject(float soil_moisture, float temp, float hum, float light, bool valid);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_MANAGER_H */
