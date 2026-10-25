/**
 * @file safety_manager.h
 * @brief Hard Real-Time Safety Interlocks & Hardware Guardrails
 */

#ifndef SAFETY_MANAGER_H
#define SAFETY_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "sensor_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SAFETY_OK = 0,
    SAFETY_ERR_MAX_RUNTIME_EXCEEDED = 1,
    SAFETY_ERR_COOLDOWN_ACTIVE      = 2,
    SAFETY_ERR_SENSOR_FAULT         = 3,
    SAFETY_ERR_TEMP_OUT_OF_BOUNDS   = 4,
    SAFETY_ERR_INVALID_COMMAND      = 5,
    SAFETY_ERR_EMERGENCY_STOP       = 6
} SafetyStatus;

typedef struct {
    bool         pump_is_running;
    uint32_t     pump_started_at_ms;
    uint32_t     last_pump_stopped_at_ms;
    uint32_t     max_runtime_limit_ms;
    uint32_t     min_cooldown_limit_ms;
    SafetyStatus last_violation;
    uint32_t     total_safety_trips;
} SafetyState;

/**
 * @brief Initialize safety manager state with hard safety defaults
 */
void safety_manager_init(uint32_t max_runtime_ms, uint32_t min_cooldown_ms);

/**
 * @brief Check if pump can safely be turned ON given current time and sensor readings
 */
bool safety_manager_can_start_pump(const SensorReadings *readings, uint32_t current_time_ms, SafetyStatus *out_reason);

/**
 * @brief Periodic safety tick - monitors running pump and auto-cuts off if duration exceeded
 * @return true if pump was forcibly stopped due to safety violation
 */
bool safety_manager_tick(uint32_t current_time_ms);

/**
 * @brief Record pump start event in safety state
 */
void safety_manager_record_pump_start(uint32_t current_time_ms);

/**
 * @brief Record pump stop event in safety state
 */
void safety_manager_record_pump_stop(uint32_t current_time_ms);

/**
 * @brief Get current safety state snapshot
 */
const SafetyState* safety_manager_get_state(void);

#ifdef __cplusplus
}
#endif

#endif /* SAFETY_MANAGER_H */
