/**
 * @file board_config.h
 * @brief Hardware Pinout, DHT11 Sensor & Safety Aliases for ESP32-S3 DevKitC-1 N16R8
 *
 * NOTE: The primary source of truth is 'project_config.h'.
 * This header maps hardware board aliases to 'project_config.h' symbols.
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "project_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
   BOARD & HARDWARE TARGET METRICS
   ========================================================================= */
#define BOARD_NAME                   PROJECT_BOARD_NAME
#define FIRMWARE_VERSION             PROJECT_FIRMWARE_VERSION
#define FIRMWARE_BUILD_DATE          PROJECT_FIRMWARE_BUILD_DATE
#define SCHEMA_VERSION               PROJECT_SCHEMA_VERSION

/* =========================================================================
   GPIO PIN ALLOCATION (ESP32-S3 DevKitC-1 N16R8)
   ========================================================================= */
/* Analog Inputs (ADC1 channels only) */
#define PIN_SOIL_MOISTURE_ADC        PROJECT_PIN_SOIL_ADC   /* GPIO1 (ADC1_CH0) - Capacitive Soil Moisture */
#define PIN_LIGHT_LDR_ADC            PROJECT_PIN_LDR_ADC    /* GPIO2 (ADC1_CH1) - Photoresistor LDR */

/* Digital Sensors - DHT11 Temperature & Humidity Single-Wire */
#define PIN_DHT11_DATA               PROJECT_PIN_DHT11      /* GPIO4 - DHT11 Single-Wire Bus */
#define PIN_DHT22_DATA               PIN_DHT11_DATA         /* Backward compatibility alias */

/* Actuators */
#define PIN_RELAY_PUMP               PROJECT_PIN_RELAY_PUMP /* GPIO10 - Water Pump Relay */
#define PIN_SERVO_VENT               PROJECT_PIN_SERVO_VENT /* GPIO11 - PWM Servo (Ventilation Roof) */
#define PIN_BUZZER                   PROJECT_PIN_BUZZER     /* GPIO12 - Piezo Buzzer */
#define PIN_STATUS_LED               PROJECT_PIN_STATUS_LED /* GPIO21 - On-board Status / Diagnostics LED */

/* I2C Bus for Optional OLED / Sensors (SDA, SCL) */
#define PIN_I2C_SDA                  PROJECT_PIN_I2C_SDA    /* GPIO8 */
#define PIN_I2C_SCL                  PROJECT_PIN_I2C_SCL    /* GPIO9 */

/* =========================================================================
   ACTUATOR POLARITY & ELECTRICAL CHARACTERISTICS
   ========================================================================= */
#define RELAY_PUMP_ACTIVE_LEVEL      PROJECT_RELAY_PUMP_ACTIVE_LEVEL /* 0 = Active Low, 1 = Active High */
#define STATUS_LED_ACTIVE_LEVEL      PROJECT_STATUS_LED_ACTIVE_LEVEL /* 1 = Active High */

/* =========================================================================
   HARDWARE SAFETY LIMITS (Firmware-enforced Hard Real-time Constraints)
   ========================================================================= */
#define SAFETY_MAX_PUMP_DURATION_MS  PROJECT_SAFETY_MAX_PUMP_DURATION_MS /* 120s max runtime */
#define SAFETY_MIN_PUMP_COOLDOWN_MS  PROJECT_SAFETY_MIN_PUMP_COOLDOWN_MS /* 180s cooldown */
#define SAFETY_OFFLINE_FAILSAFE_MS   PROJECT_SAFETY_OFFLINE_FAILSAFE_MS  /* 300s failsafe */

/* Environmental Plausibility Limits for DHT11 and Sensors */
#define SENSOR_SOIL_MIN_VALID_PCT    PROJECT_SENSOR_SOIL_MIN_VALID_PCT
#define SENSOR_SOIL_MAX_VALID_PCT    PROJECT_SENSOR_SOIL_MAX_VALID_PCT
#define SENSOR_TEMP_MIN_VALID_C      PROJECT_DHT11_TEMP_MIN_C
#define SENSOR_TEMP_MAX_VALID_C      PROJECT_DHT11_TEMP_MAX_C
#define SENSOR_HUM_MIN_VALID_PCT     PROJECT_DHT11_HUMIDITY_MIN_PCT
#define SENSOR_HUM_MAX_VALID_PCT     PROJECT_DHT11_HUMIDITY_MAX_PCT
#define SENSOR_LIGHT_MIN_VALID_PCT   PROJECT_SENSOR_LIGHT_MIN_VALID_PCT
#define SENSOR_LIGHT_MAX_VALID_PCT   PROJECT_SENSOR_LIGHT_MAX_VALID_PCT

#ifdef __cplusplus
}
#endif

#endif /* BOARD_CONFIG_H */
