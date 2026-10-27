#include "safety_manager.h"
#include "board_config.h"
#include "actuator_manager.h"
#include "firmware_config.h"

#if defined(ESP_PLATFORM)
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
static const char *TAG = "safety_mgr";
static SemaphoreHandle_t s_safety_mutex = NULL;
#define SAFETY_LOCK()   do { if (s_safety_mutex) xSemaphoreTakeRecursive(s_safety_mutex, portMAX_DELAY); } while(0)
#define SAFETY_UNLOCK() do { if (s_safety_mutex) xSemaphoreGiveRecursive(s_safety_mutex); } while(0)
#else
#include <stdio.h>
#define ESP_LOGI(tag, fmt, ...) printf("[%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) printf("[%s][WARN] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGE(tag, fmt, ...) printf("[%s][ERR] " fmt "\n", tag, ##__VA_ARGS__)
static const char *TAG = "safety_mgr";
#define SAFETY_LOCK()   do {} while(0)
#define SAFETY_UNLOCK() do {} while(0)
#endif

static SafetyState s_state;

static const char* safety_source_to_str(SafetyRequestSource src) {
    switch (src) {
        case SAFETY_SRC_MANUAL:       return "MANUAL";
        case SAFETY_SRC_DOWNLINK_CMD: return "DOWNLINK_CMD";
        case SAFETY_SRC_LOCAL_RULE:   return "LOCAL_RULE";
        case SAFETY_SRC_EDGE_AI:      return "EDGE_AI";
        case SAFETY_SRC_WATCHDOG:     return "WATCHDOG";
        default:                      return "UNKNOWN";
    }
}

static const char* safety_status_to_str(SafetyStatus st) {
    switch (st) {
        case SAFETY_OK:                       return "SAFETY_OK";
        case SAFETY_ERR_MAX_RUNTIME_EXCEEDED: return "MAX_RUNTIME_EXCEEDED";
        case SAFETY_ERR_COOLDOWN_ACTIVE:      return "COOLDOWN_ACTIVE";
        case SAFETY_ERR_SENSOR_FAULT:         return "SENSOR_FAULT";
        case SAFETY_ERR_TEMP_OUT_OF_BOUNDS:   return "TEMP_OUT_OF_BOUNDS";
        case SAFETY_ERR_INVALID_COMMAND:      return "INVALID_COMMAND";
        case SAFETY_ERR_EMERGENCY_STOP:       return "EMERGENCY_STOP";
        case SAFETY_ERR_LOCKOUT_ACTIVE:       return "LOCKOUT_ACTIVE";
        default:                              return "UNKNOWN_STATUS";
    }
}

void safety_manager_init(uint32_t max_runtime_ms, uint32_t min_cooldown_ms) {
#if defined(ESP_PLATFORM)
    if (!s_safety_mutex) {
        s_safety_mutex = xSemaphoreCreateRecursiveMutex();
    }
#endif
    SAFETY_LOCK();
    const FirmwareConfig *cfg = firmware_config_get();
    uint32_t default_runtime = cfg ? cfg->safety.max_pump_duration_ms : SAFETY_MAX_PUMP_DURATION_MS;
    uint32_t default_cooldown = cfg ? cfg->safety.min_pump_cooldown_ms : SAFETY_MIN_PUMP_COOLDOWN_MS;

    s_state.pump_is_running         = false;
    s_state.pump_started_at_ms      = 0;
    s_state.last_pump_stopped_at_ms = 0;
    s_state.max_runtime_limit_ms    = (max_runtime_ms > 0) ? max_runtime_ms : default_runtime;
    s_state.min_cooldown_limit_ms   = (min_cooldown_ms > 0) ? min_cooldown_ms : default_cooldown;
    s_state.target_duration_ms      = 0;
    s_state.current_source          = SAFETY_SRC_MANUAL;
    s_state.in_lockout              = false;
    s_state.lockout_until_ms        = 0;
    s_state.last_violation          = SAFETY_OK;
    s_state.total_safety_trips      = 0;
    s_state.total_normal_stops      = 0;
    SAFETY_UNLOCK();
}

bool safety_manager_clear_lockout_ex(uint32_t current_time_ms, const SensorReadings *readings) {
    SAFETY_LOCK();
    if (!s_state.in_lockout) {
        SAFETY_UNLOCK();
        return true;
    }

    /* 1. Cooldown condition: evaluated independently */
    if (s_state.last_pump_stopped_at_ms > 0) {
        uint32_t elapsed = current_time_ms - s_state.last_pump_stopped_at_ms;
        if (elapsed < s_state.min_cooldown_limit_ms) {
            ESP_LOGW(TAG, "[SAFETY_GATE] Cannot clear lockout: cooldown still active (%u ms remaining)",
                     (unsigned)(s_state.min_cooldown_limit_ms - elapsed));
            SAFETY_UNLOCK();
            return false;
        }
    }

    /* 2. Sensor health condition: evaluated independently.
     * Cannot clear lockout if current readings indicate sensor fault, or if lockout was
     * caused by a sensor fault and no valid readings are provided to prove recovery. */
    if (readings) {
        if (!readings->soil_valid || !readings->dht_valid) {
            ESP_LOGW(TAG, "[SAFETY_GATE] Cannot clear lockout: sensor fault active (soil=%d, dht=%d)",
                     (int)readings->soil_valid, (int)readings->dht_valid);
            SAFETY_UNLOCK();
            return false;
        }
    } else if (s_state.last_violation == SAFETY_ERR_SENSOR_FAULT ||
               s_state.last_violation == SAFETY_ERR_TEMP_OUT_OF_BOUNDS) {
        ESP_LOGW(TAG, "[SAFETY_GATE] Cannot clear sensor-induced lockout without valid sensor recovery readings");
        SAFETY_UNLOCK();
        return false;
    }

    s_state.in_lockout = false;
    s_state.lockout_until_ms = 0;
    s_state.last_violation = SAFETY_OK;
    ESP_LOGI(TAG, "[SAFETY_GATE] Lockout latch cleared. System ready for normal operation.");
    SAFETY_UNLOCK();
    return true;
}

bool safety_manager_clear_lockout(uint32_t current_time_ms) {
    return safety_manager_clear_lockout_ex(current_time_ms, NULL);
}

bool safety_manager_can_start_pump(const SensorReadings *readings, uint32_t current_time_ms, SafetyStatus *out_reason) {
    SAFETY_LOCK();
    if (s_state.pump_is_running) {
        if (out_reason) *out_reason = SAFETY_OK;
        SAFETY_UNLOCK();
        return true;
    }

    /* 0. Enforce SAFE control mode (failsafe inhibit) */
    const FirmwareConfig *cfg = firmware_config_get();
    if (cfg && cfg->control.mode == CONTROL_MODE_SAFE) {
        s_state.last_violation = SAFETY_ERR_EMERGENCY_STOP;
        if (out_reason) *out_reason = SAFETY_ERR_EMERGENCY_STOP;
        SAFETY_UNLOCK();
        return false;
    }

    /* 1. Check mandatory pump cooldown timer (uint32_t rollover safe) */
    if (s_state.last_pump_stopped_at_ms > 0) {
        uint32_t elapsed_since_stop = current_time_ms - s_state.last_pump_stopped_at_ms;
        if (elapsed_since_stop < s_state.min_cooldown_limit_ms) {
            s_state.last_violation = SAFETY_ERR_COOLDOWN_ACTIVE;
            if (out_reason) *out_reason = SAFETY_ERR_COOLDOWN_ACTIVE;
            SAFETY_UNLOCK();
            return false;
        }
    }

    /* 2. Check if latched in emergency lockout (requires explicit clear_lockout) */
    if (s_state.in_lockout) {
        s_state.last_violation = SAFETY_ERR_LOCKOUT_ACTIVE;
        if (out_reason) *out_reason = SAFETY_ERR_LOCKOUT_ACTIVE;
        SAFETY_UNLOCK();
        return false;
    }

    /* 3. Check mandatory sensor validity if readings provided */
    if (readings) {
        if (!readings->soil_valid || !readings->dht_valid) {
            s_state.last_violation = SAFETY_ERR_SENSOR_FAULT;
            if (out_reason) *out_reason = SAFETY_ERR_SENSOR_FAULT;
            SAFETY_UNLOCK();
            return false;
        }

        /* 4. Check extreme environmental temperatures against DHT11 safety limits */
        float min_temp = cfg ? cfg->safety.min_safe_temp_c : SENSOR_TEMP_MIN_VALID_C;
        float max_temp = cfg ? cfg->safety.max_safe_temp_c : SENSOR_TEMP_MAX_VALID_C;

        if (readings->temperature_c < min_temp || readings->temperature_c > max_temp) {
            s_state.last_violation = SAFETY_ERR_TEMP_OUT_OF_BOUNDS;
            if (out_reason) *out_reason = SAFETY_ERR_TEMP_OUT_OF_BOUNDS;
            SAFETY_UNLOCK();
            return false;
        }
    }

    if (out_reason) *out_reason = SAFETY_OK;
    SAFETY_UNLOCK();
    return true;
}

bool safety_manager_request_pump_on(SafetyRequestSource source,
                                    uint32_t duration_ms,
                                    const SensorReadings *readings,
                                    uint32_t current_time_ms,
                                    SafetyStatus *out_reason) {
    SAFETY_LOCK();
    /* 1. If pump is already running, avoid restarting or interrupting active cycle */
    if (s_state.pump_is_running) {
        if (out_reason) *out_reason = SAFETY_OK;
        SAFETY_UNLOCK();
        return true;
    }

    /* 2. Check all interlocks and guardrails */
    SafetyStatus reason = SAFETY_OK;
    if (!safety_manager_can_start_pump(readings, current_time_ms, &reason)) {
        if (out_reason) *out_reason = reason;
        ESP_LOGW(TAG, "[SAFETY_GATE] REQUEST=PUMP_ON | SOURCE=%s | DECISION=REJECTED | REJECTION_REASON=%s | ACTUAL_OUTPUT_STATE=OFF",
                 safety_source_to_str(source), safety_status_to_str(reason));
        SAFETY_UNLOCK();
        return false;
    }

    /* 3. Clamp target duration to hard safety limit */
    if (duration_ms == 0) {
        duration_ms = 30000; /* Default 30 seconds */
    }
    if (duration_ms > s_state.max_runtime_limit_ms) {
        ESP_LOGW(TAG, "[SAFETY_GATE] Requested duration %u ms exceeds limit %u ms; clamping to limit",
                 (unsigned)duration_ms, (unsigned)s_state.max_runtime_limit_ms);
        duration_ms = s_state.max_runtime_limit_ms;
    }

    /* 4. Commit pump state and engage physical relay via centralized gate */
    s_state.pump_is_running    = true;
    s_state.pump_started_at_ms = current_time_ms;
    s_state.target_duration_ms = duration_ms;
    s_state.current_source     = source;
    s_state.last_violation     = SAFETY_OK;

    actuator_manager_set_pump(true);

    ESP_LOGI(TAG, "[SAFETY_GATE] REQUEST=PUMP_ON | SOURCE=%s | TARGET_DURATION=%ums | DECISION=APPROVED | REJECTION_REASON=NONE | ACTUAL_OUTPUT_STATE=ON",
             safety_source_to_str(source), (unsigned)duration_ms);

    if (out_reason) *out_reason = SAFETY_OK;
    SAFETY_UNLOCK();
    return true;
}

void safety_manager_request_pump_off(SafetyRequestSource source,
                                     const char *reason,
                                     uint32_t current_time_ms) {
    SAFETY_LOCK();
    bool was_running = s_state.pump_is_running;

    s_state.pump_is_running = false;
    actuator_manager_set_pump(false);

    if (was_running) {
        s_state.last_pump_stopped_at_ms = current_time_ms;
        s_state.target_duration_ms      = 0;
        s_state.total_normal_stops++;
        ESP_LOGI(TAG, "[SAFETY_GATE] REQUEST=PUMP_OFF | SOURCE=%s | REASON=%s | DECISION=STOPPED | ACTUAL_OUTPUT_STATE=OFF",
                 safety_source_to_str(source), reason ? reason : "UNSPECIFIED");
    } else {
        ESP_LOGI(TAG, "[SAFETY_GATE] REQUEST=PUMP_OFF | SOURCE=%s | REASON=%s | DECISION=ALREADY_OFF | ACTUAL_OUTPUT_STATE=OFF",
                 safety_source_to_str(source), reason ? reason : "UNSPECIFIED");
    }
    SAFETY_UNLOCK();
}

bool safety_manager_tick(uint32_t current_time_ms) {
    SAFETY_LOCK();
    if (!s_state.pump_is_running) {
        SAFETY_UNLOCK();
        return false;
    }

    uint32_t runtime = current_time_ms - s_state.pump_started_at_ms;

    /* Check 1: Hard real-time safety watchdog cutoff (max runtime exceeded) */
    if (runtime >= s_state.max_runtime_limit_ms) {
        s_state.pump_is_running         = false;
        s_state.last_pump_stopped_at_ms = current_time_ms;
        s_state.last_violation          = SAFETY_ERR_MAX_RUNTIME_EXCEEDED;
        s_state.total_safety_trips++;
        s_state.in_lockout              = true;
        s_state.lockout_until_ms        = current_time_ms + s_state.min_cooldown_limit_ms;
        s_state.target_duration_ms      = 0;

        actuator_manager_set_pump(false);

        ESP_LOGW(TAG, "[SAFETY_GATE] EMERGENCY_WATCHDOG_CUTOFF! RUNTIME=%ums | REASON=MAX_RUNTIME_EXCEEDED | LOCKOUT=ENGAGED | ACTUAL_OUTPUT_STATE=OFF",
                 (unsigned)runtime);
        SAFETY_UNLOCK();
        return true;
    }

    /* Check 2: Planned target duration completed normally (when < max_runtime_limit_ms) */
    if (s_state.target_duration_ms > 0 && runtime >= s_state.target_duration_ms) {
        s_state.pump_is_running         = false;
        s_state.last_pump_stopped_at_ms = current_time_ms;
        s_state.target_duration_ms      = 0;
        s_state.total_normal_stops++;

        actuator_manager_set_pump(false);

        ESP_LOGI(TAG, "[SAFETY_GATE] REQUEST=PLANNED_STOP | RUNTIME=%ums | DECISION=CYCLE_COMPLETE | REJECTION_REASON=NONE | ACTUAL_OUTPUT_STATE=OFF",
                 (unsigned)runtime);
        SAFETY_UNLOCK();
        return false;
    }

    SAFETY_UNLOCK();
    return false;
}

void safety_manager_record_pump_start(uint32_t current_time_ms) {
    SAFETY_LOCK();
    s_state.pump_is_running    = true;
    s_state.pump_started_at_ms = current_time_ms;
    if (s_state.target_duration_ms == 0) {
        s_state.target_duration_ms = s_state.max_runtime_limit_ms;
    }
    SAFETY_UNLOCK();
}

void safety_manager_record_pump_stop(uint32_t current_time_ms) {
    SAFETY_LOCK();
    s_state.pump_is_running         = false;
    s_state.last_pump_stopped_at_ms = current_time_ms;
    s_state.target_duration_ms      = 0;
    SAFETY_UNLOCK();
}

const SafetyState* safety_manager_get_state(void) {
    return &s_state;
}

