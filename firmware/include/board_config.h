/**
 * @file board_config.h
 * @brief Centralized Hardware Pinout and Safety Configurations for ESP32-S3 DevModule N16R8
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
   BOARD & HARDWARE TARGET METRICS
   ========================================================================= */
#define BOARD_NAME                   "ESP32-S3-N16R8"
#define FIRMWARE_VERSION             "1.0.0"
#define FIRMWARE_BUILD_DATE          __DATE__ " " __TIME__
#define SCHEMA_VERSION               "1.0.0"

/* =========================================================================
   GPIO PIN ALLOCATION (ESP32-S3 DevKitC-1 N16R8)
   ========================================================================= */
/* Analog Inputs (ADC1 channels preferred when Wi-Fi is active) */
#define PIN_SOIL_MOISTURE_ADC        1    /* GPIO1 (ADC1_CH0) - Analog Soil Moisture Sensor */
#define PIN_LIGHT_LDR_ADC            2    /* GPIO2 (ADC1_CH1) - Photoresistor LDR Sensor */

/* Digital Sensors */
#define PIN_DHT22_DATA               4    /* GPIO4 - DHT22 Temperature & Humidity One-Wire */

/* Actuators */
#define PIN_RELAY_PUMP               10   /* GPIO10 - Water Pump Relay */
#define PIN_SERVO_VENT               11   /* GPIO11 - PWM Servo (Greenhouse Ventilation Roof) */
#define PIN_BUZZER                   12   /* GPIO12 - Piezo Buzzer */
#define PIN_STATUS_LED               21   /* GPIO21 - On-board Status / Diagnostics LED */

/* I2C Bus for Optional OLED / Sensors (SDA, SCL) */
#define PIN_I2C_SDA                  8    /* GPIO8 */
#define PIN_I2C_SCL                  9    /* GPIO9 */

/* =========================================================================
   ACTUATOR POLARITY & ELECTRICAL CHARACTERISTICS
   ========================================================================= */
/* Most optocoupled relay modules are ACTIVE-LOW; set to false if ACTIVE-HIGH */
#define RELAY_PUMP_ACTIVE_LEVEL      0    /* 0 = Active Low, 1 = Active High */
#define STATUS_LED_ACTIVE_LEVEL      1    /* 1 = Active High */

/* =========================================================================
   HARDWARE SAFETY LIMITS (Firmware-enforced Hard Real-time Constraints)
   ========================================================================= */
#define SAFETY_MAX_PUMP_DURATION_MS  (120 * 1000)   /* Max pump continuous run: 120 seconds */
#define SAFETY_MIN_PUMP_COOLDOWN_MS  (180 * 1000)   /* Minimum cooldown between runs: 3 minutes */
#define SAFETY_OFFLINE_FAILSAFE_MS   (300 * 1000)   /* Emergency offline fallback timeout: 5 minutes */

/* Environmental Plausibility Limits for Sensor Fault Detection */
#define SENSOR_SOIL_MIN_VALID_PCT    0.0f
#define SENSOR_SOIL_MAX_VALID_PCT    100.0f
#define SENSOR_TEMP_MIN_VALID_C      -10.0f
#define SENSOR_TEMP_MAX_VALID_C      70.0f
#define SENSOR_HUM_MIN_VALID_PCT     0.0f
#define SENSOR_HUM_MAX_VALID_PCT     100.0f
#define SENSOR_LIGHT_MIN_VALID_PCT   0.0f
#define SENSOR_LIGHT_MAX_VALID_PCT   100.0f

#ifdef __cplusplus
}
#endif

#endif /* BOARD_CONFIG_H */
