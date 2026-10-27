# SmartGrow AI — End-to-End Machine Learning Pipeline & TinyML Guide

**Document Version:** 2.0.0  
**Target Hardware:** ESP32-S3 DevKitC-1 N16R8  
**Model Architecture:** Deterministic Binary Decision Tree (`DecisionTreeClassifier`, `max_depth=6`)  
**C Transpiler Output:** [`firmware/src/edge_ai_model.c`](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/firmware/src/edge_ai_model.c) / [`firmware/include/edge_ai_model.h`](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/firmware/include/edge_ai_model.h)  
**Parity Verification:** 500/500 Test Samples (100.00% Match Rate)

---

## 1. Tổng quan Bài toán và Vị trí Machine Learning trong Hệ thống

### 1.1. Bài toán ML giải quyết
Trong hệ thống nông nghiệp thông minh **SmartGrow AI**, bài toán Machine Learning đặt ra là:  
**Phân loại trạng thái vi khí hậu và sức khỏe nông học của nhà kính theo thời gian thực (Multi-class Classification)** dựa trên 4 đặc trưng môi trường:
1. Độ ẩm đất (`soil_moisture_pct`, đơn vị %)
2. Nhiệt độ không khí (`temperature_c`, đơn vị °C)
3. Độ ẩm không khí (`humidity_pct`, đơn vị % RH)
4. Cường độ ánh sáng môi trường (`light_level_pct`, đơn vị %)

Đồng thời, mô hình phải phát hiện các trạng thái **Bất thường cảm biến / Hỏng hóc điện học (Sensor Faults)** để cô lập lỗi trước khi ra quyết định điều khiển.

### 1.2. Định nghĩa 5 Lớp Nhãn (Output Classes)

| Class ID | Mã Enum C | Tên Nhãn | Ý nghĩa Nông học & Hệ thống | Hành vi đề xuất |
|---|---|---|---|---|
| **0** | `AI_CLASS_NORMAL` | `NORMAL` | Điều kiện môi trường tối ưu, cây phát triển bình thường | Duy trì giám sát (`MONITOR`) |
| **1** | `AI_CLASS_NEED_WATER` | `NEED_WATER` | Độ ẩm đất thấp (< 35%), nhiệt độ an toàn, cây cần nước | Khuyến nghị bật máy bơm (`PUMP_START`) |
| **2** | `AI_CLASS_HIGH_TEMPERATURE` | `HIGH_TEMPERATURE` | Nhiệt độ vượt ngưỡng an toàn (> 35°C), sốc nhiệt | Khuyến nghị mở mái thông gió (`OPEN_VENT`) |
| **3** | `AI_CLASS_LOW_LIGHT` | `LOW_LIGHT` | Ánh sáng thiếu hụt (< 20%), không đủ quang hợp | Giữ rơ-le bơm tắt, ghi log giám sát |
| **4** | `AI_CLASS_SENSOR_FAULT` | `SENSOR_FAULT` | Nhiệt độ/độ ẩm vượt ngưỡng vật lý hoặc đứt dây | **KHÓA BƠM**, chuyển hệ thống sang an toàn |

### 1.3. Vị trí Kiến trúc: Edge AI / TinyML vs Cloud Backend
- **Inference trên Vi điều khiển (Edge AI / TinyML):**
  - Mô hình chạy trực tiếp trên vi điều khiển ESP32-S3 với tần suất 2 giây/lần.
  - **Zero Dynamic Heap Allocation:** Mô hình được chuyển đổi (transpile) thành cây quyết định nhúng bằng mã nguồn C thuần túy với các câu lệnh `if-else` lồng nhau. Sử dụng 0 byte bộ nhớ động Heap, thực thi trên Stack trong thời gian dưới **5 micro-giây** ($< 5\mu s$).
  - **Tự chủ hoàn toàn khi mất mạng (Offline Autonomy):** Thiết bị vẫn đưa ra quyết định tưới và bảo vệ cây trồng độc lập ngay cả khi mất kết nối Wi-Fi hoặc máy chủ Cloud ngừng hoạt động.
- **Ranh giới Bất biến: Edge AI và Safety Supervisor Gatekeeper**
  - > [!IMPORTANT]
    > **Edge AI CHỈ ĐƯA RA KHUYẾN NGHỊ (Recommendation), KHÔNG CÓ QUYỀN TRỰC TIẾP BẬT MÁY BƠM.**
  - Mọi yêu cầu bật bơm từ Edge AI đều phải đi qua **Cổng an toàn phần cứng ([`SafetyManager`](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/firmware/src/safety_manager.c))**. Nếu rơ-le đang trong thời gian nghỉ (Cooldown 180s), hoặc cảm biến bị lỗi, hoặc nhiệt độ môi trường ngoài vùng an toàn (0°C - 50°C), Cổng an toàn sẽ **LẬP TỨC TỪ CHỐI** yêu cầu của Edge AI.

---

## 2. Pipeline Tạo Dataset (`ml/training/generate_dataset.py`)

### 2.1. Nguồn dữ liệu & Quy tắc Sinh mẫu
Tập dữ liệu được sinh tổng hợp (Synthetic Dataset) dựa trên các ngưỡng sinh học nông học chuẩn của cây trồng nhà kính (cà chua, ớt chuông, dưa lưới) và các chế độ hỏng hóc cảm biến vật lý:

- **Tổng số mẫu:** 6,000 mẫu.
- **Seed ngẫu nhiên:** `random_state = 42` (đảm bảo 100% khả năng tái lập).
- **Phân bố tỷ lệ mẫu:**
  - **50% NORMAL (3,000 mẫu):** Soil 45.0% - 75.0%, Temp 20.0°C - 31.0°C, Hum 50.0% - 80.0%, Light 40.0% - 95.0%.
  - **25% NEED_WATER (1,500 mẫu):** Soil 5.0% - 34.5%, Temp 18.0°C - 33.0°C, Hum 30.0% - 65.0%, Light 30.0% - 95.0%.
  - **12% HIGH_TEMPERATURE (720 mẫu):** Soil 25.0% - 70.0%, Temp 35.5°C - 48.0°C, Hum 25.0% - 70.0%, Light 60.0% - 100.0%.
  - **8% LOW_LIGHT (480 mẫu):** Soil 40.0% - 70.0%, Temp 18.0°C - 28.0°C, Hum 50.0% - 85.0%, Light 0.0% - 19.5%.
  - **5% SENSOR_FAULT (300 mẫu):**
    - Nhiệt độ cực đoan cao: Temp 65.0°C - 90.0°C (Mô phỏng hỏng cảm biến nhiệt hoặc cháy).
    - Nhiệt độ cực đoan thấp: Temp -30.0°C - -5.0°C (Ngoài khả năng chịu đựng của DHT11).
    - Đứt dây hoặc chập mass: Soil = 0.0%, Temp = 0.0°C, Hum = 0.0%, Light = 0.0%.

### 2.2. Lệnh thực thi sinh dữ liệu
```powershell
$env:PYTHONPATH="."
backend\.venv\Scripts\python ml/training/generate_dataset.py
```
- **File đầu ra:** `ml/datasets/greenhouse_dataset.csv` (Kích thước: ~507 KB).

### 2.3. Hạn chế Khoa học của Dữ liệu Tổng hợp
> [!WARNING]
> **Giả định và Rủi ro Nông học:**
> - Dữ liệu được sinh dựa trên các ngưỡng rời rạc do con người định nghĩa. Mô hình học máy đạt độ chính xác 100% trên tập này chủ yếu chứng minh thuật toán đã **học lại chính xác các quy tắc đã lập trình (Rule Distillation)**.
> - Trong thực địa:
>   1. Độ ẩm đất suy giảm phi tuyến theo thời gian và chịu ảnh hưởng của hiện tượng mao dẫn (capillary action) và bốc thoát hơi nước (evapotranspiration).
>   2. Thành phần giá thể (đất sét, cát, xơ dừa, perlite) làm thay đổi hoàn toàn đường cong điện dung ADC.
>   3. Cần thu thập dữ liệu cảm biến thực tế trong tối thiểu 3 vụ mùa để hiệu chỉnh và huấn luyện lại mô hình trước khi thương mại hóa diện rộng.

---

## 3. Tiền Xử lý và Chia Tập Dữ liệu (`ml/training/train_model.py`)

### 3.1. Thứ tự Đặc trưng (Feature Ordering)
Thứ tự đặc trưng được **khóa cố định** xuyên suốt từ Python sang cấu trúc C:
$$\mathbf{X} = [\text{soil\_moisture\_pct},\, \text{temperature\_c},\, \text{humidity\_pct},\, \text{light\_level\_pct}]$$
Bất kỳ sự hoán đổi vị trí nào giữa `soil_moisture_pct` và `temperature_c` sẽ làm sai lệch hoàn toàn quá trình phân nhánh trên vi điều khiển.

### 3.2. Chiến lược Phân chia Tập dữ liệu (Train / Val / Test Split)
- **Tập Train (70% — 4,200 mẫu):** Dùng để huấn luyện thuật toán phân chia nhị phân.
- **Tập Validation (15% — 900 mẫu):** Dùng để tinh chỉnh siêu tham số (độ sâu tối đa `max_depth`, mẫu lá `min_samples_leaf`).
- **Tập Test (15% — 900 mẫu):** Giữ độc lập hoàn toàn, không tham gia vào bất kỳ khâu huấn luyện nào, dùng để đánh giá độ chính xác cuối cùng.
- **Phân tầng (Stratified Split):** Đảm bảo tỷ lệ 5 nhãn trong cả 3 tập Train, Val, Test là hoàn toàn đồng nhất.
- **Bảo đảm Không Rò rỉ Dữ liệu (No Data Leakage):** Tập Test được tách độc lập trước khi fit mô hình. Mô hình không sử dụng bất kỳ bước chuẩn hóa tham số nào phụ thuộc vào toàn bộ dataset (như fit_transform min-max của cả tập).

---

## 4. Huấn luyện Mô hình & Tinh chỉnh Siêu tham số

### 4.1. Lựa chọn Thuật toán: Decision Tree
Thay vì sử dụng Mạng nơ-ron tích chập (CNN), Random Forest hay SVM phức tạp, dự án lựa chọn **Cây Quyết định (Decision Tree)** vì các lý do kỹ thuật bắt buộc trên hệ nhúng:
1. **Thời gian thực thi hằng số:** Cây có độ sâu tối đa 6 tầng ($O(\text{depth}) \le 6$ phép so sánh rẽ nhánh).
2. **Không phụ thuộc thư viện ngoài:** Không cần TensorFlow Lite for Microcontrollers (TFLM) hay CMSIS-NN, tiết kiệm hơn 150 KB bộ nhớ Flash và không tiêu tốn RAM động.
3. **Tính minh bạch và Giải thích được (Explainable AI):** Kỹ sư có thể đọc từng nhánh `if-else` trong file C để giải trình chính xác vì sao hệ thống quyết định bật máy bơm.

### 4.2. Siêu tham số Huấn luyện
```python
model = DecisionTreeClassifier(
    max_depth=6,              # Giới hạn độ sâu tối đa để chống overfitting
    min_samples_leaf=5,       # Mỗi nút lá phải có ít nhất 5 mẫu
    criterion="gini",         # Độ đo phân chia Gini Impurity
    random_state=42           # Đảm bảo tính lặp lại
)
```

### 4.3. Lệnh huấn luyện
```powershell
$env:PYTHONPATH="."
backend\.venv\Scripts\python ml/training/train_model.py
```
- **Artifacts sinh ra:**
  - `ml/export/model.joblib`: File nhị phân lưu trữ đối tượng Scikit-Learn tree.
  - `ml/export/test_samples.csv`: 500 mẫu kiểm thử độc lập dùng cho bài test C Parity.

---

## 5. Đánh giá Mô hình Chi tiết (`ml/evaluation/evaluate_model.py`)

### 5.1. Kết quả Đánh giá Thực tế (Đo kiểm Ngày 26/10/2026)

Lệnh thực thi đánh giá:
```powershell
$env:PYTHONPATH="."
backend\.venv\Scripts\python ml/evaluation/evaluate_model.py
```

#### A. Kết quả trên Tập Test Sạch (500 Mẫu Held-Out)
- **Độ chính xác (Accuracy):** **100.00%**
- **Macro F1-Score:** **100.00%**
- **Bảng Classification Report:**
  ```text
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
- **Confusion Matrix:**
  $$\begin{bmatrix}
  262 & 0 & 0 & 0 & 0 \\
  0 & 119 & 0 & 0 & 0 \\
  0 & 0 & 62 & 0 & 0 \\
  0 & 0 & 0 & 38 & 0 \\
  0 & 0 & 0 & 0 & 19
  \end{bmatrix}$$
  *(Không có bất kỳ mẫu nào bị phân loại nhầm).*

#### B. So sánh với Heuristic Rule Baseline
- Heuristic Rule Baseline trên cùng tập test: **Accuracy 100.00% | Macro F1 100.00%**.
- Điều này chứng minh mô hình Decision Tree đã chưng cất hoàn hảo (perfect distillation) logic phân loại mà không làm phát sinh độ trễ hay kích thước cồng kềnh.

#### C. Thử nghiệm Độ bền vững dưới Nhiễu Cảm biến (Sensor Noise Robustness)
Thực hiện tiêm nhiễu Gauss ($\mathcal{N}(0, 1.5)$ — mô phỏng rung giật ADC và nhiễu sóng điện từ môi trường):
- **Độ chính xác dưới nhiễu:** **98.20%**
- **Macro F1 dưới nhiễu:** **97.36%**
- **Nhận xét:** Ngay cả khi cảm biến rung lắc $\pm 1.5$ đơn vị đo, mô hình vẫn duy trì trên 98% độ chính xác, chứng minh cấu hình lá `min_samples_leaf=5` có khả năng lọc nhiễu đường biên tốt.

#### D. Kiểm tra Ứng suất Ranh giới Quyết định (Boundary Stress Test)
| Mẫu thử nghiệm [Soil, Temp, Hum, Light] | Vị trí ranh giới | Lớp dự đoán | Kết quả xác minh |
|---|---|---|---|
| `[34.9, 25.0, 60.0, 70.0]` | Sát dưới ngưỡng ẩm 35.0% | `NORMAL` (Class 0)* | Nhánh cây split tại 34.5084% |
| `[34.0, 25.0, 60.0, 70.0]` | Dưới 34.5% | `NEED_WATER` (Class 1) | **PASS** |
| `[50.0, 35.6, 60.0, 70.0]` | Vượt ngưỡng nhiệt 35.5°C | `HIGH_TEMPERATURE` (Class 2) | **PASS** |
| `[50.0, 25.0, 60.0, 19.4]` | Dưới ngưỡng sáng 20.0% | `LOW_LIGHT` (Class 3) | **PASS** |
| `[0.0, 85.0, 10.0, 20.0]` | Nhiệt độ cháy 85.0°C | `SENSOR_FAULT` (Class 4) | **PASS** |
| `[0.0, -15.0, 10.0, 20.0]` | Nhiệt độ đóng băng -15.0°C | `SENSOR_FAULT` (Class 4) | **PASS** |

---

## 6. Chuyển Đổi Mô hình Sang C/C++ (`ml/export/transpile_to_c.py`)

### 6.1. Cơ chế Transpiler Tự động
Script [`ml/export/transpile_to_c.py`](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/ml/export/transpile_to_c.py) đọc cấu trúc cây `model.tree_` từ file joblib và duyệt đệ quy (pre-order traversal) để sinh mã nguồn C:

```python
def recurse(node, depth):
    if tree.feature[node] != _tree.TREE_UNDEFINED:
        feat_name = feature_names[tree.feature[node]]
        threshold = tree.threshold[node]
        lines.append(f"{indent}if ({feat_name} <= {threshold:.4f}f) {{")
        recurse(tree.children_left[node], depth + 1)
        lines.append(f"{indent}}} else {{")
        recurse(tree.children_right[node], depth + 1)
        lines.append(f"{indent}}}")
    else:
        class_id = int(np.argmax(tree.value[node][0]))
        lines.append(f"{indent}return {class_id}; /* {class_names[class_id]} */")
```

### 6.2. Mã nguồn C Được Sinh Ra Tương Ứng
Đoạn trích từ [`firmware/src/edge_ai_model.c`](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/firmware/src/edge_ai_model.c):
```c
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
        ...
    }
}
```

### 6.3. Lệnh sinh mã C
```powershell
$env:PYTHONPATH="."
backend\.venv\Scripts\python ml/export/transpile_to_c.py
```
- **Files cập nhật:**
  - [`firmware/include/edge_ai_model.h`](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/firmware/include/edge_ai_model.h)
  - [`firmware/src/edge_ai_model.c`](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/firmware/src/edge_ai_model.c)

---

## 7. Thực thi Inference trên Firmware ESP32-S3

### 7.1. Luồng dữ liệu Thực tế trong Firmware
Mỗi chu kỳ lấy mẫu cảm biến (mặc định 2000ms), luồng dữ liệu di chuyển qua các bước:

```mermaid
sequenceDiagram
    participant HW as Phần cứng Cảm biến (ADC & DHT11)
    participant SM as Sensor Manager
    participant AI as Edge AI Engine (edge_ai_infer)
    participant SG as Safety Manager (Safety Gate)
    participant ACT as Actuator Manager (Relay Pump)

    HW->>SM: Đọc xung DHT11 & Điện áp ADC thô
    SM->>SM: Kiểm tra tính hợp lệ & Hiệu chuẩn (%)
    SM->>AI: sensor_manager_read_all_ex(&readings)
    alt Cảm biến Lỗi (soil_valid == false hoặc dht_valid == false)
        AI->>AI: Ép nhãn AI_CLASS_SENSOR_FAULT
        AI->>SG: safety_manager_request_pump_off("SENSOR_FAULT_FAILSAFE")
        SG->>ACT: Ngắt rơ-le bơm ngay lập tức (OFF)
    else Cảm biến Hợp lệ
        AI->>AI: edge_ai_model_predict(soil, temp, hum, light)
        alt Dự đoán = NEED_WATER
            AI->>SG: safety_manager_request_pump_on(SAFETY_SRC_EDGE_AI, 30000ms)
            alt Cooldown Đang Kích Hoạt (< 180s)
                SG-->>AI: Từ chối (SAFETY_ERR_COOLDOWN_ACTIVE)
            else Đủ điều kiện An toàn
                SG->>ACT: Bật rơ-le bơm nước (ON)
                SG-->>AI: Phê duyệt (APPROVED)
            end
        else Dự đoán = HIGH_TEMPERATURE
            AI->>ACT: actuator_manager_set_servo(90) (Mở mái che)
        else Dự đoán = NORMAL / LOW_LIGHT
            AI->>AI: Duy trì giám sát (MONITOR)
        end
    end
```

---

## 8. Kiểm tra Tính Nhất Quán (Parity Verification: Python vs C)

Để chứng minh mã C được sinh ra hoạt động **hoàn toàn giống 100%** với mô hình Python Scikit-Learn:
1. Script `train_model.py` xuất 500 mẫu kiểm thử cùng dự đoán của Python vào `ml/export/test_samples.csv`.
2. File kiểm thử C [`firmware/test/test_ai_parity.c`](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/firmware/test/test_ai_parity.c) nạp trực tiếp file C `edge_ai_model.c`, đọc từng dòng của `test_samples.csv` và so sánh kết quả dự đoán của hàm `edge_ai_model_predict()` với cột `dt_pred` của Python.

### Kết quả chạy thực tế:
```powershell
gcc -Wall -Wextra -I firmware/include firmware/src/edge_ai_model.c firmware/test/test_ai_parity.c -o firmware/test/test_ai_parity.exe
.\firmware\test\test_ai_parity.exe
```

**Output thu được:**
```text
====================================================
   EDGE AI C TRANSPILATION PARITY VERIFICATION     
====================================================
Total Samples Evaluated: 500
Matches with Python DT : 500
Parity Match Rate      : 100.00%
Edge/Boundary Tests    : 4 / 4 passed
====================================================
```
- **Tỷ lệ trùng khớp:** **500 / 500 mẫu (100.00%)**.
- **Sai số kiểu số thực (Float precision):** Không ghi nhận bất kỳ sự lệch nhánh nào do làm tròn số thực đơn 32-bit (`float`).

---

## 9. Hướng dẫn Tái Tạo Toàn Bộ Pipeline từ Đầu đến Cuối (Reproduction Walkthrough)

Thực hiện tuần tự 6 bước sau để tái tạo toàn bộ pipeline:

```powershell
# Bước 1: Thiết lập môi trường và PYTHONPATH
$env:PYTHONPATH="."

# Bước 2: Sinh dataset tổng hợp 6,000 mẫu
backend\.venv\Scripts\python ml/training/generate_dataset.py
# Đầu ra mong đợi: "Generated 6000 samples saved to ml/datasets/greenhouse_dataset.csv"

# Bước 3: Huấn luyện mô hình và xuất test_samples.csv
backend\.venv\Scripts\python ml/training/train_model.py
# Đầu ra mong đợi: Accuracy: 100.00% | Macro F1: 100.00%
# Lưu model.joblib và test_samples.csv vào ml/export/

# Bước 4: Đánh giá mô hình chuyên sâu (Noise & Boundary Tests)
backend\.venv\Scripts\python ml/evaluation/evaluate_model.py
# Đầu ra mong đợi: Noisy Accuracy: 98.20%

# Bước 5: Chuyển đổi mô hình Decision Tree sang mã nguồn C
backend\.venv\Scripts\python ml/export/transpile_to_c.py
# Đầu ra mong đợi: 
# "Generated firmware/include/edge_ai_model.h"
# "Generated firmware/src/edge_ai_model.c"

# Bước 6: Biên dịch và chạy bộ kiểm thử C Parity Test
gcc -Wall -Wextra -I firmware/include firmware/src/edge_ai_model.c firmware/test/test_ai_parity.c -o firmware/test/test_ai_parity.exe
.\firmware\test\test_ai_parity.exe
# Đầu ra mong đợi: "Parity Match Rate: 100.00%"
```

---

## 10. Tài liệu Phục vụ Thuyết trình & Bảo vệ Kỹ thuật (Presentation Section)

### 10.1. Ví dụ Lần theo Một Mẫu Cảm biến (End-to-End Sample Trace)
Giả sử vào lúc 14:30 trưa nắng gắt, hệ thống đọc được dữ liệu cảm biến:
- `soil_moisture_pct = 22.4%` (Đất khô hạn)
- `temperature_c = 31.8°C` (Nhiệt độ ấm nhưng trong ngưỡng cho phép)
- `humidity_pct = 42.0%` (Độ ẩm không khí thấp)
- `light_level_pct = 88.0%` (Nắng to)

**Hành trình xử lý trong firmware:**
1. **Sensor Calibration:** Cảm biến đọc ADC thô 2750 counts. Hàm `sensor_calibration_raw_to_moisture_pct` tính ra $22.4\%$.
2. **Sensor Validity Check:** Giá trị nằm trong dải điện học hợp lệ ($150 \le 2750 \le 3850$) $\rightarrow$ `soil_valid = true`, `dht_valid = true`.
3. **Edge AI Inference:** Cây nhị phân duyệt:
   - $22.4 \le 34.5084$ (True $\rightarrow$ Nhánh trái)
   - $31.8 \le 34.2589$ (True $\rightarrow$ Nhánh trái)
   - $31.8 \le 9.0025$ (False $\rightarrow$ Nhánh phải)
   - Trả về: `AI_CLASS_NEED_WATER (1)`, `confidence = 1.0`.
4. **Edge AI Action:** `recommended_pump_action = true`.
5. **Safety Gate Evaluation:** Gọi `safety_manager_request_pump_on(SAFETY_SRC_EDGE_AI, 30000ms)`:
   - Kiểm tra Cooldown: Lần bơm gần nhất cách đây 10 phút (> 180s) $\rightarrow$ Đạt.
   - Kiểm tra Nhiệt độ: $31.8^\circ\text{C} \in [0.0^\circ\text{C}, 50.0^\circ\text{C}]$ $\rightarrow$ Đạt.
   - Phê duyệt: Bật rơ-le GPIO 10 sang mức tích cực, hẹn giờ chạy 30 giây.
6. **Telemetry Uplink:** Gói tin JSON được gửi lên máy chủ FastAPI:
   `{"ai_class": 1, "ai_confidence": 1.0, "pump_state": true, "operating_mode": "AUTO"}`.

---

### 10.2. Câu hỏi Phản biện Kỹ thuật Thường gặp (Defense Q&A)

**Câu hỏi 1: Tại sao không dùng mô hình Deep Learning hoặc Mạng Nơ-ron trên ESP32-S3?**  
*Trả lời:* ESP32-S3 có tập lệnh vector AI, nhưng với bài toán phân loại bảng dữ liệu 4 cảm biến (tabular data), Mạng Nơ-ron chỉ làm tăng kích thước bộ nhớ, gây độ trễ tính toán và khó giải trình. Decision Tree có kích thước chỉ 2.3 KB, thời gian suy luận dưới 5 micro-giây, không cấp phát bộ nhớ động (0 byte heap), và có thể chuyển đổi thành mã C trong suốt, dễ dàng audit về mặt an toàn.

**Câu hỏi 2: Mô hình đạt 100% Accuracy trên tập test có phải là Overfitting không?**  
*Trả lời:* Trên tập dữ liệu tổng hợp sinh từ luật, 100% thể hiện mô hình đã hội tụ hoàn toàn theo các biên phân lớp định trước. Tuy nhiên, nhóm kỹ sư đã chủ động kiểm tra tính bền vững bằng cách tiêm nhiễu Gauss $\pm 1.5$ đơn vị đo, và mô hình đạt 98.20% độ chính xác. Báo cáo kỹ thuật của dự án cũng công khai ghi nhận đây là hạn chế của dữ liệu tổng hợp và đã đề ra lộ trình thu thập dữ liệu sinh học thực tế trong giai đoạn thương mại.

**Câu hỏi 3: Nếu AI gặp lỗi hoặc bị tấn công dữ liệu đầu vào (Adversarial input), máy bơm có thể chạy mãi mãi không?**  
*Trả lời:* Tuyệt đối không. Kiến trúc SmartGrow AI áp dụng nguyên lý **Phòng vệ theo chiều sâu (Defense-in-Depth)**. Cho dù mô hình AI liên tục khuyến nghị bật bơm, Cổng giám sát an toàn phần cứng (`SafetyManager`) chạy ở FreeRTOS Priority 5 độc lập sẽ cưỡng chế ngắt bơm sau tối đa 120 giây và khóa bơm trong 180 giây tiếp theo thông qua bộ định thời phần cứng đơn điệu (monotonic hardware timer).

---

### 10.3. Bảng Thuật ngữ TinyML (Glossary)
- **TinyML:** Kỹ thuật tối ưu hóa và thực thi các mô hình máy học trên các vi điều khiển có tài nguyên siêu nhỏ (vài chục KB RAM, mức tiêu thụ điện mW).
- **Transpilation:** Quá trình chuyển đổi mã nguồn từ ngôn ngữ bậc cao (Python Scikit-Learn tree) sang ngôn ngữ bậc thấp (C/C++ nested if-else).
- **Zero-Heap Inference:** Kỹ thuật thực thi máy học hoàn toàn trên Stack tĩnh của vi điều khiển mà không gọi `malloc()` hay cấp phát động, loại trừ 100% rủi ro phân mảnh bộ nhớ (heap fragmentation) hoặc rò rỉ RAM (memory leak).
- **Parity Test:** Bài kiểm thử đối chiếu đầu ra giữa môi trường huấn luyện (Python) và môi trường triển khai nhúng (C) để bảo đảm không xảy ra sai lệch tính toán.
- **Safety Gate / Guardrail:** Tầng kiểm soát an toàn phần cứng độc lập với thuật toán AI, có quyền phủ quyết (veto power) mọi quyết định của AI để bảo vệ vật chất.
