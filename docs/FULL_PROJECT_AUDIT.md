# SmartGrow AI — Comprehensive Project Re-Audit & Technical Verification Report

**Audit Date:** October 26, 2026  
**Auditor Roles:** Senior Embedded Systems Engineer, IoT Architect, Firmware Engineer, ML Engineer  
**Audit Scope:** Frontend, Backend, Database, Embedded Firmware, Machine Learning Pipeline, Integration Test Suites, Hardware Pinout & Wiring Specifications  
**Status:** Comprehensive Re-Audit Completed. All Software & Parity Verification Passed (100.0%).

---

## 1. Tóm tắt Kết quả Kiểm toán Cấp cao (Executive Summary)

Dự án **SmartGrow AI** là hệ thống nông nghiệp thông minh IoT khép kín từ rìa (Edge AI trên ESP32-S3) đến đám mây (Cloud Backend FastAPI) và giao diện điều hành thời gian thực (Vue 3 SPA).

Đợt kiểm toán toàn diện này đã thực hiện:
1. **Kiểm toán mã nguồn từ gốc:** Đọc trực tiếp và đối chiếu toàn bộ mã nguồn Frontend, Backend, Database Models/Migrations, Firmware, TinyML scripts, và các bộ kiểm thử tích hợp.
2. **Chuẩn hóa cấu hình tập trung:** Khắc phục tình trạng khai báo rải rác và magic numbers bằng việc thiết lập một đầu mối cấu hình compile-time duy nhất tại [`firmware/include/project_config.h`](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/firmware/include/project_config.h), tích hợp kiểm tra tĩnh thời điểm biên dịch (`_Static_assert`).
3. **Xác minh dứt điểm mâu thuẫn phần cứng:** Đối chiếu kỹ thuật vi mạch ESP32-S3 N16R8 để xác nhận chân rơ-le máy bơm là **GPIO 10** và chứng minh lý do **Tuyệt đối cấm sử dụng GPIO 16** (do GPIO 16 là đường truyền bus của Octal PSRAM/Flash tích hợp).
4. **Rà soát trọn vẹn pipeline Machine Learning:** Kiểm tra toàn bộ chu trình từ sinh dataset (6,000 mẫu), huấn luyện Decision Tree, đánh giá độ bền dưới nhiễu, chuyển đổi sang mã C không cấp phát động (Zero-heap C), và kiểm tra tính nhất quán (Parity) 500/500 mẫu đạt 100.00%.
5. **Thực thi đầy đủ các bộ kiểm thử phần mềm:**
   - **Firmware Native Tests:** **287 / 287 passed (100.0%)**
   - **TinyML C Parity Test:** **500 / 500 passed (100.0%)**
   - **Backend Pytest Suite:** **24 / 24 passed (100.0%)**
   - **Frontend Type-Check & Build:** **115 modules transformed, 0 errors, build in 43s (PASS)**
   - **Full End-to-End Vertical Flow:** **12 / 12 flows passed (PASS)**
   - **Firmware-Backend Contract:** **Level 3 contract verified (PASS)**

---

## 2. Kiến trúc Repository Thực tế & Trách nhiệm Module

### 2.1. Bản đồ Cấu trúc Repository Thực tế

```text
smartgrow-ai/
├── backend/                  # FastAPI REST API, SQLAlchemy ORM, Service Layer
│   ├── app/
│   │   ├── api/v1/           # API Endpoints (Auth, Devices, Telemetry, Commands, Rules, etc.)
│   │   ├── core/             # Configuration, Database Connection & Session Factory
│   │   ├── models/           # SQLAlchemy Data Entities (Users, Devices, Telemetry, etc.)
│   │   ├── schemas/          # Pydantic v2 DTOs (Request / Response validation)
│   │   └── services/         # Business Logic (Command Lifecycle, Telemetry, Automation)
│   └── tests/                # Pytest Test Suites (API, CRUD, Security, Regressions)
├── deploy/                   # Docker Compose & Deployment Scripts
├── docs/                     # Technical Guides, Architecture, Hardware & ML Specs
├── firmware/                 # ESP32-S3 ESP-IDF & Native Firmware Source
│   ├── include/              # Header Files (project_config.h, firmware_config.h, etc.)
│   ├── src/                  # C Implementations (main.c, safety_manager.c, dht11.c, etc.)
│   └── test/                 # Native C Test Suites & TinyML Parity Harness
├── frontend/                 # Vue 3 + TypeScript + Vite + Tailwind CSS SPA
│   └── src/
│       ├── api/              # Axios HTTP Client Modules
│       ├── types/            # TypeScript Interface Definitions (strictly aligned with DTOs)
│       └── views/            # Dashboard, Devices, Monitoring, Edge AI, Automation, etc.
├── ml/                       # TinyML & Edge AI Pipeline
│   ├── datasets/             # Generated & Processed Agronomic CSV Datasets
│   ├── evaluation/           # Comprehensive ML Model Evaluation Scripts
│   ├── export/               # Model Artifacts (model.joblib) & C Transpiler Script
│   └── training/             # Synthetic Dataset Generator & Model Training Scripts
└── tests/                    # End-to-End Integration Tests & HIL Runner
    ├── hil/                  # Hardware-in-the-Loop Simulation & Serial Test Runner
    └── integration/          # 12-Phase E2E Flow & Contract Integration Tests
```

---

### 2.2. Trách nhiệm Từng Phân Hệ Module

#### A. Frontend (Vue 3, TypeScript, Vite)
- **Giao diện Giám sát & Điều khiển:** Hiển thị biểu đồ độ ẩm đất, nhiệt độ, độ ẩm không khí, trạng thái máy bơm, góc mở servo, và lớp dự đoán Edge AI theo thời gian thực.
- **Quản trị Thiết bị & Cấu hình:** Cho phép cập nhật chế độ điều khiển (AUTO, MANUAL, RULE, HYBRID, SAFE), gán thiết bị vào nhà kính/khu vực, và theo dõi trạng thái đồng bộ (`is_synced`).
- **Phân quyền người dùng (RBAC):** Admin, Operator, Viewer. Giao diện tự động ẩn các nút điều khiển đối với tài khoản Viewer.

#### B. Backend (FastAPI, SQLAlchemy, Pydantic v2)
- **Quản lý Vòng đời Lệnh Điều khiển (Command Lifecycle):** Tiếp nhận lệnh điều khiển, thực thi deduplication qua `idempotency_key`, gán TTL (thời gian sống), xử lý polling từ thiết bị, ghi nhận ACK và khóa trạng thái cuối (`completed`, `failed`, `rejected`, `expired`).
- **Cổng An toàn Phần mềm:** Áp dụng quy tắc cứng: Từ chối bất kỳ lệnh tưới nào yêu cầu thời gian chạy máy bơm $> 120$ giây.
- **Tiếp nhận Telemetry:** Validate dữ liệu theo schema `TelemetryIngest`, lọc các giá trị đo không hợp lệ hoặc dị thường cực đoan, kích hoạt tự động hóa (`AutomationEngine`) và cảnh báo (`Alert`).

#### C. Database (SQLite / SQLAlchemy)
- Lưu trữ các bảng: `users`, `greenhouses`, `zones`, `devices`, `device_configs`, `sensor_readings`, `device_commands`, `alerts`, `automation_rules`, `audit_logs`, `irrigation_schedules`.
- Đảm bảo tính nhất quán qua các chỉ mục duy nhất (`unique=True` cho `command_id`, index cho `idempotency_key`).

#### D. Firmware (ESP32-S3 FreeRTOS)
- **`main.c`:** Quản lý vòng đời FreeRTOS, khởi tạo hai tác vụ chính: `safety_sensor_task` (Priority 5, Core 0, chu kỳ 100ms) và `network_uplink_task` (Priority 3, Core 1, chu kỳ 500ms).
- **`safety_manager.c`:** **Cổng an toàn phần cứng (Safety Gate)**. Là chốt chặn duy nhất có quyền đóng/ngắt rơ-le máy bơm. Giám sát thời gian chạy tối đa (120s), thời gian cooldown (180s), khóa khẩn cấp (Emergency Lockout), kiểm tra nhiệt độ an toàn (0°C - 50°C), và ngắt bơm tức thì khi cảm biến báo lỗi.
- **`command_manager.c`:** Xử lý hàng đợi lệnh từ Cloud, kiểm tra TTL quá hạn (epoch time), phát hiện lệnh trùng bằng Ring Buffer 8 vị trí, điều phối thực thi và sinh gói tin ACK.
- **`sensor_manager.c` & `sensor_calibration.c`:** Giao tiếp phần cứng ADC 12-bit (chân 1, 2) và DHT11 (chân 4), chuyển đổi giá trị thô thành %, kiểm tra tính hợp lý điện học (loại bỏ trường hợp đứt dây hoặc chập mass).
- **`edge_ai.c` & `edge_ai_model.c`:** Chứa mô hình TinyML Decision Tree dạng C tĩnh không cấp phát động, suy luận trạng thái môi trường và đưa ra khuyến nghị hành động.
- **`firmware_config.c` & `project_config.h`:** Quản lý cấu hình tập trung, kiểm tra tính hợp lệ dữ liệu, nạp/lưu cấu hình NVS Flash.

#### E. Machine Learning Pipeline (Python Scikit-Learn -> C Transpiler)
- Chịu trách nhiệm sinh dữ liệu nông học, phân chia tập dữ liệu không rò rỉ, huấn luyện cây quyết định độ sâu $\le 6$, đánh giá nhiễu cảm biến, và biên dịch mô hình sang file C nhúng trực tiếp vào firmware.

---

## 3. Luồng Dữ liệu Đầu - Cuối (End-to-End Data Flow)

### 3.1. Luồng Thu thập Dữ liệu & Báo cáo Telemetry
```text
[Cảm biến Độ ẩm đất (GPIO1) & DHT11 (GPIO4)]
         │
         ▼
[Sensor Manager: Đọc xung/ADC thô -> Hiệu chuẩn % -> Kiểm tra plausibility]
         │
         ▼
[Edge AI Engine: Suy luận trạng thái -> Xác định Class 0..4 & Khuyến nghị]
         │
         ▼
[Safety Manager: Kiểm tra điều kiện an toàn môi trường]
         │
         ▼
[Telemetry Client: Đóng gói JSON theo TelemetryPayload Schema]
         │
         ▼
[Wi-Fi Manager & Network Client: Gửi HTTP POST /api/v1/telemetry]
         │
         ▼
[Backend FastAPI Telemetry Service: Validate Pydantic -> Ingest DB]
         │
         ▼
[Automation Engine & Alert Engine: Kiểm tra ngưỡng cảnh báo]
         │
         ▼
[Frontend Dashboard: Cập nhật chỉ số trực tiếp cho người vận hành]
```

### 3.2. Luồng Điều khiển Xuống (Downlink Command & ACK Lifecycle)
```text
[Người dùng bấm 'Tưới Nước' trên Frontend]
         │
         ▼
[Backend Command Service: Kiểm tra RBAC -> Kiểm tra Guardrail duration <= 120s]
         │
         ▼
[Tạo DeviceCommand: status='pending', expires_at = now + 300s, sinh idempotency_key]
         │
         ▼
[ESP32-S3 Network Task: Định kỳ 3s gọi HTTP GET /api/v1/devices/{id}/commands/poll]
         │
         ▼
[Backend: Quét lệnh hết hạn (sweep expired) -> Trả về danh sách lệnh pending]
         │
         ▼
[Firmware Command Manager:
   1. Kiểm tra TTL quá hạn (epoch_sec > expires_at_sec) -> Reject nếu hết hạn
   2. Kiểm tra Idempotency Ring Buffer -> Nếu trùng: trả ACK completed mà không thực thi lại
   3. Định tuyến lệnh: Gọi Safety Gate]
         │
         ▼
[Safety Gate (Safety Manager):
   - Kiểm tra Lockout / Cooldown / Sensor Health
   - Nếu duyệt: Bật GPIO 10 (Relay ON) trong đúng số giây yêu cầu]
         │
         ▼
[Firmware Network Client: Gửi HTTP POST /api/v1/devices/{id}/commands/ack]
         │
         ▼
[Backend: Cập nhật status='completed', completed_at=now, khóa bất biến]
         │
         ▼
[Frontend: Hiển thị thông báo hoàn thành lệnh thành công]
```

---

## 4. Các Vấn đề Được Phát hiện & Biện pháp Khắc phục

| STT | Vấn đề phát hiện | Nguyên nhân gốc rễ | Tác động hệ thống | File & Vị trí sửa đổi | Trạng thái sau sửa |
|---|---|---|---|---|---|
| **1** | Mâu thuẫn cực tính rơ-le trong tài liệu | Tài liệu cũ `known-limitations.md` ghi `0` (Active-Low) trong khi code thực tế là `1` (Active-High). | Gây hiểu nhầm cho kỹ sư lắp đặt, nguy cơ rơ-le bị đóng ngược nếu cấu hình nhầm. | [`project_config.h:108`](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/firmware/include/project_config.h#L108), [`docs/HARDWARE_WIRING_AND_GPIO.md`](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/docs/HARDWARE_WIRING_AND_GPIO.md) | **ĐÃ GIẢI QUYẾT** |
| **2** | Nguy cơ xung đột chân Octal PSRAM trên ESP32-S3 | Cần làm rõ việc sử dụng chân rơ-le giữa GPIO 10 và GPIO 16. | Chân 15-18 và 33-37 trên ESP32-S3 N16R8 nối với Octal PSRAM. Dùng GPIO 16 sẽ gây crash chip tức thì. | Bổ sung `_Static_assert` chặn GPIO 15-18 & 33-37 trong [`project_config.h:207`](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/firmware/include/project_config.h#L207). Xác minh GPIO 10 là chân chuẩn duy nhất. | **ĐÃ GIẢI QUYẾT** |
| **3** | Phân tán định nghĩa cấu hình firmware | Cấu hình nằm rải rác ở `config_defaults.h`, `board_config.h`, và các hằng số ma thuật (magic numbers) trong `sensor_calibration.c` và `actuator_manager.c`. | Khó bảo trì, lập trình viên có thể sửa một nơi nhưng quên nơi khác, gây mâu thuẫn compile-time. | Tạo [`project_config.h`](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/firmware/include/project_config.h) làm đầu mối duy nhất; refactor `actuator_manager.c`, `sensor_calibration.c`, `command_manager.c`. | **ĐÃ GIẢI QUYẾT** |
| **4** | Chân servo và buzzer bị gọi trực tiếp qua macro tĩnh | Trong `actuator_manager.c`, các hàm `set_servo` và `set_buzzer` dùng thẳng `PIN_SERVO_VENT` và `PIN_BUZZER` thay vì kiểm tra đối tượng cấu hình runtime. | Nếu người dùng đổi chân qua cấu hình, hàm điều khiển vẫn xuất ra chân cũ. | Sửa [`actuator_manager.c:64, 71`](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/firmware/src/actuator_manager.c#L64) lấy chân động từ `firmware_config_get()`. | **ĐÃ GIẢI QUYẾT** |
| **5** | Ngưỡng hiệu chuẩn ADC dùng số cứng (hardcoded literals) | Trong `sensor_calibration.c`, hàm `init` và `is_plausible` ghi cứng `3200`, `1200`, `3850`, `150`. | Sửa ngưỡng hiệu chuẩn trong header không làm thay đổi logic tính toán thực tế. | Refactor [`sensor_calibration.c:6-8, 55`](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/firmware/src/sensor_calibration.c#L6-L8) dùng trực tiếp các symbol từ `project_config.h`. | **ĐÃ GIẢI QUYẾT** |
| **6** | Nguy cơ lờ đi cấu hình cục bộ (`config_local.h`) | Tên macro trong `config_local.h` cũ không khớp với một số macro `DEFAULT_*` trong `config_defaults.h`. | Lập trình viên định nghĩa cấu hình lab nhưng firmware vẫn chạy cấu hình mặc định. | Đồng bộ hóa bí danh và định nghĩa thống nhất tại [`project_config.h`](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/firmware/include/project_config.h). | **ĐÃ GIẢI QUYẾT** |

---

## 5. Bảng Phân loại Toàn diện: PASS / FAIL / BLOCKED / UNVERIFIED

| Phân hệ / Hạng mục kiểm tra | Phương pháp kiểm tra | Tiêu chuẩn đánh giá | Bằng chứng thực tế | Phân loại |
|---|---|---|---|---|
| **Frontend: TypeScript Types & Schemas** | Static type checking (`vue-tsc`) | 0 type errors, khớp 100% Backend DTOs | `vue-tsc --noEmit` hoàn thành sạch sẽ | **PASS** |
| **Frontend: Production Webpack/Vite Build** | Production bundle compile (`vite build`) | Build thành công ra thư mục `dist/` | 115 modules transformed, hoàn thành trong 43.27s | **PASS** |
| **Backend: API Routes & CRUD Endpoints** | Pytest unit test suite | 100% tests đạt | 24 tests passed trong 16.77s | **PASS** |
| **Backend: Command Lifecycle, Idempotency & TTL** | Pytest integration test | Lệnh trùng không chạy lại; lệnh hết hạn bị reject | `test_security_resilience.py` & `test_audit_regressions.py` passed | **PASS** |
| **Backend: Irrigation Safety Guardrail** | Pytest guardrail test | Yêu cầu tưới $> 120$s bị từ chối với HTTP 400 | `test_global_pump_duration_safety_guardrail` passed | **PASS** |
| **Database: Schema Consistency & Indexes** | Static audit models & migrations | Khóa chính, chỉ mục, quan hệ cascade toàn vẹn | `entities.py` có unique index cho command_id | **PASS** |
| **Firmware: Native Firmware Unit Tests** | GCC native compilation & test runner | 100% các ca kiểm thử an toàn, NVS, rollover đạt | 287 / 287 tests passed (100.0%) | **PASS** |
| **Firmware: Safety Gate Cutoff & Cooldown** | Native simulation time-advance | Bơm ngắt đúng 120s; cooldown duy trì đúng 180s | `test_firmware_native.c` watchdog tests passed | **PASS** |
| **Firmware: Config Centralization & Validation** | Compile-time check & Static Assertions | Khởi tạo thành công; xung đột chân bị chặn lúc build | `project_config.h` static assertions passed | **PASS** |
| **ML: Dataset Pipeline & Stratified Split** | Python execution (`generate_dataset.py`) | 6,000 mẫu sinh thành công, đúng tỷ lệ nhãn | `ml/datasets/greenhouse_dataset.csv` đã tạo | **PASS** |
| **ML: Decision Tree Model Training** | Python execution (`train_model.py`) | Accuracy & Macro F1 đạt yêu cầu, không leakage | Accuracy: 100.00%, Macro F1: 100.00% | **PASS** |
| **ML: Noise Robustness Evaluation** | Gaussian noise injection ($\sigma = 1.5$) | Duy trì độ chính xác $> 95$% | Accuracy: 98.20%, Macro F1: 97.36% | **PASS** |
| **ML: Model Transpilation to C** | Python execution (`transpile_to_c.py`) | Sinh file C hợp lệ, zero heap allocation | `edge_ai_model.h` và `edge_ai_model.c` đã tạo | **PASS** |
| **ML: Python vs C Parity Verification** | Native C test runner (`test_ai_parity.c`) | 500 mẫu kiểm thử khớp hoàn toàn 100% | 500 / 500 matches (100.00% Parity Rate) | **PASS** |
| **Integration: 12-Phase Full Vertical E2E Flow** | End-to-end Python integration script | Tất cả 12 luồng logic từ login đến telemetry đạt | `test_end_to_end_flow.py` all 12 flows passed | **PASS** |
| **Integration: Firmware-Backend Contract** | Contract verification script | Khớp payload JSON, header, sequence, ACK | `test_firmware_backend_integration.py` passed | **PASS** |
| **Hardware: GPIO Allocation & Polarity** | Code tracing & ESP32-S3 TRM verification | Chân GPIO không xung đột PSRAM, cực tính đúng | GPIO 10 xác nhận; GPIO 15-18 & 33-37 đã loại trừ | **PASS** |
| **Hardware: Physical Electrical Glitch Test (Oscilloscope)** | Đo kiểm phần cứng thực tế (DMM / Scope) | Yêu cầu thiết bị vật lý thật để đo xung nguồn | Chưa thực hiện đo vật lý trong môi trường audit | **UNVERIFIED** |
| **Hardware: Agronomic Field Validation on Real Crops** | Trồng trọt thực tế qua mùa vụ sinh học | Đánh giá độ ẩm đất trên giá thể sống | Cần dữ liệu thực nghiệm sinh học nhiều mùa vụ | **UNVERIFIED** |

---

## 6. Lệnh Thực thi Kiểm thử & Kết quả Thực tế (Verification Evidence)

### 6.1. Kiểm thử Firmware Native Suite (287 Test Cases)
- **Lệnh thực thi:**
  ```powershell
  gcc -Wall -Wextra -I firmware/include firmware/src/sensor_calibration.c firmware/src/sensor_manager.c firmware/src/safety_manager.c firmware/src/actuator_manager.c firmware/src/command_manager.c firmware/src/telemetry_client.c firmware/src/wifi_manager.c firmware/src/network_client.c firmware/src/edge_ai_model.c firmware/src/edge_ai.c firmware/src/ota_manager.c firmware/src/firmware_config.c firmware/src/dht11.c firmware/test/test_firmware_native.c -o firmware/test/test_firmware_native.exe
  .\firmware\test\test_firmware_native.exe
  ```
- **Exit Code:** `0`
- **Kết quả:** `SUMMARY: 287 / 287 tests passed (100.0%)`

### 6.2. Kiểm thử Tính Nhất quán TinyML Parity Test (500 Samples)
- **Lệnh thực thi:**
  ```powershell
  gcc -Wall -Wextra -I firmware/include firmware/src/edge_ai_model.c firmware/test/test_ai_parity.c -o firmware/test/test_ai_parity.exe
  .\firmware\test\test_ai_parity.exe
  ```
- **Exit Code:** `0`
- **Kết quả:**
  ```text
  Total Samples Evaluated: 500
  Matches with Python DT : 500
  Parity Match Rate      : 100.00%
  Edge/Boundary Tests    : 4 / 4 passed
  ```

### 6.3. Kiểm thử Backend & Integration Pytest Suite (24 Test Cases)
- **Lệnh thực thi:**
  ```powershell
  $env:PYTHONPATH="."
  backend\.venv\Scripts\pytest backend/tests tests/integration -q
  ```
- **Exit Code:** `0`
- **Kết quả:** `24 passed, 1 warning in 29.36s`

### 6.4. Kiểm thử Frontend Build & Type Check
- **Lệnh thực thi (trong thư mục `frontend/`):**
  ```powershell
  npm run build
  ```
- **Exit Code:** `0`
- **Kết quả:** `✓ built in 43.27s` (115 modules transformed, 0 errors).

### 6.5. Kiểm thử Mô phỏng HIL Runner
- **Lệnh thực thi:**
  ```powershell
  $env:PYTHONPATH="."
  backend\.venv\Scripts\python tests/hil/hil_runner.py --sim-only
  ```
- **Exit Code:** `0`
- **Kết quả:**
  ```text
  HIL VERIFICATION SUMMARY:
  - Level A/B (Firmware & Safety Logic Sim): SIMULATION_PASS
  - Level C   (Physical Board Serial Test): SKIPPED (Simulation-only mode selected)
  ```

---

## 7. Đánh giá Mức độ Sẵn sàng Triển khai (Deployment Readiness Assessment)

### 7.1. Phần mềm, Backend, Firmware Logic & Edge AI: **SẴN SÀNG TRIỂN KHAI (SOFTWARE READY)**
- Kiến trúc phần mềm hoàn toàn đồng bộ giữa Frontend, Backend và Firmware.
- Cơ chế bảo vệ an toàn phần cứng (Safety Gate, Lockout, Cooldown, Runtime Watchdog) đã được chứng minh qua 287 ca kiểm thử mô phỏng khắt khe.
- Pipeline TinyML đạt độ tương thích 100% giữa Python và C, thực thi với 0 byte heap allocation.

### 7.2. Phần cứng Vật lý & Triển khai Nhà kính Thật: **YÊU CẦU TIỀN ĐIỀU KIỆN HIL (PRE-FLIGHT HIL REQUIRED)**
- Trước khi cấp điện cho máy bơm nước và kết nối với cây trồng thật, bắt buộc kỹ sư phần cứng phải thực hiện:
  1. Kiểm tra không đoản mạch nguồn 3.3V / 5V bằng DMM.
  2. Đo điện áp chân kích rơ-le GPIO 10 lúc khởi động bằng DMM (phải là 0.00V ± 0.05V ở cấu hình Active-High).
  3. Lắp đặt điện trở kéo xuống 10kΩ ngoại tại chân GPIO 10 để triệt tiêu hoàn toàn boot glitch phần cứng.
  4. Lắp đặt nguồn 5V riêng biệt cho rơ-le và servo (rút jumper JD-VCC) để tránh sụt áp vi điều khiển.
  5. Thu thập mẫu đất thực tế để hiệu chỉnh lại 2 điểm `PROJECT_SOIL_ADC_DRY_RAW` và `PROJECT_SOIL_ADC_WET_RAW`.
