import os
import numpy as np
import pandas as pd

def generate_greenhouse_dataset(n_samples: int = 5000, random_state: int = 42) -> pd.DataFrame:
    np.random.seed(random_state)

    data = []

    # Distribution breakdown:
    # 50% Normal optimal conditions
    # 25% Need Water
    # 12% High Temperature stress
    # 8%  Low Light
    # 5%  Sensor Faults / Out-of-bounds

    n_normal = int(n_samples * 0.50)
    n_water = int(n_samples * 0.25)
    n_hightemp = int(n_samples * 0.12)
    n_lowlight = int(n_samples * 0.08)
    n_fault = n_samples - (n_normal + n_water + n_hightemp + n_lowlight)

    # 1. Normal Conditions (Class 0)
    for _ in range(n_normal):
        soil = np.random.uniform(45.0, 75.0)
        temp = np.random.uniform(20.0, 31.0)
        hum  = np.random.uniform(50.0, 80.0)
        light = np.random.uniform(40.0, 95.0)
        data.append([soil, temp, hum, light, 0, "NORMAL"])

    # 2. Need Water (Class 1) - Low soil moisture (< 35%), safe temp
    for _ in range(n_water):
        soil = np.random.uniform(5.0, 34.5)
        temp = np.random.uniform(18.0, 33.0)
        hum  = np.random.uniform(30.0, 65.0)
        light = np.random.uniform(30.0, 95.0)
        data.append([soil, temp, hum, light, 1, "NEED_WATER"])

    # 3. High Temperature Stress (Class 2) - Temp > 35°C
    for _ in range(n_hightemp):
        soil = np.random.uniform(25.0, 70.0)
        temp = np.random.uniform(35.5, 48.0)
        hum  = np.random.uniform(25.0, 70.0)
        light = np.random.uniform(60.0, 100.0)
        data.append([soil, temp, hum, light, 2, "HIGH_TEMPERATURE"])

    # 4. Low Light (Class 3) - Light < 20%, normal temp and moisture
    for _ in range(n_lowlight):
        soil = np.random.uniform(40.0, 70.0)
        temp = np.random.uniform(18.0, 28.0)
        hum  = np.random.uniform(50.0, 85.0)
        light = np.random.uniform(0.0, 19.5)
        data.append([soil, temp, hum, light, 3, "LOW_LIGHT"])

    # 5. Sensor Fault (Class 4) - Extreme out-of-bounds or physically impossible
    for _ in range(n_fault):
        fault_type = np.random.choice(["temp_extreme_high", "temp_extreme_low", "sensor_disconnected"])
        if fault_type == "temp_extreme_high":
            soil = np.random.uniform(30.0, 60.0)
            temp = np.random.uniform(65.0, 90.0)
            hum = np.random.uniform(10.0, 50.0)
            light = np.random.uniform(20.0, 80.0)
        elif fault_type == "temp_extreme_low":
            soil = np.random.uniform(30.0, 60.0)
            temp = np.random.uniform(-30.0, -5.0)
            hum = np.random.uniform(10.0, 50.0)
            light = np.random.uniform(20.0, 80.0)
        else:
            soil = 0.0  # probe pulled out or shorted
            temp = 0.0
            hum = 0.0
            light = 0.0
        data.append([soil, temp, hum, light, 4, "SENSOR_FAULT"])

    df = pd.DataFrame(data, columns=[
        "soil_moisture_pct", "temperature_c", "humidity_pct", "light_level_pct",
        "class_id", "class_label"
    ])
    # Shuffle
    df = df.sample(frac=1.0, random_state=random_state).reset_index(drop=True)
    return df

if __name__ == "__main__":
    os.makedirs("ml/datasets", exist_ok=True)
    df = generate_greenhouse_dataset(n_samples=6000)
    output_path = "ml/datasets/greenhouse_dataset.csv"
    df.to_csv(output_path, index=False)
    print(f"Generated {len(df)} samples saved to {output_path}")
    print("Class distribution:\n", df['class_label'].value_counts())
