import os
import joblib
import numpy as np
import pandas as pd
from sklearn.metrics import classification_report, confusion_matrix, accuracy_score, f1_score
from ml.training.train_model import FEATURE_NAMES, CLASS_NAMES, rule_based_baseline

def run_comprehensive_evaluation():
    print("=======================================================")
    print("   SMARTGROW AI — EDGE AI COMPREHENSIVE EVALUATION     ")
    print("=======================================================")

    model_path = "ml/export/model.joblib"
    if not os.path.exists(model_path):
        from ml.training.train_model import train_and_evaluate
        train_and_evaluate()

    model = joblib.load(model_path)

    # 1. Evaluate on clean held-out test samples
    test_samples_path = "ml/export/test_samples.csv"
    if not os.path.exists(test_samples_path):
        raise FileNotFoundError(f"{test_samples_path} not found")

    df_test = pd.read_csv(test_samples_path)
    X_test = df_test[FEATURE_NAMES].values
    y_test = df_test["true_class"].values

    test_preds = model.predict(X_test)
    test_acc = accuracy_score(y_test, test_preds)
    test_f1 = f1_score(y_test, test_preds, average="macro")

    print("\n--- 1. Clean Held-out Test Set (500 Samples) ---")
    print(f"Accuracy: {test_acc * 100:.2f}% | Macro F1: {test_f1 * 100:.2f}%")
    print("\nClassification Report:\n", classification_report(y_test, test_preds, target_names=CLASS_NAMES))
    cm = confusion_matrix(y_test, test_preds)
    print("Confusion Matrix:\n", cm)

    # 2. Rule-Based Baseline Comparison
    base_preds = rule_based_baseline(X_test)
    base_acc = accuracy_score(y_test, base_preds)
    base_f1 = f1_score(y_test, base_preds, average="macro")
    print("\n--- 2. Heuristic Rule-Based Baseline on Same Test Set ---")
    print(f"Baseline Accuracy: {base_acc * 100:.2f}% | Baseline Macro F1: {base_f1 * 100:.2f}%")

    # 3. Robustness Evaluation with Sensor Noise (Gaussian Jitter)
    np.random.seed(42)
    noise = np.random.normal(0, 1.5, X_test.shape) # 1.5 units std noise
    X_noisy = np.clip(X_test + noise, 0.0, 100.0)
    noisy_preds = model.predict(X_noisy)
    noisy_acc = accuracy_score(y_test, noisy_preds)
    noisy_f1 = f1_score(y_test, noisy_preds, average="macro")
    print("\n--- 3. Robustness under Sensor Jitter (+/- 1.5 unit noise) ---")
    print(f"Noisy Accuracy: {noisy_acc * 100:.2f}% | Noisy Macro F1: {noisy_f1 * 100:.2f}%")

    # 4. Critical Decision Threshold Boundary Stress Test
    # Decision boundaries: Soil ~35%, Temp ~35.5C, Light ~20%
    boundary_samples = [
        # Exactly at and around 35.0% soil moisture
        [34.9, 25.0, 60.0, 70.0], # Just below -> NEED_WATER (Class 1)
        [35.1, 25.0, 60.0, 70.0], # Just above -> NORMAL (Class 0)
        # Exactly at and around 35.0 C temp
        [50.0, 34.9, 60.0, 70.0], # Just below -> NORMAL (Class 0)
        [50.0, 35.6, 60.0, 70.0], # Just above -> HIGH_TEMP (Class 2)
        # Exactly at and around 20.0% light
        [50.0, 25.0, 60.0, 19.4], # Just below -> LOW_LIGHT (Class 3)
        [50.0, 25.0, 60.0, 20.5], # Just above -> NORMAL (Class 0)
        # Extreme out-of-bounds sensor fault
        [0.0, 85.0, 10.0, 20.0],   # Temp 85C -> SENSOR_FAULT (Class 4)
        [0.0, -15.0, 10.0, 20.0],  # Temp -15C -> SENSOR_FAULT (Class 4)
    ]
    b_preds = model.predict(np.array(boundary_samples))
    print("\n--- 4. Decision Boundary Transitions ---")
    for i, s in enumerate(boundary_samples):
        pred_label = CLASS_NAMES[b_preds[i]]
        print(f"Sample {s} -> Predicted: {pred_label} (Class {b_preds[i]})")

    # Write evaluation report to docs
    os.makedirs("docs", exist_ok=True)
    report_path = "docs/ai-model.md"
    with open(report_path, "w", encoding="utf-8") as f:
        f.write(f"""# SmartGrow AI — Edge AI & TinyML Model Report

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
| **Clean Held-out Test Set (500 samples)** | {test_acc * 100:.2f}% | {test_f1 * 100:.2f}% | Stratified test split |
| **Heuristic Rule Baseline** | {base_acc * 100:.2f}% | {base_f1 * 100:.2f}% | Benchmark heuristic |
| **Gaussian Noise Robustness (+/- 1.5 units)** | {noisy_acc * 100:.2f}% | {noisy_f1 * 100:.2f}% | Simulates sensor ADC noise |

### Classification Report (Test Set):
```
{classification_report(y_test, test_preds, target_names=CLASS_NAMES)}
```

### Confusion Matrix:
```
{cm}
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
""")
    print(f"\nSaved updated AI report to {report_path}")

if __name__ == "__main__":
    run_comprehensive_evaluation()
