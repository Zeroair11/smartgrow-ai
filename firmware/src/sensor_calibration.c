#include "sensor_calibration.h"

void sensor_calibration_init(SoilCalibrationConfig *config) {
    if (!config) return;
    /* Default factory baseline for typical capacitive soil moisture sensor v1.2 / v2.0 on 12-bit ADC */
    config->adc_dry_raw   = 3200;
    config->adc_wet_raw   = 1200;
    config->vref_mv       = 3300.0f;
    config->is_calibrated = false;
}

float sensor_calibration_raw_to_moisture_pct(const SoilCalibrationConfig *config, uint16_t raw_adc) {
    if (!config) return 0.0f;

    uint16_t dry = config->adc_dry_raw;
    uint16_t wet = config->adc_wet_raw;

    /* Guard against divide-by-zero or inverted config */
    if (dry <= wet) {
        return 0.0f;
    }

    /* Capacitive sensors output higher voltage/ADC when dry, lower when wet */
    if (raw_adc >= dry) {
        return 0.0f;
    }
    if (raw_adc <= wet) {
        return 100.0f;
    }

    float percentage = ((float)(dry - raw_adc) / (float)(dry - wet)) * 100.0f;

    if (percentage < 0.0f) percentage = 0.0f;
    if (percentage > 100.0f) percentage = 100.0f;

    return percentage;
}

bool sensor_calibration_set_points(SoilCalibrationConfig *config, uint16_t dry_raw, uint16_t wet_raw) {
    if (!config) return false;
    /* Basic validation: Dry ADC must be significantly higher than Wet ADC */
    if (dry_raw <= wet_raw + 200) {
        return false;
    }
    config->adc_dry_raw = dry_raw;
    config->adc_wet_raw = wet_raw;
    config->is_calibrated = true;
    return true;
}
