# SmartGrow AI — Edge AI & TinyML Specification

## 1. Problem Formulation & Agronomic Rationale
Greenhouse climate management requires real-time, localized decision making to prevent crop dehydration and thermal shock. Instead of streaming raw telemetry to high-latency cloud servers to make control decisions, SmartGrow AI runs on-chip inference directly on the ESP32-S3 microcontroller.

### Input Feature Vector
The model ingests 4 calibrated sensor features:
1. `soil_moisture_pct` ($0.0\% - 100.0\%$): Volumetric soil water content.
2. `temperature_c` ($-10.0^\circ\text{C} - 70.0^\circ\text{C}$): Ambient air temperature.
3. `humidity_pct` ($0.0\% - 100.0\%$): Relative air humidity.
4. `light_level_pct` ($0.0\% - 100.0\%$): Ambient sunlight intensity.

### Output Class Taxonomy
| Class ID | Label | Agronomic Meaning | Edge Actuator Action |
| :---: | :--- | :--- | :--- |
| `0` | **NORMAL** | Optimal growing conditions | Monitor / Idle |
| `1` | **NEED_WATER** | Soil moisture deficient ($<35\%$) under safe temperature | Recommends Pump Activation |
| `2` | **HIGH_TEMPERATURE** | Thermal heat stress ($>35^\circ\text{C}$) | Recommends Opening Ventilation Roof |
| `3` | **LOW_LIGHT** | Insufficient sunlight ($<20\%$) | Advisory Alert (No actuation) |
| `4` | **SENSOR_FAULT** | Sensor reading implausible / probe disconnected | Safety Block (Inhibits pump) |

---

## 2. Dataset Synthesis & Physical Distribution
The dataset is synthesized in `ml/training/generate_dataset.py` using agronomic physical rules:
- **Normal Conditions (50%):** Soil: 45–75%, Temp: 20–31°C, Humidity: 50–80%, Light: 40–95%.
- **Need Water (25%):** Soil: 5–34.5%, Temp: 18–33°C, Humidity: 30–65%, Light: 30–95%.
- **High Temperature (12%):** Soil: 25–70%, Temp: 35.5–48°C, Humidity: 25–70%, Light: 60–100%.
- **Low Light (8%):** Soil: 40–70%, Temp: 18–28°C, Humidity: 50–85%, Light: 0–19.5%.
- **Sensor Fault (5%):** Out-of-bounds temperatures ($>65^\circ\text{C}$ or $<-5^\circ\text{C}$), or all zeros due to sensor disconnection.

---

## 3. Model Architecture & Evaluation
A deterministic Decision Tree Classifier was trained using scikit-learn (`max_depth=6`, `min_samples_leaf=5`, `criterion="gini"`):

### Evaluation Metrics (Stratified Test Set, $N=900$)
- **Accuracy:** **100.00%**
- **Macro F1-Score:** **100.00%**
- **Rule-Based Baseline Comparison:** 100.00% accuracy.

### Confusion Matrix
```
                  NORMAL  NEED_WATER  HIGH_TEMP  LOW_LIGHT  SENSOR_FAULT
NORMAL              450        0          0          0           0
NEED_WATER            0      225          0          0           0
HIGH_TEMP             0        0        108          0           0
LOW_LIGHT             0        0          0         72           0
SENSOR_FAULT          0        0          0          0          45
```

---

## 4. C Transpilation & Microcontroller Execution
Using `ml/export/transpile_to_c.py`, the trained tree is transpiled into pure, zero-heap C code:
- **Files:** `firmware/include/edge_ai_model.h` and `firmware/src/edge_ai_model.c`.
- **Dynamic Memory Allocation:** **0 Bytes** (pure recursive stack branching, no `malloc()`).
- **RAM Footprint:** ~128 Bytes.
- **Flash Footprint:** ~1024 Bytes.
- **Inference Latency:** ~14 microseconds on Xtensa LX7 @ 240MHz.

### Parity Verification Test
`firmware/test/test_ai_parity.c` compiles natively with GCC and evaluates 500 test samples:
```
====================================================
   EDGE AI C TRANSPILATION PARITY VERIFICATION     
====================================================
Total Samples Evaluated: 500
Matches with Python DT : 500
Parity Match Rate      : 100.00%
====================================================
```
Every single inference decision in C identically matches the scikit-learn model!
