import os
import joblib
import numpy as np
from sklearn.tree import _tree

def generate_c_code(tree, feature_names, class_names, model_version="v1.0.0-dt"):
    header_content = f"""/**
 * @file edge_ai_model.h
 * @brief Auto-generated TinyML Decision Tree Inference Engine for ESP32-S3
 * Model Version: {model_version}
 * Architecture: Zero-heap deterministic binary decision tree
 */

#ifndef EDGE_AI_MODEL_H
#define EDGE_AI_MODEL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {{
#endif

#define EDGE_AI_MODEL_VERSION      "{model_version}"
#define EDGE_AI_NUM_CLASSES        {len(class_names)}
#define EDGE_AI_NUM_FEATURES       {len(feature_names)}

typedef enum {{
    AI_CLASS_NORMAL           = 0,
    AI_CLASS_NEED_WATER       = 1,
    AI_CLASS_HIGH_TEMPERATURE = 2,
    AI_CLASS_LOW_LIGHT        = 3,
    AI_CLASS_SENSOR_FAULT     = 4
}} EdgeAIClass;

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
}}
#endif

#endif /* EDGE_AI_MODEL_H */
"""

    # Generate C implementation recursive tree traversal
    lines = []
    lines.append('#include "edge_ai_model.h"')
    lines.append('#include <stddef.h>')
    lines.append('')
    lines.append('static const char* s_class_labels[] = {')
    for c in class_names:
        lines.append(f'    "{c}",')
    lines.append('};')
    lines.append('')
    lines.append('const char* edge_ai_model_class_to_str(int class_id) {')
    lines.append(f'    if (class_id >= 0 && class_id < {len(class_names)}) {{')
    lines.append('        return s_class_labels[class_id];')
    lines.append('    }')
    lines.append('    return "UNKNOWN";')
    lines.append('}')
    lines.append('')
    lines.append('int edge_ai_model_predict(float soil_moisture_pct, float temperature_c, float humidity_pct, float light_level_pct, float *out_confidence) {')

    def recurse(node, depth):
        indent = "    " * depth
        if tree.feature[node] != _tree.TREE_UNDEFINED:
            feat_idx = tree.feature[node]
            feat_name = feature_names[feat_idx]
            threshold = tree.threshold[node]
            lines.append(f"{indent}if ({feat_name} <= {threshold:.4f}f) {{")
            recurse(tree.children_left[node], depth + 1)
            lines.append(f"{indent}}} else {{")
            recurse(tree.children_right[node], depth + 1)
            lines.append(f"{indent}}}")
        else:
            values = tree.value[node][0]
            total = np.sum(values)
            class_id = int(np.argmax(values))
            confidence = float(values[class_id] / total) if total > 0 else 1.0
            lines.append(f"{indent}if (out_confidence) *out_confidence = {confidence:.4f}f;")
            lines.append(f"{indent}return {class_id}; /* {class_names[class_id]} */")

    recurse(0, 1)
    lines.append('}')
    lines.append('')

    source_content = "\n".join(lines)
    return header_content, source_content

def transpile():
    model_path = "ml/export/model.joblib"
    if not os.path.exists(model_path):
        from ml.training.train_model import train_and_evaluate
        train_and_evaluate()

    model = joblib.load(model_path)
    feature_names = ["soil_moisture_pct", "temperature_c", "humidity_pct", "light_level_pct"]
    class_names = ["NORMAL", "NEED_WATER", "HIGH_TEMPERATURE", "LOW_LIGHT", "SENSOR_FAULT"]

    header, source = generate_c_code(model.tree_, feature_names, class_names)

    os.makedirs("firmware/include", exist_ok=True)
    os.makedirs("firmware/src", exist_ok=True)

    with open("firmware/include/edge_ai_model.h", "w") as f:
        f.write(header)
    print("Generated firmware/include/edge_ai_model.h")

    with open("firmware/src/edge_ai_model.c", "w") as f:
        f.write(source)
    print("Generated firmware/src/edge_ai_model.c")

if __name__ == "__main__":
    transpile()
