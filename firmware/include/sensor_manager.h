/**
 * @file sensor_manager.h
 * @brief Sensor Acquisition, DHT11 Integration, Plausibility Checks & Fault Diagnostics
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "sensor_calibration.h"
#include "dht11.h"

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
 * @brief Initialize all sensor peripherals, ADC calibration and DHT11 driver
 */
void sensor_manager_init(const SoilCalibrationConfig *calibration);

/**
 * @brief Acquire a full set of sensor readings and perform plausibility checks
 * @param out_readings Output structure for all sensor data and validity flags
 * @return true if ALL critical sensors (soil moisture AND DHT11) are valid
 */
bool sensor_manager_read_all(SensorReadings *out_readings);

/**
 * @brief Acquire sensor readings with explicit system timestamp
 * @param out_readings Output structure
 * @param now_ms Current system timestamp in milliseconds
 * @return true if ALL critical sensors are valid
 */
bool sensor_manager_read_all_ex(SensorReadings *out_readings, uint32_t now_ms);

/**
 * @brief Inject mocked sensor readings (used for native unit testing and HIL simulation)
 */
void sensor_manager_mock_inject(float soil_moisture, float temp, float hum, float light, bool valid);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_MANAGER_H */
