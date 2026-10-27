/**
 * @file dht11.h
 * @brief High-Reliability DHT11 Temperature & Humidity Sensor Driver
 *
 * Implements microsecond-accurate single-wire bus protocol with:
 * - Proper host start pulse (> 18ms)
 * - 40-bit transmission decoding
 * - Byte 4 checksum validation
 * - Hardware rate limiting (minimum 2000ms interval)
 * - Range plausibility bounds (0-50°C, 20-90% RH)
 * - Fault detection & consecutive error counter
 * - Native desktop mock injection for unit and HIL simulation
 */

#ifndef DHT11_H
#define DHT11_H

#include <stdint.h>
#include <stdbool.h>
#include "firmware_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DHT11_OK = 0,
    DHT11_ERR_TIMEOUT = 1,
    DHT11_ERR_CHECKSUM = 2,
    DHT11_ERR_OUT_OF_RANGE = 3,
    DHT11_ERR_TOO_FAST = 4,
    DHT11_ERR_DISCONNECTED = 5,
    DHT11_ERR_INVALID_PARAM = 6
} Dht11ErrorCode;

typedef struct {
    float temperature_c;
    float humidity_pct;
    bool  is_valid;
    Dht11ErrorCode error_code;
    uint32_t timestamp_ms;
} Dht11Reading;

/**
 * @brief Initialize the DHT11 single-wire driver with configuration
 * @param config Pointer to Dht11Config (or NULL to load global firmware_config)
 */
void dht11_init(const Dht11Config *config);

/**
 * @brief Execute a single-wire measurement cycle with rate limiting and checksum validation
 * @param out_temp Output pointer for temperature in Celsius
 * @param out_hum Output pointer for relative humidity in percent
 * @param now_ms Current system timestamp in milliseconds
 * @return true if fresh plausible measurement was obtained, false on any fault
 */
bool dht11_read(float *out_temp, float *out_hum, uint32_t now_ms);

/**
 * @brief Get the last recorded DHT11 reading (valid or invalid)
 */
const Dht11Reading* dht11_get_last_reading(void);

/**
 * @brief Check if the DHT11 sensor is healthy (consecutive errors below threshold)
 */
bool dht11_is_healthy(void);

/**
 * @brief Get the current consecutive error count
 */
uint32_t dht11_get_consecutive_errors(void);

/**
 * @brief Reset error counter manually
 */
void dht11_reset_faults(void);

#if !defined(ESP_PLATFORM)
/**
 * @brief Desktop test helper: Inject mock temperature and humidity values
 */
void dht11_mock_inject(float temp, float hum, bool valid);

/**
 * @brief Desktop test helper: Force a specific error code on next read
 */
void dht11_mock_set_fault(Dht11ErrorCode fault);
#endif

#ifdef __cplusplus
}
#endif

#endif /* DHT11_H */
