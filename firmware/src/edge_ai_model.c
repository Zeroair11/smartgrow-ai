#include "edge_ai_model.h"
#include <stddef.h>

static const char* s_class_labels[] = {
    "NORMAL",
    "NEED_WATER",
    "HIGH_TEMPERATURE",
    "LOW_LIGHT",
    "SENSOR_FAULT",
};

const char* edge_ai_model_class_to_str(int class_id) {
    if (class_id >= 0 && class_id < 5) {
        return s_class_labels[class_id];
    }
    return "UNKNOWN";
}

int edge_ai_model_predict(float soil_moisture_pct, float temperature_c, float humidity_pct, float light_level_pct, float *out_confidence) {
    if (soil_moisture_pct <= 34.5084f) {
        if (temperature_c <= 34.2589f) {
            if (temperature_c <= 9.0025f) {
                if (out_confidence) *out_confidence = 1.0000f;
                return 4; /* SENSOR_FAULT */
            } else {
                if (out_confidence) *out_confidence = 1.0000f;
                return 1; /* NEED_WATER */
            }
        } else {
            if (temperature_c <= 57.3397f) {
                if (out_confidence) *out_confidence = 1.0000f;
                return 2; /* HIGH_TEMPERATURE */
            } else {
                if (out_confidence) *out_confidence = 1.0000f;
                return 4; /* SENSOR_FAULT */
            }
        }
    } else {
        if (temperature_c <= 33.3087f) {
            if (light_level_pct <= 39.9507f) {
                if (temperature_c <= 6.4189f) {
                    if (out_confidence) *out_confidence = 1.0000f;
                    return 4; /* SENSOR_FAULT */
                } else {
                    if (out_confidence) *out_confidence = 1.0000f;
                    return 3; /* LOW_LIGHT */
                }
            } else {
                if (humidity_pct <= 49.8848f) {
                    if (out_confidence) *out_confidence = 1.0000f;
                    return 4; /* SENSOR_FAULT */
                } else {
                    if (out_confidence) *out_confidence = 1.0000f;
                    return 0; /* NORMAL */
                }
            }
        } else {
            if (temperature_c <= 57.2386f) {
                if (out_confidence) *out_confidence = 1.0000f;
                return 2; /* HIGH_TEMPERATURE */
            } else {
                if (out_confidence) *out_confidence = 1.0000f;
                return 4; /* SENSOR_FAULT */
            }
        }
    }
}
