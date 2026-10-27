/**
 * @file sensor_calibration.h
 * @brief Soil Moisture and ADC Calibration routines
 */

#ifndef SENSOR_CALIBRATION_H
#define SENSOR_CALIBRATION_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t adc_dry_raw;     /* Raw ADC reading when probe is completely dry in air (e.g. 3100) */
    uint16_t adc_wet_raw;     /* Raw ADC reading when probe is submerged in water (e.g. 1100) */
    float    vref_mv;         /* ADC reference voltage in millivolts (nominal 3300) */
    bool     is_calibrated;   /* Flag indicating calibration parameters were loaded/saved */
} SoilCalibrationConfig;

/**
 * @brief Initialize calibration with safe defaults
 */
void sensor_calibration_init(SoilCalibrationConfig *config);

/**
 * @brief Convert raw ADC reading into calibrated moisture percentage (0.0% to 100.0%)
 */
float sensor_calibration_raw_to_moisture_pct(const SoilCalibrationConfig *config, uint16_t raw_adc);

/**
 * @brief Update calibration points
 */
bool sensor_calibration_set_points(SoilCalibrationConfig *config, uint16_t dry_raw, uint16_t wet_raw);

/**
 * @brief Check whether raw ADC reading is electrically plausible (not shorted or disconnected)
 */
bool sensor_calibration_is_adc_plausible(uint16_t raw_adc);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_CALIBRATION_H */
