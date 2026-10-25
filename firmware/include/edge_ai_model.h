/**
 * @file edge_ai_model.h
 * @brief Auto-generated TinyML Decision Tree Inference Engine for ESP32-S3
 * Model Version: v1.0.0-dt
 * Architecture: Zero-heap deterministic binary decision tree
 */

#ifndef EDGE_AI_MODEL_H
#define EDGE_AI_MODEL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EDGE_AI_MODEL_VERSION      "v1.0.0-dt"
#define EDGE_AI_NUM_CLASSES        5
#define EDGE_AI_NUM_FEATURES       4

typedef enum {
    AI_CLASS_NORMAL           = 0,
    AI_CLASS_NEED_WATER       = 1,
    AI_CLASS_HIGH_TEMPERATURE = 2,
    AI_CLASS_LOW_LIGHT        = 3,
    AI_CLASS_SENSOR_FAULT     = 4
} EdgeAIClass;

/**
 * @brief Perform deterministic decision tree inference on sensor features
 * @param soil_moisture_pct Soil moisture in percent (0.0 - 100.0)
 * @param temperature_c Temperature in Celsius
 * @param humidity_pct Air humidity in percent (0.0 - 100.0)
 * @param light_level_pct Ambient light in percent (0.0 - 100.0)
 * @param out_confidence Optional pointer to receive estimated prediction confidence (0.0 - 1.0)
 * @return Predicted EdgeAIClass enum
 */
int edge_ai_model_predict(float soil_moisture_pct, float temperature_c, float humidity_pct, float light_level_pct, float *out_confidence);

/**
 * @brief Get human-readable label for a given class ID
 */
const char* edge_ai_model_class_to_str(int class_id);

#ifdef __cplusplus
}
#endif

#endif /* EDGE_AI_MODEL_H */
