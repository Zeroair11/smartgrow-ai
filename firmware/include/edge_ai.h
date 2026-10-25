/**
 * @file edge_ai.h
 * @brief Firmware Edge AI Engine, Normalization, Inference & Safety Interlock Wrap
 */

#ifndef EDGE_AI_H
#define EDGE_AI_H

#include <stdint.h>
#include <stdbool.h>
#include "sensor_manager.h"
#include "edge_ai_model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int         predicted_class;
    const char* class_label;
    float       confidence;
    bool        recommended_pump_action;
    bool        safety_override_applied;
    const char* final_action;
} EdgeAIResult;

/**
 * @brief Initialize Edge AI subsystem
 */
void edge_ai_init(void);

/**
 * @brief Run Edge AI inference and apply safety guardrails
 */
bool edge_ai_infer(const SensorReadings *readings, uint32_t current_time_ms, EdgeAIResult *out_result);

#ifdef __cplusplus
}
#endif

#endif /* EDGE_AI_H */
