#include "safety_manager.h"
#include "board_config.h"

static SafetyState s_state;

void safety_manager_init(uint32_t max_runtime_ms, uint32_t min_cooldown_ms) {
    s_state.pump_is_running         = false;
    s_state.pump_started_at_ms      = 0;
    s_state.last_pump_stopped_at_ms = 0;
    s_state.max_runtime_limit_ms    = (max_runtime_ms > 0) ? max_runtime_ms : SAFETY_MAX_PUMP_DURATION_MS;
    s_state.min_cooldown_limit_ms   = (min_cooldown_ms > 0) ? min_cooldown_ms : SAFETY_MIN_PUMP_COOLDOWN_MS;
    s_state.last_violation          = SAFETY_OK;
    s_state.total_safety_trips      = 0;
}

bool safety_manager_can_start_pump(const SensorReadings *readings, uint32_t current_time_ms, SafetyStatus *out_reason) {
    if (s_state.pump_is_running) {
        if (out_reason) *out_reason = SAFETY_OK;
        return true;
    }

    /* Check mandatory cooldown */
    if (s_state.last_pump_stopped_at_ms > 0) {
        uint32_t elapsed_since_stop = current_time_ms - s_state.last_pump_stopped_at_ms;
        if (elapsed_since_stop < s_state.min_cooldown_limit_ms) {
            s_state.last_violation = SAFETY_ERR_COOLDOWN_ACTIVE;
            if (out_reason) *out_reason = SAFETY_ERR_COOLDOWN_ACTIVE;
            return false;
        }
    }

    /* Check mandatory sensor validity if readings provided */
    if (readings) {
        if (!readings->soil_valid || !readings->dht_valid) {
            s_state.last_violation = SAFETY_ERR_SENSOR_FAULT;
            if (out_reason) *out_reason = SAFETY_ERR_SENSOR_FAULT;
            return false;
        }

        /* Check extreme environmental temperatures */
        if (readings->temperature_c < SENSOR_TEMP_MIN_VALID_C || readings->temperature_c > SENSOR_TEMP_MAX_VALID_C) {
            s_state.last_violation = SAFETY_ERR_TEMP_OUT_OF_BOUNDS;
            if (out_reason) *out_reason = SAFETY_ERR_TEMP_OUT_OF_BOUNDS;
            return false;
        }
    }

    if (out_reason) *out_reason = SAFETY_OK;
    return true;
}

bool safety_manager_tick(uint32_t current_time_ms) {
    if (!s_state.pump_is_running) {
        return false;
    }

    uint32_t runtime = current_time_ms - s_state.pump_started_at_ms;
    if (runtime >= s_state.max_runtime_limit_ms) {
        /* Force cutoff */
        s_state.pump_is_running = false;
        s_state.last_pump_stopped_at_ms = current_time_ms;
        s_state.last_violation = SAFETY_ERR_MAX_RUNTIME_EXCEEDED;
        s_state.total_safety_trips++;
        return true;
    }

    return false;
}

void safety_manager_record_pump_start(uint32_t current_time_ms) {
    s_state.pump_is_running = true;
    s_state.pump_started_at_ms = current_time_ms;
}

void safety_manager_record_pump_stop(uint32_t current_time_ms) {
    s_state.pump_is_running = false;
    s_state.last_pump_stopped_at_ms = current_time_ms;
}

const SafetyState* safety_manager_get_state(void) {
    return &s_state;
}
