import os
import joblib
import numpy as np
import pandas as pd
from sklearn.model_selection import train_test_split
from sklearn.tree import DecisionTreeClassifier
from sklearn.metrics import classification_report, confusion_matrix, accuracy_score, f1_score

FEATURE_NAMES = ["soil_moisture_pct", "temperature_c", "humidity_pct", "light_level_pct"]
CLASS_NAMES = ["NORMAL", "NEED_WATER", "HIGH_TEMPERATURE", "LOW_LIGHT", "SENSOR_FAULT"]

def rule_based_baseline(X: np.ndarray) -> np.ndarray:
    """Heuristic rule-based baseline for benchmarking against the ML model"""
    preds = []
    for row in X:
        soil, temp, hum, light = row[0], row[1], row[2], row[3]
        if temp > 60.0 or temp < -5.0 or (soil == 0 and temp == 0 and hum == 0):
            preds.append(4)  # SENSOR_FAULT
        elif temp > 35.0:
            preds.append(2)  # HIGH_TEMPERATURE
        elif soil < 35.0:
            preds.append(1)  # NEED_WATER
        elif light < 20.0:
            preds.append(3)  # LOW_LIGHT
        else:
            preds.append(0)  # NORMAL
    return np.array(preds)

def train_and_evaluate():
    dataset_path = "ml/datasets/greenhouse_dataset.csv"
    if not os.path.exists(dataset_path):
        from ml.training.generate_dataset import generate_greenhouse_dataset
        df = generate_greenhouse_dataset(6000)
        df.to_csv(dataset_path, index=False)
    else:
        df = pd.read_csv(dataset_path)

    X = df[FEATURE_NAMES].values
    y = df["class_id"].values

    # Train / Val / Test split: 70% Train, 15% Val, 15% Test
    X_train, X_temp, y_train, y_temp = train_test_split(X, y, test_size=0.30, random_state=42, stratify=y)
    X_val, X_test, y_val, y_test = train_test_split(X_temp, y_temp, test_size=0.50, random_state=42, stratify=y_temp)

    print(f"Dataset Split: Train={len(X_train)}, Val={len(X_val)}, Test={len(X_test)}")

    # Baseline Evaluation
    baseline_preds = rule_based_baseline(X_test)
    base_acc = accuracy_score(y_test, baseline_preds)
    base_f1 = f1_score(y_test, baseline_preds, average="macro")
    print("\n--- Rule-Based Baseline Performance ---")
    print(f"Accuracy: {base_acc * 100:.2f}% | Macro F1: {base_f1 * 100:.2f}%")

    # Train Compact Decision Tree (Ideal for TinyML / ESP32-S3 execution)
    model = DecisionTreeClassifier(
        max_depth=6,
        min_samples_leaf=5,
        criterion="gini",
        random_state=42
    )
    model.fit(X_train, y_train)

    # Validation
    val_preds = model.predict(X_val)
    val_acc = accuracy_score(y_val, val_preds)
    print(f"\nValidation Accuracy: {val_acc * 100:.2f}%")

    # Test Evaluation
    test_preds = model.predict(X_test)
    test_acc = accuracy_score(y_test, test_preds)
    test_f1 = f1_score(y_test, test_preds, average="macro")
    print("\n--- Decision Tree (TinyML) Test Performance ---")
    print(f"Accuracy: {test_acc * 100:.2f}% | Macro F1: {test_f1 * 100:.2f}%")
    print("\nClassification Report:\n", classification_report(y_test, test_preds, target_names=CLASS_NAMES))
    print("Confusion Matrix:\n", confusion_matrix(y_test, test_preds))

    # Save Model Artifact
    os.makedirs("ml/export", exist_ok=True)
    joblib.dump(model, "ml/export/model.joblib")
    print("\nSaved trained model to ml/export/model.joblib")

    # Also save test samples for C transpiler parity test
    test_df = pd.DataFrame(X_test[:500], columns=FEATURE_NAMES)
    test_df["true_class"] = y_test[:500]
    test_df["dt_pred"] = test_preds[:500]
    test_df.to_csv("ml/export/test_samples.csv", index=False)
    print("Saved 500 test samples to ml/export/test_samples.csv")

    return model

if __name__ == "__main__":
    train_and_evaluate()
