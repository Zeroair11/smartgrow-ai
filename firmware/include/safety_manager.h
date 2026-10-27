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
    SAFETY_ERR_EMERGENCY_STOP       = 6,
    SAFETY_ERR_LOCKOUT_ACTIVE       = 7
} SafetyStatus;

typedef enum {
    SAFETY_SRC_MANUAL         = 0,
    SAFETY_SRC_DOWNLINK_CMD   = 1,
    SAFETY_SRC_LOCAL_RULE     = 2,
    SAFETY_SRC_EDGE_AI        = 3,
    SAFETY_SRC_WATCHDOG       = 4
} SafetyRequestSource;

typedef struct {
    bool                pump_is_running;
    uint32_t            pump_started_at_ms;
    uint32_t            last_pump_stopped_at_ms;
    uint32_t            max_runtime_limit_ms;
    uint32_t            min_cooldown_limit_ms;
    uint32_t            target_duration_ms;       /* Planned run time for active cycle */
    SafetyRequestSource current_source;           /* Source of active pump command */
    bool                in_lockout;               /* Latched when emergency cutoff trips */
    uint32_t            lockout_until_ms;         /* Uptime ms when lockout expires */
    SafetyStatus        last_violation;
    uint32_t            total_safety_trips;       /* Emergency watchdog cutoffs */
    uint32_t            total_normal_stops;       /* Target duration completions */
} SafetyState;

/**
 * @brief Initialize safety manager state with hard safety defaults
 */
void safety_manager_init(uint32_t max_runtime_ms, uint32_t min_cooldown_ms);

/**
 * @brief Centralized Relay Safety Gate: Request pump ON with safety interlocks, duration planning & structured logging
 * @param source Origin of request (Manual, Downlink, Rule, Edge AI)
 * @param duration_ms Planned run duration in milliseconds (clamped to max_runtime_limit_ms)
 * @param readings Current sensor readings for validity and environmental boundary checking
 * @param current_time_ms Current system uptime in ms
 * @param out_reason Pointer to receive SafetyStatus if rejected
 * @return true if approved and relay turned ON, false if rejected
 */
bool safety_manager_request_pump_on(SafetyRequestSource source,
                                    uint32_t duration_ms,
                                    const SensorReadings *readings,
                                    uint32_t current_time_ms,
                                    SafetyStatus *out_reason);

/**
 * @brief Centralized Relay Safety Gate: Request pump OFF (always immediate, safe actuator cutoff)
 * @param source Origin of request
 * @param reason Reason string for audit log
 * @param current_time_ms Current system uptime in ms
 */
void safety_manager_request_pump_off(SafetyRequestSource source,
                                     const char *reason,
                                     uint32_t current_time_ms);

/**
 * @brief Explicitly clear watchdog lockout latch if conditions are met
 */
bool safety_manager_clear_lockout(uint32_t current_time_ms);

/**
 * @brief Explicitly clear watchdog lockout latch with sensor health verification
 */
bool safety_manager_clear_lockout_ex(uint32_t current_time_ms, const SensorReadings *readings);

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
