/**
 * @file dht11.c
 * @brief High-Reliability DHT11 Temperature & Humidity Sensor Driver Implementation
 */

#include "dht11.h"
#include <string.h>
#include <stdio.h>

#if defined(ESP_PLATFORM)
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "dht11";
#endif

static Dht11Config s_config;
static Dht11Reading s_last_reading = {
    .temperature_c = 25.0f,
    .humidity_pct = 60.0f,
    .is_valid = true,
    .error_code = DHT11_OK,
    .timestamp_ms = 0
};
static uint32_t s_last_read_ms = 0;
static uint32_t s_consecutive_errors = 0;
static bool s_is_initialized = false;

#if !defined(ESP_PLATFORM)
static bool s_mock_injected = false;
static float s_mock_temp = 25.0f;
static float s_mock_hum = 60.0f;
static bool s_mock_valid = true;
static Dht11ErrorCode s_forced_fault = DHT11_OK;
#endif

void dht11_init(const Dht11Config *config) {
    if (config) {
        s_config = *config;
    } else {
        const FirmwareConfig *fw_cfg = firmware_config_get();
        s_config = fw_cfg->dht11;
    }

    s_last_read_ms = 0;
    s_consecutive_errors = 0;
    s_last_reading.temperature_c = 0.0f;
    s_last_reading.humidity_pct = 0.0f;
    s_last_reading.is_valid = false;
    s_last_reading.error_code = DHT11_OK;
    s_last_reading.timestamp_ms = 0;

#if defined(ESP_PLATFORM)
    gpio_num_t pin = (gpio_num_t)s_config.pin;
    gpio_reset_pin(pin);
    /* Open-drain with pull-up avoids bus contention when DHT11 responds */
    gpio_set_direction(pin, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_pull_mode(pin, GPIO_PULLUP_ONLY);
    gpio_set_level(pin, 1);
    ESP_LOGI(TAG, "DHT11 initialized on GPIO %u (Open-Drain, Interval: %u ms, Range: %.0f-%.0f C, %.0f-%.0f %%)",
             s_config.pin, (unsigned)s_config.sample_interval_ms,
             s_config.temp_min_c, s_config.temp_max_c,
             s_config.humidity_min_pct, s_config.humidity_max_pct);
#endif

    s_is_initialized = true;
}

#if defined(ESP_PLATFORM)
static int wait_for_level(gpio_num_t pin, int level, uint32_t timeout_us) {
    uint32_t count_us = 0;
    while (gpio_get_level(pin) != level) {
        if (count_us >= timeout_us) {
            return -1;
        }
        esp_rom_delay_us(1);
        count_us++;
    }
    return (int)count_us;
}
#endif

bool dht11_read(float *out_temp, float *out_hum, uint32_t now_ms) {
    if (!s_is_initialized) {
        dht11_init(NULL);
    }

    /* 1. Hardware Rate-Limiting: Do not read faster than sample_interval_ms (2000ms) */
    if (s_last_read_ms > 0 && (now_ms - s_last_read_ms) < s_config.sample_interval_ms) {
        s_last_reading.error_code = DHT11_ERR_TOO_FAST;
        /* If we have a previously valid reading, we can keep the value but flag rate-limit */
        if (out_temp) *out_temp = s_last_reading.temperature_c;
        if (out_hum) *out_hum = s_last_reading.humidity_pct;
        return false;
    }

#if defined(ESP_PLATFORM)
    gpio_num_t pin = (gpio_num_t)s_config.pin;
    uint8_t data[5] = {0};

    /* 2. Send Start Signal: Pull line LOW for 20ms (> 18ms) */
    gpio_set_level(pin, 0);
    esp_rom_delay_us(20000); /* 20 ms start pulse */

    /* Critical section covers release, sensor handshake, and 40-bit readout
     * to eliminate timing jitter from FreeRTOS task switches or Wi-Fi ISR */
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    portENTER_CRITICAL(&mux);

    /* Release line (pulled high by pull-up resistor) */
    gpio_set_level(pin, 1);
    esp_rom_delay_us(30);

    /* 3. Wait for DHT11 Response: 80us LOW followed by 80us HIGH */
    if (wait_for_level(pin, 0, 100) < 0) {
        portEXIT_CRITICAL(&mux);
        s_consecutive_errors++;
        s_last_reading.is_valid = false;
        s_last_reading.error_code = DHT11_ERR_DISCONNECTED;
        s_last_reading.timestamp_ms = now_ms;
        if (s_consecutive_errors == 1 || s_consecutive_errors % 5 == 0) {
            ESP_LOGW(TAG, "DHT11 not responding on GPIO %u (timeout waiting for initial LOW pulse; check wiring/pull-up) [faults: %lu]",
                     s_config.pin, (unsigned long)s_consecutive_errors);
        }
        return false;
    }

    if (wait_for_level(pin, 1, 100) < 0) {
        portEXIT_CRITICAL(&mux);
        s_consecutive_errors++;
        s_last_reading.is_valid = false;
        s_last_reading.error_code = DHT11_ERR_TIMEOUT;
        s_last_reading.timestamp_ms = now_ms;
        if (s_consecutive_errors == 1 || s_consecutive_errors % 5 == 0) {
            ESP_LOGW(TAG, "DHT11 timeout waiting for response HIGH pulse [faults: %lu]",
                     (unsigned long)s_consecutive_errors);
        }
        return false;
    }

    if (wait_for_level(pin, 0, 100) < 0) {
        portEXIT_CRITICAL(&mux);
        s_consecutive_errors++;
        s_last_reading.is_valid = false;
        s_last_reading.error_code = DHT11_ERR_TIMEOUT;
        s_last_reading.timestamp_ms = now_ms;
        if (s_consecutive_errors == 1 || s_consecutive_errors % 5 == 0) {
            ESP_LOGW(TAG, "DHT11 timeout waiting for transmission start pulse [faults: %lu]",
                     (unsigned long)s_consecutive_errors);
        }
        return false;
    }

    /* 4. Read 40 bits (5 bytes) */
    int failed_bit = -1;
    for (int i = 0; i < 40; i++) {
        /* Wait for bit start (LOW pulse ~50us) */
        if (wait_for_level(pin, 1, 80) < 0) {
            failed_bit = i;
            break;
        }

        /* Measure HIGH pulse duration: ~26-28us = '0', ~70us = '1' */
        int high_duration = wait_for_level(pin, 0, 100);
        if (high_duration < 0) {
            failed_bit = i;
            break;
        }

        if (high_duration > 40) {
            data[i / 8] |= (1 << (7 - (i % 8)));
        }
    }
    portEXIT_CRITICAL(&mux);

    if (failed_bit >= 0) {
        s_consecutive_errors++;
        s_last_reading.is_valid = false;
        s_last_reading.error_code = DHT11_ERR_TIMEOUT;
        s_last_reading.timestamp_ms = now_ms;
        if (s_consecutive_errors == 1 || s_consecutive_errors % 5 == 0) {
            ESP_LOGW(TAG, "DHT11 transmission timeout at bit %d/40 [faults: %lu]",
                     failed_bit, (unsigned long)s_consecutive_errors);
        }
        return false;
    }

    /* 5. Checksum verification */
    uint8_t checksum = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
    if (checksum != data[4]) {
        s_consecutive_errors++;
        s_last_reading.is_valid = false;
        s_last_reading.error_code = DHT11_ERR_CHECKSUM;
        s_last_reading.timestamp_ms = now_ms;
        if (s_consecutive_errors == 1 || s_consecutive_errors % 5 == 0) {
            ESP_LOGW(TAG, "DHT11 checksum failure: computed %u != received %u [raw: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x] [faults: %lu]",
                     checksum, data[4], data[0], data[1], data[2], data[3], data[4],
                     (unsigned long)s_consecutive_errors);
        }
        return false;
    }

    /* 6. Decode values */
    float hum = (float)data[0] + ((float)data[1] * 0.1f);
    float temp = (float)data[2] + ((float)data[3] * 0.1f);
    if (data[3] & 0x80) {
        temp = -temp;
    }

    /* 7. Range plausibility check */
    if (temp < s_config.temp_min_c || temp > s_config.temp_max_c ||
        hum < s_config.humidity_min_pct || hum > s_config.humidity_max_pct) {
        s_consecutive_errors++;
        s_last_reading.is_valid = false;
        s_last_reading.error_code = DHT11_ERR_OUT_OF_RANGE;
        s_last_reading.temperature_c = temp;
        s_last_reading.humidity_pct = hum;
        s_last_reading.timestamp_ms = now_ms;
        if (s_consecutive_errors == 1 || s_consecutive_errors % 5 == 0) {
            ESP_LOGW(TAG, "DHT11 readings out of physical plausibility: Temp=%.1fC (limits: %.0f-%.0f), Hum=%.1f%% (limits: %.0f-%.0f) [faults: %lu]",
                     temp, s_config.temp_min_c, s_config.temp_max_c,
                     hum, s_config.humidity_min_pct, s_config.humidity_max_pct,
                     (unsigned long)s_consecutive_errors);
        }
        return false;
    }

    /* Success */
    s_last_read_ms = now_ms;
    s_consecutive_errors = 0;
    s_last_reading.temperature_c = temp;
    s_last_reading.humidity_pct = hum;
    s_last_reading.is_valid = true;
    s_last_reading.error_code = DHT11_OK;
    s_last_reading.timestamp_ms = now_ms;

    if (out_temp) *out_temp = temp;
    if (out_hum) *out_hum = hum;
    return true;

#else
    /* Native Desktop / Simulation logic */
    if (s_forced_fault != DHT11_OK) {
        Dht11ErrorCode code = s_forced_fault;
        s_forced_fault = DHT11_OK; /* Reset one-shot */
        s_consecutive_errors++;
        s_last_reading.is_valid = false;
        s_last_reading.error_code = code;
        s_last_reading.timestamp_ms = now_ms;
        return false;
    }

    float temp = s_mock_injected ? s_mock_temp : 25.0f;
    float hum = s_mock_injected ? s_mock_hum : 60.0f;
    bool valid = s_mock_injected ? s_mock_valid : true;

    /* Check plausibility bounds */
    if (!valid || temp < s_config.temp_min_c || temp > s_config.temp_max_c ||
        hum < s_config.humidity_min_pct || hum > s_config.humidity_max_pct) {
        s_consecutive_errors++;
        s_last_reading.is_valid = false;
        s_last_reading.error_code = DHT11_ERR_OUT_OF_RANGE;
        s_last_reading.temperature_c = temp;
        s_last_reading.humidity_pct = hum;
        s_last_reading.timestamp_ms = now_ms;
        return false;
    }

    s_last_read_ms = now_ms;
    s_consecutive_errors = 0;
    s_last_reading.temperature_c = temp;
    s_last_reading.humidity_pct = hum;
    s_last_reading.is_valid = true;
    s_last_reading.error_code = DHT11_OK;
    s_last_reading.timestamp_ms = now_ms;

    if (out_temp) *out_temp = temp;
    if (out_hum) *out_hum = hum;
    return true;
#endif
}

const Dht11Reading* dht11_get_last_reading(void) {
    return &s_last_reading;
}

bool dht11_is_healthy(void) {
    return (s_consecutive_errors < s_config.max_consecutive_errors);
}

uint32_t dht11_get_consecutive_errors(void) {
    return s_consecutive_errors;
}

void dht11_reset_faults(void) {
    s_consecutive_errors = 0;
}

#if !defined(ESP_PLATFORM)
void dht11_mock_inject(float temp, float hum, bool valid) {
    s_mock_injected = true;
    s_mock_temp = temp;
    s_mock_hum = hum;
    s_mock_valid = valid;
    s_forced_fault = DHT11_OK;
}

void dht11_mock_set_fault(Dht11ErrorCode fault) {
    s_forced_fault = fault;
}
#endif
