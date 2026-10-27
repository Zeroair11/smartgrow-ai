/**
 * @file project_config.h
 * @brief Master Centralized Compile-Time Configuration for SmartGrow AI
 * Firmware
 *
 * Target Hardware : ESP32-S3 DevKitC-1 N16R8 (16MB Quad-SPI Flash, 8MB Octal
 * PSRAM) Framework       : ESP-IDF v5.x / Native GCC Simulation
 *
 * ARCHITECTURAL PRINCIPLE:
 * This header serves as the SINGLE AUTHORITATIVE SOURCE OF TRUTH for all
 * default compile-time settings, GPIO pin mappings, safety limits, sensor
 * parameters, network timing, and TinyML constants.
 *
 * CONFIGURATION PRECEDENCE HIERARCHY:
 * 1. Compile-Time Defaults (Defined in this file)
 * 2. Optional Developer Lab Overrides (via 'config_local.h' - git-ignored)
 * 3. Non-Volatile Storage (NVS persisted user adjustments)
 * 4. Backend Cloud Updates (Downlink REST / OTA configuration commands)
 *
 * SAFETY INVARIANT:
 * No remote command or NVS override may violate the hard safety limits defined
 * in SECTION 4 (e.g. pump duration ceiling of 120s and cooldown of 180s).
 */

#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
   OPTIONAL LOCAL DEVELOPER OVERRIDES
   ========================================================================= */
#if __has_include("config_local.h")
#include "config_local.h"
#endif

/* =========================================================================
   SECTION 1: HARDWARE TARGET & DEVICE IDENTITY
   ========================================================================= */
#ifndef PROJECT_BOARD_NAME
#define PROJECT_BOARD_NAME "ESP32-S3-N16R8"
#endif

#if defined(DEFAULT_DEVICE_ID) && !defined(PROJECT_DEVICE_ID_DEFAULT)
#define PROJECT_DEVICE_ID_DEFAULT DEFAULT_DEVICE_ID
#endif
#ifndef PROJECT_DEVICE_ID_DEFAULT
#define PROJECT_DEVICE_ID_DEFAULT "esp32s3-default"
#endif

#ifndef PROJECT_DEVICE_NAME_DEFAULT
#define PROJECT_DEVICE_NAME_DEFAULT "SmartGrow-Node"
#endif

#ifndef PROJECT_FIRMWARE_VERSION
#define PROJECT_FIRMWARE_VERSION "1.0.0"
#endif

#ifndef PROJECT_FIRMWARE_BUILD_DATE
#define PROJECT_FIRMWARE_BUILD_DATE __DATE__ " " __TIME__
#endif

#ifndef PROJECT_SCHEMA_VERSION
#define PROJECT_SCHEMA_VERSION "1.0.0"
#endif

/* =========================================================================
   SECTION 2: GPIO PIN ALLOCATION (ESP32-S3 DevKitC-1 N16R8)
   Note: Pins 33-37 and 15-18 are reserved for Octal Flash / Octal PSRAM.
   ADC1 channels (GPIO 1-10) are used because ADC2 is shared with Wi-Fi.
   ========================================================================= */
/* Analog Inputs (ADC1 only) */
#ifndef PROJECT_PIN_SOIL_ADC
#define PROJECT_PIN_SOIL_ADC                                                   \
  1 /* GPIO1 (ADC1_CH0) - Capacitive Soil Moisture                             \
     */
#endif

#ifndef PROJECT_PIN_LDR_ADC
#define PROJECT_PIN_LDR_ADC 2 /* GPIO2 (ADC1_CH1) - Photoresistor LDR */
#endif

/* Digital Sensor Bus */
#ifndef PROJECT_PIN_DHT11
#define PROJECT_PIN_DHT11 4 /* GPIO4 - Single-Wire Digital Bus (DHT11) */
#endif

/* I2C Peripheral Bus (Optional OLED / Digital Environmental Sensors) */
#ifndef PROJECT_PIN_I2C_SDA
#define PROJECT_PIN_I2C_SDA 8 /* GPIO8 - I2C SDA */
#endif

#ifndef PROJECT_PIN_I2C_SCL
#define PROJECT_PIN_I2C_SCL 9 /* GPIO9 - I2C SCL */
#endif

/* Actuator Outputs */
#ifndef PROJECT_PIN_RELAY_PUMP
#define PROJECT_PIN_RELAY_PUMP 10 /* GPIO10 - Water Pump Relay Control */
#endif

#ifndef PROJECT_PIN_SERVO_VENT
#define PROJECT_PIN_SERVO_VENT 11 /* GPIO11 - PWM Servo (Ventilation Flap) */
#endif

#ifndef PROJECT_PIN_BUZZER
#define PROJECT_PIN_BUZZER 12 /* GPIO12 - Audible Piezo Buzzer */
#endif

#ifndef PROJECT_PIN_STATUS_LED
#define PROJECT_PIN_STATUS_LED                                                 \
  21 /* GPIO21 - On-Board Diagnostic Status LED                                \
      */
#endif

/* =========================================================================
   SECTION 3: ACTUATOR ELECTRICAL CHARACTERISTICS & POLARITY
   ========================================================================= */
/**
 * Relay Polarity:
 * 0 = ACTIVE-LOW (Common for optocoupled Songle relay modules)
 * 1 = ACTIVE-HIGH (Direct logic-level transistor / modern solid-state relay)
 * Default: 1 (Active-High). Pre-flight multimeter verification required.
 */
#ifndef PROJECT_RELAY_PUMP_ACTIVE_LEVEL
#define PROJECT_RELAY_PUMP_ACTIVE_LEVEL 1
#endif

/**
 * Diagnostic LED Polarity:
 * 1 = ACTIVE-HIGH (LED illuminates on logic 1 / 3.3V)
 */
#ifndef PROJECT_STATUS_LED_ACTIVE_LEVEL
#define PROJECT_STATUS_LED_ACTIVE_LEVEL 1
#endif

/* =========================================================================
   SECTION 4: HARDWARE SAFETY LIMITS & SAFETY SUPERVISOR GUARDRAILS
   Firmware-enforced hard real-time constraints; cannot be exceeded by remote
   cmds.
   ========================================================================= */
#ifndef PROJECT_SAFETY_MAX_PUMP_DURATION_MS
#define PROJECT_SAFETY_MAX_PUMP_DURATION_MS                                    \
  (120 * 1000) /* Max continuous pump run: 120s */
#endif

#ifndef PROJECT_SAFETY_MIN_PUMP_COOLDOWN_MS
#define PROJECT_SAFETY_MIN_PUMP_COOLDOWN_MS                                    \
  (180 * 1000) /* Min cooldown between cycles: 180s */
#endif

#ifndef PROJECT_SAFETY_OFFLINE_FAILSAFE_MS
#define PROJECT_SAFETY_OFFLINE_FAILSAFE_MS                                     \
  (300 * 1000) /* Cloud heartbeat failsafe: 300s */
#endif

#ifndef PROJECT_SAFETY_MIN_SAFE_TEMP_C
#define PROJECT_SAFETY_MIN_SAFE_TEMP_C 0.0f /* Freezing floor: 0.0°C */
#endif

#ifndef PROJECT_SAFETY_MAX_SAFE_TEMP_C
#define PROJECT_SAFETY_MAX_SAFE_TEMP_C 50.0f /* Heat ceiling: 50.0°C */
#endif

/* =========================================================================
   SECTION 5: ENVIRONMENTAL PLAUSIBILITY BOUNDARIES
   ========================================================================= */
#define PROJECT_SENSOR_SOIL_MIN_VALID_PCT 0.0f
#define PROJECT_SENSOR_SOIL_MAX_VALID_PCT 100.0f
#define PROJECT_SENSOR_LIGHT_MIN_VALID_PCT 0.0f
#define PROJECT_SENSOR_LIGHT_MAX_VALID_PCT 100.0f

/* =========================================================================
   SECTION 6: DHT11 TEMPERATURE & HUMIDITY SENSOR PARAMETERS
   ========================================================================= */
#ifndef PROJECT_DHT11_SAMPLE_INTERVAL_MS
#define PROJECT_DHT11_SAMPLE_INTERVAL_MS 2000 /* >= 1000ms; 2000ms nominal */
#endif

#ifndef PROJECT_DHT11_READ_TIMEOUT_MS
#define PROJECT_DHT11_READ_TIMEOUT_MS 100 /* Protocol read timeout in ms */
#endif

#ifndef PROJECT_DHT11_TEMP_MIN_C
#define PROJECT_DHT11_TEMP_MIN_C                                               \
  0.0f /* DHT11 rated nominal min temperature                                  \
        */
#endif

#ifndef PROJECT_DHT11_TEMP_MAX_C
#define PROJECT_DHT11_TEMP_MAX_C                                               \
  50.0f /* DHT11 rated nominal max temperature                                 \
         */
#endif

#ifndef PROJECT_DHT11_HUMIDITY_MIN_PCT
#define PROJECT_DHT11_HUMIDITY_MIN_PCT                                         \
  20.0f /* DHT11 rated nominal min humidity */
#endif

#ifndef PROJECT_DHT11_HUMIDITY_MAX_PCT
#define PROJECT_DHT11_HUMIDITY_MAX_PCT                                         \
  90.0f /* DHT11 rated nominal max humidity */
#endif

#ifndef PROJECT_DHT11_MAX_CONSECUTIVE_ERRORS
#define PROJECT_DHT11_MAX_CONSECUTIVE_ERRORS 3 /* Fault latch threshold */
#endif

/* =========================================================================
   SECTION 7: CAPACITIVE SOIL MOISTURE SENSOR & ADC CALIBRATION
   ========================================================================= */
#ifndef PROJECT_SOIL_ADC_DRY_RAW
#define PROJECT_SOIL_ADC_DRY_RAW 3200 /* In ambient air (~0% moisture) */
#endif

#ifndef PROJECT_SOIL_ADC_WET_RAW
#define PROJECT_SOIL_ADC_WET_RAW                                               \
  1200 /* Submerged in water (~100% moisture)                                  \
        */
#endif

#ifndef PROJECT_ADC_VREF_MV
#define PROJECT_ADC_VREF_MV 3300.0f /* ADC reference voltage in mV */
#endif

#ifndef PROJECT_SOIL_RAW_MIN_PLAUSIBLE
#define PROJECT_SOIL_RAW_MIN_PLAUSIBLE                                         \
  150 /* Below this indicates GND short circuit */
#endif

#ifndef PROJECT_SOIL_RAW_MAX_PLAUSIBLE
#define PROJECT_SOIL_RAW_MAX_PLAUSIBLE                                         \
  3850 /* Above this indicates disconnected probe */
#endif

#ifndef PROJECT_SENSOR_SAMPLE_INTERVAL_MS
#define PROJECT_SENSOR_SAMPLE_INTERVAL_MS                                      \
  2000 /* Sensor task sampling interval */
#endif

/* =========================================================================
   SECTION 8: CONTROL MODES & AUTONOMOUS OFFLINE EXECUTION POLICIES
   ========================================================================= */
#ifndef PROJECT_CONTROL_MODE_DEFAULT
#define PROJECT_CONTROL_MODE_DEFAULT                                           \
  0 /* 0 = CONTROL_MODE_AUTO (AI + Safety Gate) */
#endif

#ifndef PROJECT_OFFLINE_AUTONOMOUS_ENABLED
#define PROJECT_OFFLINE_AUTONOMOUS_ENABLED                                     \
  true /* Continue operation if backend offline */
#endif

#ifndef PROJECT_LOCAL_RULE_ENABLED
#define PROJECT_LOCAL_RULE_ENABLED true /* Local threshold fallback rule */
#endif

#ifndef PROJECT_LOCAL_RULE_SOIL_THRESHOLD_PCT
#define PROJECT_LOCAL_RULE_SOIL_THRESHOLD_PCT                                  \
  30.0f /* Irrigation trigger threshold (%) */
#endif

#ifndef PROJECT_LOCAL_RULE_PUMP_DURATION_S
#define PROJECT_LOCAL_RULE_PUMP_DURATION_S                                     \
  30 /* Target pump runtime in seconds */
#endif

#ifndef PROJECT_LOCAL_RULE_COOLDOWN_S
#define PROJECT_LOCAL_RULE_COOLDOWN_S 180 /* Minimum cooldown in seconds */
#endif

/* =========================================================================
   SECTION 9: EDGE AI / TINYML DECISION TREE CONFIGURATION
   ========================================================================= */
#ifndef PROJECT_AI_MODEL_VERSION
#define PROJECT_AI_MODEL_VERSION "v1.0.0-dt"
#endif

#ifndef PROJECT_AI_CONFIDENCE_THRESHOLD
#define PROJECT_AI_CONFIDENCE_THRESHOLD                                        \
  0.50f /* Minimum confidence for AI action */
#endif

#ifndef PROJECT_AI_INFERENCE_INTERVAL_MS
#define PROJECT_AI_INFERENCE_INTERVAL_MS                                       \
  2000 /* Period between AI inferences                                         \
        */
#endif

#ifndef PROJECT_AI_NUM_CLASSES
#define PROJECT_AI_NUM_CLASSES 5
#endif

#ifndef PROJECT_AI_NUM_FEATURES
#define PROJECT_AI_NUM_FEATURES 4
#endif

/* =========================================================================
   SECTION 10: WI-FI NETWORKING & EXPONENTIAL BACKOFF POLICY
   ========================================================================= */
#if defined(DEFAULT_WIFI_SSID) && !defined(PROJECT_WIFI_SSID_DEFAULT)
#define PROJECT_WIFI_SSID_DEFAULT DEFAULT_WIFI_SSID
#endif
#ifndef PROJECT_WIFI_SSID_DEFAULT
#define PROJECT_WIFI_SSID_DEFAULT "SmartGrow_Lab"
#endif

#if defined(DEFAULT_WIFI_PASSWORD) && !defined(PROJECT_WIFI_PASSWORD_DEFAULT)
#define PROJECT_WIFI_PASSWORD_DEFAULT DEFAULT_WIFI_PASSWORD
#endif
#ifndef PROJECT_WIFI_PASSWORD_DEFAULT
#define PROJECT_WIFI_PASSWORD_DEFAULT "" /* Empty default for security */
#endif

#ifndef PROJECT_WIFI_MAX_RETRY
#define PROJECT_WIFI_MAX_RETRY 0 /* 0 = infinite exponential backoff */
#endif

#ifndef PROJECT_WIFI_INITIAL_BACKOFF_MS
#define PROJECT_WIFI_INITIAL_BACKOFF_MS 1000 /* Initial backoff delay (1s) */
#endif

#ifndef PROJECT_WIFI_MAX_BACKOFF_MS
#define PROJECT_WIFI_MAX_BACKOFF_MS 30000 /* Maximum backoff ceiling (30s) */
#endif

#ifndef PROJECT_WIFI_CONNECT_TIMEOUT_MS
#define PROJECT_WIFI_CONNECT_TIMEOUT_MS                                        \
  15000 /* Connect attempt timeout (15s) */
#endif

/* =========================================================================
   SECTION 11: BACKEND REST API, POLLING & UPLINK TIMING
   ========================================================================= */
#if defined(DEFAULT_BACKEND_HOST) && !defined(PROJECT_BACKEND_HOST_DEFAULT)
#define PROJECT_BACKEND_HOST_DEFAULT DEFAULT_BACKEND_HOST
#endif
#ifndef PROJECT_BACKEND_HOST_DEFAULT
#define PROJECT_BACKEND_HOST_DEFAULT "192.168.1.2"
#endif

#if defined(DEFAULT_BACKEND_PORT) && !defined(PROJECT_BACKEND_PORT_DEFAULT)
#define PROJECT_BACKEND_PORT_DEFAULT DEFAULT_BACKEND_PORT
#endif
#ifndef PROJECT_BACKEND_PORT_DEFAULT
#define PROJECT_BACKEND_PORT_DEFAULT 8000
#endif

#ifndef PROJECT_API_BASE_PATH_DEFAULT
#define PROJECT_API_BASE_PATH_DEFAULT "/api/v1"
#endif

#ifndef PROJECT_REGISTRATION_TOKEN_DEFAULT
#define PROJECT_REGISTRATION_TOKEN_DEFAULT                                     \
  "smartgrow-device-enrollment-secret-2026"
#endif

#ifndef PROJECT_HTTP_TIMEOUT_MS
#define PROJECT_HTTP_TIMEOUT_MS 5000 /* HTTP request timeout (5s) */
#endif

#ifndef PROJECT_TELEMETRY_INTERVAL_MS
#define PROJECT_TELEMETRY_INTERVAL_MS 5000 /* Telemetry uplink period (5s) */
#endif

#ifndef PROJECT_COMMAND_POLL_INTERVAL_MS
#define PROJECT_COMMAND_POLL_INTERVAL_MS 3000 /* Command poll period (3s) */
#endif

#ifndef PROJECT_PAYLOAD_SIZE_LIMIT
#define PROJECT_PAYLOAD_SIZE_LIMIT 2048 /* Max HTTP JSON payload buffer */
#endif

/* =========================================================================
   SECTION 12: OTA FIRMWARE UPDATES & SYSTEM DIAGNOSTICS
   ========================================================================= */
#ifndef PROJECT_OTA_ENABLED
#define PROJECT_OTA_ENABLED true
#endif

#ifndef PROJECT_OTA_CHECK_INTERVAL_MS
#define PROJECT_OTA_CHECK_INTERVAL_MS                                          \
  (60 * 1000) /* Periodic OTA check: 60s                                       \
               */
#endif

#ifndef PROJECT_OTA_TIMEOUT_MS
#define PROJECT_OTA_TIMEOUT_MS 10000 /* OTA download timeout: 10s */
#endif

#ifndef PROJECT_SERIAL_BAUD_RATE
#define PROJECT_SERIAL_BAUD_RATE 115200 /* Debug console baud rate */
#endif

#ifndef PROJECT_LOG_LEVEL
#define PROJECT_LOG_LEVEL 3 /* 3 = ESP_LOG_INFO */
#endif

/* =========================================================================
   SECTION 13: BUFFER CAPACITIES & QUEUE LIMITS
   ========================================================================= */
#ifndef PROJECT_IDEMPOTENCY_HISTORY_SIZE
#define PROJECT_IDEMPOTENCY_HISTORY_SIZE                                       \
  8 /* Duplicate filter ring buffer size */
#endif

#ifndef PROJECT_COMMAND_BATCH_LIMIT
#define PROJECT_COMMAND_BATCH_LIMIT 4 /* Max commands processed per poll */
#endif

#ifndef PROJECT_NVS_NAMESPACE
#define PROJECT_NVS_NAMESPACE "smartgrow_cfg"
#endif

/* =========================================================================
   SECTION 14: COMPILE-TIME STATIC SANITY CHECKS (C11 Static Assertions)
   ========================================================================= */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L) &&              \
    !defined(__cplusplus)
/* Verify GPIO Pin Distinctness (Collision Detection) */
_Static_assert(PROJECT_PIN_SOIL_ADC != PROJECT_PIN_LDR_ADC,
               "Pin collision: Soil ADC and LDR ADC");
_Static_assert(PROJECT_PIN_SOIL_ADC != PROJECT_PIN_DHT11,
               "Pin collision: Soil ADC and DHT11");
_Static_assert(PROJECT_PIN_SOIL_ADC != PROJECT_PIN_RELAY_PUMP,
               "Pin collision: Soil ADC and Relay Pump");
_Static_assert(PROJECT_PIN_DHT11 != PROJECT_PIN_RELAY_PUMP,
               "Pin collision: DHT11 and Relay Pump");
_Static_assert(PROJECT_PIN_RELAY_PUMP != PROJECT_PIN_SERVO_VENT,
               "Pin collision: Relay Pump and Servo Vent");
_Static_assert(PROJECT_PIN_RELAY_PUMP != PROJECT_PIN_BUZZER,
               "Pin collision: Relay Pump and Buzzer");
_Static_assert(PROJECT_PIN_RELAY_PUMP != PROJECT_PIN_STATUS_LED,
               "Pin collision: Relay Pump and Status LED");

/* Verify ESP32-S3 Flash/PSRAM Pin Safety (Excluding Octal bus pins 33-37 &
 * 15-18) */
_Static_assert(
    PROJECT_PIN_RELAY_PUMP != 15 && PROJECT_PIN_RELAY_PUMP != 16 &&
        PROJECT_PIN_RELAY_PUMP != 17 && PROJECT_PIN_RELAY_PUMP != 18,
    "CRITICAL: Relay Pump cannot be on Pins 15-18 (Octal PSRAM / Flash)");
_Static_assert(PROJECT_PIN_RELAY_PUMP < 33 || PROJECT_PIN_RELAY_PUMP > 37,
               "CRITICAL: Relay Pump cannot be on Pins 33-37 (Octal SPI bus)");

/* Verify Safety Limits Sanity */
_Static_assert(PROJECT_SAFETY_MAX_PUMP_DURATION_MS > 0 &&
                   PROJECT_SAFETY_MAX_PUMP_DURATION_MS <= 300000,
               "Safety max pump duration must be between 1ms and 300000ms");
_Static_assert(PROJECT_SAFETY_MIN_PUMP_COOLDOWN_MS > 0,
               "Safety pump cooldown cannot be 0ms");
_Static_assert(PROJECT_DHT11_SAMPLE_INTERVAL_MS >= 1000,
               "DHT11 physical sampling interval must be >= 1000ms");
_Static_assert(PROJECT_SOIL_ADC_DRY_RAW > PROJECT_SOIL_ADC_WET_RAW + 200,
               "Soil Dry ADC must exceed Wet ADC by at least 200 counts");
_Static_assert(PROJECT_SOIL_RAW_MIN_PLAUSIBLE < PROJECT_SOIL_RAW_MAX_PLAUSIBLE,
               "Soil plausible min must be less than plausible max");
#endif

/* =========================================================================
   SECTION 15: BACKWARD-COMPATIBILITY ALIASES
   Maps legacy DEFAULT_* and PIN_* macro symbols to the central PROJECT_*
   symbols.
   ========================================================================= */
#ifndef DEFAULT_BOARD_NAME
#define DEFAULT_BOARD_NAME PROJECT_BOARD_NAME
#endif
#ifndef DEFAULT_DEVICE_ID
#define DEFAULT_DEVICE_ID PROJECT_DEVICE_ID_DEFAULT
#endif
#ifndef DEFAULT_DEVICE_NAME
#define DEFAULT_DEVICE_NAME PROJECT_DEVICE_NAME_DEFAULT
#endif
#ifndef DEFAULT_FIRMWARE_VERSION
#define DEFAULT_FIRMWARE_VERSION PROJECT_FIRMWARE_VERSION
#endif
#ifndef DEFAULT_SCHEMA_VERSION
#define DEFAULT_SCHEMA_VERSION PROJECT_SCHEMA_VERSION
#endif

#ifndef DEFAULT_PIN_SOIL_ADC
#define DEFAULT_PIN_SOIL_ADC PROJECT_PIN_SOIL_ADC
#endif
#ifndef DEFAULT_PIN_LDR_ADC
#define DEFAULT_PIN_LDR_ADC PROJECT_PIN_LDR_ADC
#endif
#ifndef DEFAULT_PIN_DHT11
#define DEFAULT_PIN_DHT11 PROJECT_PIN_DHT11
#endif
#ifndef DEFAULT_PIN_I2C_SDA
#define DEFAULT_PIN_I2C_SDA PROJECT_PIN_I2C_SDA
#endif
#ifndef DEFAULT_PIN_I2C_SCL
#define DEFAULT_PIN_I2C_SCL PROJECT_PIN_I2C_SCL
#endif
#ifndef DEFAULT_PIN_RELAY_PUMP
#define DEFAULT_PIN_RELAY_PUMP PROJECT_PIN_RELAY_PUMP
#endif
#ifndef DEFAULT_PIN_SERVO_VENT
#define DEFAULT_PIN_SERVO_VENT PROJECT_PIN_SERVO_VENT
#endif
#ifndef DEFAULT_PIN_BUZZER
#define DEFAULT_PIN_BUZZER PROJECT_PIN_BUZZER
#endif
#ifndef DEFAULT_PIN_STATUS_LED
#define DEFAULT_PIN_STATUS_LED PROJECT_PIN_STATUS_LED
#endif

#ifndef DEFAULT_RELAY_PUMP_ACTIVE_LEVEL
#define DEFAULT_RELAY_PUMP_ACTIVE_LEVEL PROJECT_RELAY_PUMP_ACTIVE_LEVEL
#endif
#ifndef DEFAULT_STATUS_LED_ACTIVE_LEVEL
#define DEFAULT_STATUS_LED_ACTIVE_LEVEL PROJECT_STATUS_LED_ACTIVE_LEVEL
#endif

#ifndef DEFAULT_SAFETY_MAX_PUMP_DURATION_MS
#define DEFAULT_SAFETY_MAX_PUMP_DURATION_MS PROJECT_SAFETY_MAX_PUMP_DURATION_MS
#endif
#ifndef DEFAULT_SAFETY_MIN_PUMP_COOLDOWN_MS
#define DEFAULT_SAFETY_MIN_PUMP_COOLDOWN_MS PROJECT_SAFETY_MIN_PUMP_COOLDOWN_MS
#endif
#ifndef DEFAULT_SAFETY_OFFLINE_FAILSAFE_MS
#define DEFAULT_SAFETY_OFFLINE_FAILSAFE_MS PROJECT_SAFETY_OFFLINE_FAILSAFE_MS
#endif
#ifndef DEFAULT_SAFETY_MIN_SAFE_TEMP_C
#define DEFAULT_SAFETY_MIN_SAFE_TEMP_C PROJECT_SAFETY_MIN_SAFE_TEMP_C
#endif
#ifndef DEFAULT_SAFETY_MAX_SAFE_TEMP_C
#define DEFAULT_SAFETY_MAX_SAFE_TEMP_C PROJECT_SAFETY_MAX_SAFE_TEMP_C
#endif

#ifndef DEFAULT_DHT11_SAMPLE_INTERVAL_MS
#define DEFAULT_DHT11_SAMPLE_INTERVAL_MS PROJECT_DHT11_SAMPLE_INTERVAL_MS
#endif
#ifndef DEFAULT_DHT11_READ_TIMEOUT_MS
#define DEFAULT_DHT11_READ_TIMEOUT_MS PROJECT_DHT11_READ_TIMEOUT_MS
#endif
#ifndef DEFAULT_DHT11_TEMP_MIN_C
#define DEFAULT_DHT11_TEMP_MIN_C PROJECT_DHT11_TEMP_MIN_C
#endif
#ifndef DEFAULT_DHT11_TEMP_MAX_C
#define DEFAULT_DHT11_TEMP_MAX_C PROJECT_DHT11_TEMP_MAX_C
#endif
#ifndef DEFAULT_DHT11_HUMIDITY_MIN_PCT
#define DEFAULT_DHT11_HUMIDITY_MIN_PCT PROJECT_DHT11_HUMIDITY_MIN_PCT
#endif
#ifndef DEFAULT_DHT11_HUMIDITY_MAX_PCT
#define DEFAULT_DHT11_HUMIDITY_MAX_PCT PROJECT_DHT11_HUMIDITY_MAX_PCT
#endif
#ifndef DEFAULT_DHT11_MAX_CONSECUTIVE_ERRORS
#define DEFAULT_DHT11_MAX_CONSECUTIVE_ERRORS                                   \
  PROJECT_DHT11_MAX_CONSECUTIVE_ERRORS
#endif

#ifndef DEFAULT_SOIL_ADC_DRY_RAW
#define DEFAULT_SOIL_ADC_DRY_RAW PROJECT_SOIL_ADC_DRY_RAW
#endif
#ifndef DEFAULT_SOIL_ADC_WET_RAW
#define DEFAULT_SOIL_ADC_WET_RAW PROJECT_SOIL_ADC_WET_RAW
#endif
#ifndef DEFAULT_ADC_VREF_MV
#define DEFAULT_ADC_VREF_MV PROJECT_ADC_VREF_MV
#endif
#ifndef DEFAULT_SOIL_RAW_MIN_PLAUSIBLE
#define DEFAULT_SOIL_RAW_MIN_PLAUSIBLE PROJECT_SOIL_RAW_MIN_PLAUSIBLE
#endif
#ifndef DEFAULT_SOIL_RAW_MAX_PLAUSIBLE
#define DEFAULT_SOIL_RAW_MAX_PLAUSIBLE PROJECT_SOIL_RAW_MAX_PLAUSIBLE
#endif
#ifndef DEFAULT_SENSOR_SAMPLE_INTERVAL_MS
#define DEFAULT_SENSOR_SAMPLE_INTERVAL_MS PROJECT_SENSOR_SAMPLE_INTERVAL_MS
#endif

#ifndef DEFAULT_CONTROL_MODE
#define DEFAULT_CONTROL_MODE PROJECT_CONTROL_MODE_DEFAULT
#endif
#ifndef DEFAULT_OFFLINE_AUTONOMOUS_ENABLED
#define DEFAULT_OFFLINE_AUTONOMOUS_ENABLED PROJECT_OFFLINE_AUTONOMOUS_ENABLED
#endif
#ifndef DEFAULT_LOCAL_RULE_ENABLED
#define DEFAULT_LOCAL_RULE_ENABLED PROJECT_LOCAL_RULE_ENABLED
#endif
#ifndef DEFAULT_LOCAL_RULE_SOIL_THRESHOLD_PCT
#define DEFAULT_LOCAL_RULE_SOIL_THRESHOLD_PCT                                  \
  PROJECT_LOCAL_RULE_SOIL_THRESHOLD_PCT
#endif
#ifndef DEFAULT_LOCAL_RULE_PUMP_DURATION_S
#define DEFAULT_LOCAL_RULE_PUMP_DURATION_S PROJECT_LOCAL_RULE_PUMP_DURATION_S
#endif
#ifndef DEFAULT_LOCAL_RULE_COOLDOWN_S
#define DEFAULT_LOCAL_RULE_COOLDOWN_S PROJECT_LOCAL_RULE_COOLDOWN_S
#endif

#ifndef DEFAULT_AI_MODEL_VERSION
#define DEFAULT_AI_MODEL_VERSION PROJECT_AI_MODEL_VERSION
#endif
#ifndef DEFAULT_AI_CONFIDENCE_THRESHOLD
#define DEFAULT_AI_CONFIDENCE_THRESHOLD PROJECT_AI_CONFIDENCE_THRESHOLD
#endif
#ifndef DEFAULT_AI_INFERENCE_INTERVAL_MS
#define DEFAULT_AI_INFERENCE_INTERVAL_MS PROJECT_AI_INFERENCE_INTERVAL_MS
#endif

#ifndef DEFAULT_WIFI_SSID
#define DEFAULT_WIFI_SSID PROJECT_WIFI_SSID_DEFAULT
#endif
#ifndef DEFAULT_WIFI_PASSWORD
#define DEFAULT_WIFI_PASSWORD PROJECT_WIFI_PASSWORD_DEFAULT
#endif
#ifndef DEFAULT_WIFI_MAX_RETRY
#define DEFAULT_WIFI_MAX_RETRY PROJECT_WIFI_MAX_RETRY
#endif
#ifndef DEFAULT_WIFI_INITIAL_BACKOFF_MS
#define DEFAULT_WIFI_INITIAL_BACKOFF_MS PROJECT_WIFI_INITIAL_BACKOFF_MS
#endif
#ifndef DEFAULT_WIFI_MAX_BACKOFF_MS
#define DEFAULT_WIFI_MAX_BACKOFF_MS PROJECT_WIFI_MAX_BACKOFF_MS
#endif
#ifndef DEFAULT_WIFI_CONNECT_TIMEOUT_MS
#define DEFAULT_WIFI_CONNECT_TIMEOUT_MS PROJECT_WIFI_CONNECT_TIMEOUT_MS
#endif

#ifndef DEFAULT_BACKEND_HOST
#define DEFAULT_BACKEND_HOST PROJECT_BACKEND_HOST_DEFAULT
#endif
#ifndef DEFAULT_BACKEND_PORT
#define DEFAULT_BACKEND_PORT PROJECT_BACKEND_PORT_DEFAULT
#endif
#ifndef DEFAULT_API_BASE_PATH
#define DEFAULT_API_BASE_PATH PROJECT_API_BASE_PATH_DEFAULT
#endif
#ifndef DEFAULT_REGISTRATION_TOKEN
#define DEFAULT_REGISTRATION_TOKEN PROJECT_REGISTRATION_TOKEN_DEFAULT
#endif
#ifndef DEFAULT_HTTP_TIMEOUT_MS
#define DEFAULT_HTTP_TIMEOUT_MS PROJECT_HTTP_TIMEOUT_MS
#endif
#ifndef DEFAULT_TELEMETRY_INTERVAL_MS
#define DEFAULT_TELEMETRY_INTERVAL_MS PROJECT_TELEMETRY_INTERVAL_MS
#endif
#ifndef DEFAULT_COMMAND_POLL_INTERVAL_MS
#define DEFAULT_COMMAND_POLL_INTERVAL_MS PROJECT_COMMAND_POLL_INTERVAL_MS
#endif
#ifndef DEFAULT_PAYLOAD_SIZE_LIMIT
#define DEFAULT_PAYLOAD_SIZE_LIMIT PROJECT_PAYLOAD_SIZE_LIMIT
#endif

#ifndef DEFAULT_OTA_ENABLED
#define DEFAULT_OTA_ENABLED PROJECT_OTA_ENABLED
#endif
#ifndef DEFAULT_OTA_CHECK_INTERVAL_MS
#define DEFAULT_OTA_CHECK_INTERVAL_MS PROJECT_OTA_CHECK_INTERVAL_MS
#endif
#ifndef DEFAULT_OTA_TIMEOUT_MS
#define DEFAULT_OTA_TIMEOUT_MS PROJECT_OTA_TIMEOUT_MS
#endif
#ifndef DEFAULT_SERIAL_BAUD_RATE
#define DEFAULT_SERIAL_BAUD_RATE PROJECT_SERIAL_BAUD_RATE
#endif
#ifndef DEFAULT_LOG_LEVEL
#define DEFAULT_LOG_LEVEL PROJECT_LOG_LEVEL
#endif

#ifdef __cplusplus
}
#endif

#endif /* PROJECT_CONFIG_H */
