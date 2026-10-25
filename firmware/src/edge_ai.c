#include "edge_ai.h"
#include "safety_manager.h"
#include <string.h>

void edge_ai_init(void) {
    // Edge AI uses deterministic zero-heap inference
}

bool edge_ai_infer(const SensorReadings *readings, uint32_t current_time_ms, EdgeAIResult *out_result) {
    if (!readings || !out_result) return false;

    memset(out_result, 0, sizeof(*out_result));

    /* 1. If sensor readings are marked invalid by sensor_manager, predict SENSOR_FAULT immediately */
    if (!readings->soil_valid || !readings->dht_valid || !readings->light_valid) {
        out_result->predicted_class = AI_CLASS_SENSOR_FAULT;
        out_result->class_label = edge_ai_model_class_to_str(AI_CLASS_SENSOR_FAULT);
        out_result->confidence = 1.0f;
        out_result->recommended_pump_action = false;
        out_result->safety_override_applied = false;
        out_result->final_action = "SENSOR_FAULT_HOLD";
        return true;
    }

    /* 2. Run deterministic decision tree */
    float confidence = 1.0f;
    int class_id = edge_ai_model_predict(
        readings->soil_moisture_pct,
        readings->temperature_c,
        readings->humidity_pct,
        readings->light_level_pct,
        &confidence
    );

    out_result->predicted_class = class_id;
    out_result->class_label = edge_ai_model_class_to_str(class_id);
    out_result->confidence = confidence;

    /* 3. Determine recommended action from AI class */
    if (class_id == AI_CLASS_NEED_WATER) {
        out_result->recommended_pump_action = true;

        /* 4. CRITICAL SAFETY GUARDRAIL: Check with safety manager */
        SafetyStatus safety_reason;
        if (!safety_manager_can_start_pump(readings, current_time_ms, &safety_reason)) {
            out_result->safety_override_applied = true;
            out_result->final_action = "SAFETY_OVERRIDE_BLOCK_PUMP";
        } else {
            out_result->safety_override_applied = false;
            out_result->final_action = "PUMP_START";
        }
    } else if (class_id == AI_CLASS_HIGH_TEMPERATURE) {
        out_result->recommended_pump_action = false;
        out_result->safety_override_applied = false;
        out_result->final_action = "OPEN_VENTILATION";
    } else {
        out_result->recommended_pump_action = false;
        out_result->safety_override_applied = false;
        out_result->final_action = "MONITOR";
    }

    return true;
}
