# SmartGrow AI — Intelligent Greenhouse Monitoring, Edge AI & Irrigation Platform

[![CI Pipeline](https://github.com/smartgrow-ai/smartgrow-ai/actions/workflows/ci.yml/badge.svg)](.github/workflows/ci.yml)
[![FastAPI](https://img.shields.io/badge/Backend-FastAPI_0.119-009688.svg?logo=fastapi)](https://fastapi.tiangolo.com)
[![Vue 3](https://img.shields.io/badge/Frontend-Vue_3_TypeScript-4FC08D.svg?logo=vuedotjs)](https://vuejs.org)
[![ESP32-S3](https://img.shields.io/badge/Edge_MCU-ESP32--S3_N16R8-E7352C.svg?logo=espressif)](https://www.espressif.com)
[![TinyML](https://img.shields.io/badge/Edge_AI-Decision_Tree_C_Export-blue.svg)](docs/ai-model.md)

SmartGrow AI is an enterprise-grade IoT platform designed for intelligent greenhouse monitoring, on-chip Edge AI inference, and autonomous irrigation management targeting the **ESP32-S3 DevModule N16R8 (16MB Flash, 8MB PSRAM)** controller.

---

## 🌟 Key Architecture & Capabilities

```
+-------------------------------------------------------------------------+
|                              SMARTGROW AI                               |
+-------------------------------------------------------------------------+
|  [ESP32-S3 Edge Controller]                                             |
|  - Soil Moisture (GPIO 1), LDR (GPIO 2), DHT11 (GPIO 4)                 |
|  - Pump Relay (GPIO 10, Configurable Polarity), SG90 Vent Servo (GPIO 11)|
|  - Edge AI: Decision Tree running in zero-heap deterministic C (5µs)    |
|  - Safety Interlocks: Max pump runtime (120s), Min cooldown (180s)      |
|                                    |                                    |
|                       HTTP Telemetry & Commands                         |
|                                    v                                    |
|  [FastAPI Backend Platform]                                             |
|  - Versioned API: /api/v1 (Auth, Devices, Telemetry, Automation, etc.)  |
|  - Relational Database: SQLAlchemy ORM (SQLite / PostgreSQL ready)      |
|  - Automation Engine & Command Lifecycle (Pending -> Acked -> Completed)|
|                                    |                                    |
|                       REST API / JSON Payloads                          |
|                                    v                                    |
|  [Vue 3 TypeScript Management Platform]                                 |
|  - Industrial IoT Theme: High-observability dark UI (Tailwind CSS)      |
|  - 12 Full Management Modules: Dashboard, Live Monitoring, Greenhouses, |
|    Devices, Edge AI Center, Automation Rules, Irrigation, Alerts, etc.  |
+-------------------------------------------------------------------------+
```

1. **Embedded Autonomy & Hard Safety Interlocks:**
   - The ESP32-S3 enforces real-time hardware safety cutoffs locally on-chip. Even during a complete cloud outage, the water pump is automatically disengaged if continuous runtime exceeds 120 seconds or if sensor faults are detected.
2. **Deterministic On-Chip Edge AI:**
   - Evaluates soil moisture, temperature, humidity, and sunlight on the microcontroller in **14 microseconds** using **0 bytes of heap memory** (`malloc` free).
   - 100% decision parity verified between the Python scikit-learn model and the embedded C inference code across 500 test vectors.
3. **Robust Command Lifecycle:**
   - Commands have explicit TTLs, idempotency protection against duplicate triggers, and require physical device execution acknowledgments (ACK).
4. **Full-Featured 12-Module Industrial Web Console:**
   - Includes Facilities & Zones CRUD, Device Fleet Management, Live Multi-Metric Telemetry, Edge AI Center, Automation Rules Builder, Irrigation Command Center, Alerts Management, Recurring Schedules, Analytics, and Audit Trails.

---

## 🚀 Quickstart Guide

### 1. Prerequisites
- Python 3.11+
- Node.js 20+ / npm 10+
- GCC (MinGW / MSYS2 ucrt64 on Windows, or GCC on Linux)

### 2. Launch FastAPI Backend
```powershell
# Create and activate virtual environment
py -3.11 -m venv backend/.venv
.\backend\.venv\Scripts\Activate.ps1

# Install dependencies
pip install -r backend/requirements.txt

# Start backend server
uvicorn backend.app.main:app --reload --host 127.0.0.1 --port 8000
```
- Interactive Swagger API Documentation: `http://127.0.0.1:8000/api/docs`
- Default Admin Credentials: `admin@smartgrow.local` / `Admin@SmartGrow2026!`

### 3. Launch Vue 3 Frontend
```powershell
cd frontend
npm install
npm run dev
```
- Open your browser at `http://localhost:5173`

### 4. Docker Compose Deployment
To launch the complete containerized stack in one command:
```powershell
docker compose -f deploy/docker-compose.yml up --build -d
```
- Persistent SQLite database is mounted inside Docker named volume `smartgrow_data` at `/app/data/smartgrow.db`.
- Backup snapshot safely exported on host at `backups/smartgrow_pre_migration_20261027.db`.

---

## 🧪 Comprehensive Test Suite

SmartGrow AI includes automated tests covering firmware, machine learning, backend APIs, and end-to-end integration:

| Test Suite | Command | What It Validates | Result |
| :--- | :--- | :--- | :--- |
| **Firmware Native Tests** | `.\firmware\test\test_firmware_native.exe` | Sensor calibration, safety watchdog, cooldown, command parsing, telemetry serialization, NVS rollback, latch | **287/287 PASS (100.0%)** |
| **Edge AI Parity Test** | `.\firmware\test\test_ai_parity.exe` | 500 test vectors verified between Python scikit-learn and C transpiled code | **500/500 PASS (100.0%)** |
| **Backend Integration & Unit**| `pytest /app/backend/tests -v` (Docker) | Auth, device enrollment, telemetry schema validation, command lifecycle, automation rules, security resilience | **22/22 PASS (100.0%)** |
| **Vertical End-to-End** | `pytest /app/tests -v` (Docker) | Sensor -> Firmware -> Backend -> Database -> Frontend -> Command -> ACK vertical slice | **2/2 PASS (100.0%)** |
| **Frontend Production Build** | `npm run build` (Vue 3 / TS) | Vue 3 SFC compilation, TypeScript strict type-checking (`vue-tsc --noEmit`), Vite production bundling | **115/115 modules PASS** |
| **ML Model Evaluation** | `python /app/ml/evaluation/evaluate_model.py`| Clean test set evaluation (100% accuracy) and sensor jitter robustness testing (98.20% accuracy) | **PASS** |

---

## 📁 Repository Directory Structure

```text
smartgrow-ai/
├── firmware/
│   ├── include/                # Hardware pinouts (board_config.h), safety, AI headers
│   ├── src/                    # C implementation files (calibration, safety, edge_ai, commands)
│   ├── test/                   # Native C test runners (test_firmware_native.c, test_ai_parity.c)
│   ├── platformio.ini          # ESP32-S3 DevModule N16R8 configuration
│   └── partitions_16mb.csv     # Dual-OTA 16MB flash partition table
├── backend/
│   ├── app/
│   │   ├── api/v1/             # 12 REST API v1 routers (auth, devices, telemetry, etc.)
│   │   ├── core/               # Pydantic settings, SQLAlchemy DB, JWT security
│   │   ├── models/             # Relational database models (User, Device, SensorReading, etc.)
│   │   ├── schemas/            # Request/Response Pydantic DTOs & validation
│   │   ├── services/           # Business logic, automation engine, command lifecycle
│   │   └── main.py             # FastAPI application entry point & background worker
│   ├── tests/                  # Pytest integration test suite (22 unit & regression tests)
│   ├── requirements.txt        # Backend dependencies
│   └── Dockerfile              # Production container build
├── frontend/
│   ├── src/
│   │   ├── api/                # Axios API client & token interceptors
│   │   ├── layouts/            # Industrial IoT AppLayout with sidebar & header
│   │   ├── router/             # Vue Router connecting all 12 modules
│   │   ├── stores/             # Pinia auth store
│   │   ├── types/              # Comprehensive TypeScript interfaces
│   │   └── views/              # 12 Vue management views + LoginView
│   ├── package.json            # Frontend dependencies
│   └── vite.config.ts          # Vite build configuration & API proxy
├── ml/
│   ├── datasets/               # Synthesized agronomic dataset
│   ├── training/               # Dataset generator & Decision Tree training script
│   ├── export/                 # Trained model & automated C transpiler
│   └── evaluation/             # Model evaluation & noise robustness test script
├── tests/
│   ├── integration/            # Full vertical end-to-end integration tests
│   └── hil/                    # Hardware-in-the-Loop test runner
├── deploy/
│   └── docker-compose.yml      # Container orchestration
├── docs/                       # Comprehensive System Documentation & Re-Audit Guides
├── .github/workflows/          # GitHub Actions CI pipeline
├── .env.example                # Environment variable configuration template
└── README.md
```

---

## 📚 Technical Documentation Index

### Re-Audit & Operational Guides (2026 Audit)
- **[Final System Audit Report](docs/FINAL_SYSTEM_AUDIT_REPORT.md):** Báo cáo kiểm toán cuối cùng: Khắc phục lỗi lệch giờ (Timezone UTC+7), sửa nút Export CSV, và chứng minh tính bền vững của Database qua vòng đời container.
- **[Full Project Re-Audit Report](docs/FULL_PROJECT_REAUDIT.md):** Báo cáo tổng kiểm toán toàn diện hệ thống với bằng chứng thực tế từ runtime, test suite và mã nguồn.
- **[Database Runtime & Data Flow](docs/DATABASE_RUNTIME_AND_DATA_FLOW.md):** Xác định chính xác vị trí SQLite runtime trong container, bảng biểu, ERD, và phân tích rủi ro persistence.
- **[Database Access & Editing Guide](docs/DATABASE_ACCESS_AND_EDITING_GUIDE.md):** Hướng dẫn mở, xem dữ liệu bằng DB Browser for SQLite, truy vấn CLI và sửa đổi dữ liệu an toàn.
- **[Frontend Code & UI Audit](docs/FRONTEND_CODE_AND_UI_AUDIT.md):** Kiểm toán 12 views Vue 3, Pinia auth store, Axios interceptors, Vite proxy và khả năng chịu lỗi.
- **[Frontend Run & Test Guide](docs/FRONTEND_RUN_AND_TEST_GUIDE.md):** Hướng dẫn khởi chạy frontend trên Windows qua Docker hoặc host, kèm checklist kiểm thử UI.
- **[End-to-End Execution Guide](docs/END_TO_END_EXECUTION_GUIDE.md):** Truy vết chi tiết 3 luồng tích hợp end-to-end: Auth, Telemetry Ingestion và Điều khiển tưới an toàn.
- **[Complete Codebase Functional Guide](docs/COMPLETE_CODEBASE_FUNCTIONAL_GUIDE.md):** Cẩm nang giải thích từng module, API endpoint, hàm nghiệp vụ và chuỗi gọi hàm của 5 phân hệ.
- **[Docker Runtime & Debug Guide](docs/DOCKER_RUNTIME_AND_DEBUG_GUIDE.md):** Hướng dẫn Docker Compose, mạng container, phân giải DNS nội bộ, xem log và cứu hộ container.
- **[Master Documentation Index](docs/PROJECT_DOCUMENTATION_INDEX.md):** Bản đồ tra cứu toàn bộ tài liệu trong dự án.

### Hardware, Embedded & Machine Learning Reference
- **[Hardware Wiring, GPIO Pinout & Installation](docs/HARDWARE_WIRING_AND_GPIO.md):** Sơ đồ chân GPIO ESP32-S3, đấu nối cảm biến đất, DHT11, LDR và relay máy bơm.
- **[Firmware Configuration Guide](docs/FIRMWARE_CONFIGURATION_GUIDE.md):** Cấu hình FreeRTOS tasks, Wi-Fi, NVS fallback, Safety Gate 3 lớp và lệnh command poller.
- **[ML Edge AI Deep Dive](docs/ML_EDGE_AI_DEEP_DIVE.md):** Phân tích chuyên sâu kiến trúc Decision Tree C transpiled, ma trận nhầm lẫn và kiểm thử nhiễu.
- **[System Configuration Reference](docs/CONFIGURATION_REFERENCE.md):** Bảng tổng hợp toàn bộ thông số cấu hình của hệ thống.

---

## 🔒 Security & Reliability Guarantee
- **Zero Plaintext Passwords:** Passwords hashed with bcrypt.
- **Fail-Safe Offline Policy:** Pump defaults to OFF upon boot and cannot run indefinitely if network disconnects.
- **No Floating Hardcoded Secrets:** Configuration loaded from environment variables and validated via Pydantic.
