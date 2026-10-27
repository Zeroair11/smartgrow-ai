# SmartGrow AI — Edge AI & TinyML Model Report

**Model Version:** v1.0.0-dt  
**Architecture:** Deterministic C Decision Tree (`ml/export/transpile_to_c.py` -> `firmware/src/edge_ai_model.c`)  
**Target Hardware:** ESP32-S3 N16R8 (Zero Heap Allocation, 100% C Static Stack)  
**Input Features:**
1. `soil_moisture_pct` (float, 0.0 - 100.0%)
2. `temperature_c` (float, -10.0 - 70.0°C)
3. `humidity_pct` (float, 0.0 - 100.0%)
4. `light_level_pct` (float, 0.0 - 100.0%)

**Output Classes:**
- `0`: `NORMAL` — Optimal environmental conditions.
- `1`: `NEED_WATER` — Soil moisture below crop threshold.
- `2`: `HIGH_TEMPERATURE` — Heat stress requiring ventilation.
- `3`: `LOW_LIGHT` — Insufficient ambient photosynthetically active radiation.
- `4`: `SENSOR_FAULT` — Physical plausibility limits breached or disconnected probe.

---

## 1. Performance Metrics

| Evaluation Condition | Accuracy | Macro F1 | Notes |
|---|---|---|---|
| **Clean Held-out Test Set (500 samples)** | 100.00% | 100.00% | Stratified test split |
| **Heuristic Rule Baseline** | 100.00% | 100.00% | Benchmark heuristic |
| **Gaussian Noise Robustness (+/- 1.5 units)** | 98.20% | 97.36% | Simulates sensor ADC noise |

### Classification Report (Test Set):
```
                  precision    recall  f1-score   support

          NORMAL       1.00      1.00      1.00       262
      NEED_WATER       1.00      1.00      1.00       119
HIGH_TEMPERATURE       1.00      1.00      1.00        62
       LOW_LIGHT       1.00      1.00      1.00        38
    SENSOR_FAULT       1.00      1.00      1.00        19

        accuracy                           1.00       500
       macro avg       1.00      1.00      1.00       500
    weighted avg       1.00      1.00      1.00       500

```

### Confusion Matrix:
```
[[262   0   0   0   0]
 [  0 119   0   0   0]
 [  0   0  62   0   0]
 [  0   0   0  38   0]
 [  0   0   0   0  19]]
```

---

## 2. Dataset Assumptions and Critical Scientific Limitations

> [!WARNING]
> **Synthetic Dataset Limitation:**
> The training dataset was synthetically generated using agronomic boundary definitions. High accuracy on synthetic data demonstrates successful model fitting to specified rules, but does **not** validate agronomic efficacy on live biological crops.
>
> In real greenhouse deployments:
> 1. Soil moisture varies non-linearly depending on soil composition (clay, sand, coco peat).
> 2. Diurnal cycles and solar radiation introduce sensor drift.
> 3. Capillary action creates spatial gradients not captured by a single probe.

---

## 3. Safety Supervisor Architecture (AI Interlock)

Under no circumstances is Edge AI inference permitted to activate physical water pumps without explicit approval from the firmware `SafetyManager`:
1. **Cooldown Guard:** Even if AI outputs `NEED_WATER` with 100% confidence, pump activation is rejected if 180s cooldown is active.
2. **Watchdog Guard:** Pump run time is hard-limited to 120s by monotonic hardware timer.
3. **Sensor Fault Guard:** If any sensor is invalid or out-of-plausible-range, AI output is overridden and pump activation is blocked.
