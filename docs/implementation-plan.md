# SmartGrow AI — Implementation Plan & Phase Roadmap

## 1. Project Overview
SmartGrow AI is an intelligent greenhouse monitoring, edge AI, and irrigation management platform targeting the ESP32-S3 DevModule N16R8 (16MB Flash, 8MB PSRAM), backed by a Python FastAPI backend and a Vue 3 TypeScript frontend.

## 2. Environment & Repository Audit (Phase 0)
- **Repository state:** Clean workspace (`c:\Users\ADMIN\Downloads\smartgrow-ai`).
- **Host Environment:**
  - OS: Windows 10/11
  - Python: 3.11.0 (with pip and virtualenv support)
  - Node.js: v24.11.1 / npm 11.6.2
  - C/C++ Compiler: GCC 14.2.0 (x86_64-w64-mingw32 / ucrt64) for native testing and TinyML validation
  - Containerization: Docker Desktop available
  - Version Control: Git available

## 3. Architecture & Tech Stack
```
+-------------------------------------------------------------------------+
|                              SMARTGROW AI                               |
+-------------------------------------------------------------------------+
|                                                                         |
|  [ESP32-S3 Edge Controller]                                             |
|  - Sensors: Soil Moisture, DHT22 (Temp/Hum), LDR (Light)               |
|  - Actuators: Relay (Pump), Servo (Ventilation), Buzzer, Status Display |
|  - Edge AI: Decision Tree / Rule Engine running directly in C/C++       |
|  - Safety Interlocks: Auto-cutoff, Cooldown, Sensor Fault Protection    |
|  - Connectivity: Wi-Fi REST Client + Heartbeat + Local Offline Safety   |
|                                    |                                    |
|                         HTTP / REST Telemetry & ACK                     |
|                                    v                                    |
|  [FastAPI Backend Platform]                                             |
|  - REST API v1 (Auth, Devices, Telemetry, Edge AI, Automation, etc.)   |
|  - Database: SQLAlchemy ORM (SQLite for dev, PostgreSQL ready)          |
|  - Automation Engine: Sensor rules, cooldowns, threshold evaluation    |
|  - Command Lifecycle: Pending -> Acked -> Executing -> Completed/Failed |
|  - Background Monitor: Offline detector, command expiry, safety sync    |
|                                    |                                    |
|                         REST API / JSON Payloads                        |
|                                    v                                    |
|  [Vue 3 + TypeScript Frontend Management Web App]                       |
|  - Modern Industrial IoT Theme (Tailwind CSS, Glassmorphic Dashboard)   |
|  - Pinia Stores, Vue Router 4, Axios, ECharts                           |
|  - 12 Complete Management Modules (Dashboard, Live Monitor, Edge AI,   |
|    Irrigation, Automation Rules, Alerts, Devices, Greenhouses, etc.)    |
+-------------------------------------------------------------------------+
```

## 4. Phase-by-Phase Plan & Acceptance Criteria

### Phase 1: Skeleton & First End-to-End Vertical Flow
- [x] Workspace structure initialization (`firmware/`, `backend/`, `frontend/`, `ml/`, `tests/`, `docs/`, `deploy/`).
- [ ] Backend foundation: Python venv setup, FastAPI app, SQLite database setup, user auth, device registry, and telemetry ingestion endpoint.
- [ ] Firmware foundation: ESP32-S3 PlatformIO project, modular C architecture (`board_config`, `sensor_manager`, `actuator_manager`, `telemetry_client`, `command_manager`), and dual-target support (ESP32-S3 cross-compilation + native GCC test runner/simulator).
- [ ] Frontend foundation: Vue 3 + TypeScript + Vite + Tailwind CSS + Pinia + Vue Router + Axios API client.
- [ ] **Acceptance Test 1:** Execute complete telemetry flow: simulated/hardware ESP32-S3 sends telemetry -> backend validates & stores in DB -> frontend fetches and renders on dashboard -> frontend creates pump command -> backend registers command -> firmware polls/receives & ACKs -> status reflects on UI.

### Phase 2: Edge AI & Safety Manager
- [ ] ML pipeline: Dataset generator based on agronomic thresholds, scikit-learn decision tree training, precision/recall/F1 metrics evaluation, and automated C transpiler (`ml/export/edge_ai_model.h` and `.c`).
- [ ] Firmware Edge AI module: Feature normalization, inference execution in < 1ms, zero dynamic memory allocation, and model version reporting.
- [ ] Safety Manager: Maximum pump runtime watchdog, mandatory cooldown timer, sensor fault detection, offline fail-safe state.
- [ ] **Acceptance Test 2:** Native C unit test verifying 100% agreement between Python predictions and C inference, plus safety tests proving safety interlock overrides AI recommendations when limits are exceeded.

### Phase 3: Full Backend & Database Platform
- [ ] Comprehensive SQLAlchemy database schema & migrations.
- [ ] Complete API v1 routers:
  - `/api/v1/auth` (JWT login, profile, user management, roles)
  - `/api/v1/greenhouses` & `/api/v1/zones` (CRUD, threshold configs, device assignment)
  - `/api/v1/devices` (registration, heartbeat, configs, restart, command history)
  - `/api/v1/telemetry` (ingest, historical queries, aggregation, CSV export)
  - `/api/v1/edge-ai` (inference logging, model registry, baseline comparison)
  - `/api/v1/automation` (rule builder, AND/OR logic, cooldown, execution log)
  - `/api/v1/irrigation` (manual start/stop, safety duration limit, schedules, history)
  - `/api/v1/alerts` (severity levels, acknowledge, resolve, audit)
  - `/api/v1/analytics` (uptime stats, water/pump runtime stats, environmental summaries)
  - `/api/v1/system` (calibration, timezone, audit logs, backup info)
  - `/api/v1/ota` (firmware releases, binary upload, deployment tracking)
- [ ] Background worker tasks: offline device sweep, command expiry watchdog, automation engine trigger.
- [ ] **Acceptance Test 3:** Pytest suite testing all endpoints, error handling, permission checks, and database integrity.

### Phase 4: Frontend Management Web Application
- [ ] Implementation of 12 distinct functional modules:
  1. Dashboard
  2. Greenhouse & Zone Management
  3. Device Management
  4. Live Monitoring (ECharts time-series)
  5. Edge AI Center (Inference analysis & safety override inspection)
  6. Automation Rules Builder
  7. Irrigation Control (with safety modal & command feedback)
  8. Alerts & Events Center
  9. Schedule & Calendar
  10. Analytics & Reports
  11. System Settings & Sensor Calibration
  12. Users & Audit Logs
- [ ] UI Polish: Industrial IoT dark/light styling, responsive sidebar, loading skeletons, empty states, error alerts, badge indicators.
- [ ] **Acceptance Test 4:** Frontend production build verification (`npm run build`) and integration verification.

### Phase 5: OTA, CI/CD, and Hardware-in-the-Loop (HIL)
- [ ] ESP32-S3 OTA firmware update manager (partition configuration, checksum verification, rollback support).
- [ ] GitHub Actions CI workflow (`.github/workflows/ci.yml`) covering backend tests, frontend build, firmware compilation, and native tests.
- [ ] HIL test runner framework (`tests/hil/hil_runner.py`) with serial connection handler and software simulation fallback.
- [ ] **Acceptance Test 5:** Execute HIL simulation verifying simulated boot, telemetry push, command reception, and safety cutoff.

### Phase 6: Hardening, Integration Testing & Deliverables
- [ ] End-to-end integration test runner validating end-to-end workflows.
- [ ] Docker Compose setup (`deploy/docker-compose.yml`) for one-command containerized launch.
- [ ] Production environment file (`.env.example`).
- [ ] Complete documentation:
  - `README.md`
  - `docs/architecture.md`
  - `docs/api.md`
  - `docs/hardware.md`
  - `docs/ai-model.md`
  - `docs/testing.md`
  - `docs/deployment.md`
- [ ] Final acceptance audit and handover report.
