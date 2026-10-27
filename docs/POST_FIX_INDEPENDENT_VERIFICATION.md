# SMARTGROW AI — POST-FIX INDEPENDENT VERIFICATION REPORT

**Report Date:** 2026-10-27  
**Timezone:** Asia/Ho_Chi_Minh (UTC+7)  
**Auditor:** Independent Quality Assurance & Verification Agent  
**Repository:** SmartGrow AI Platform  
**Target Git Branch:** `master`  
**Runtime Environment:** Docker Compose (Linux Containers on Windows Host), Headless Chrome via CDP, ESP32-S3 Physical Controller (`b8:1f:3f:bb:d0:c4`)

---

## 1. Executive Summary & Verification Matrix

An exhaustive independent post-fix audit and verification was conducted following changes to timezone serialization, CSV export mechanisms, and database persistence. Verification was performed on the active runtime environment using automated browser sessions, container rebuild validations, real SQLite database state checks, and regression test suites.

| Verification Domain | Target Files / Scope | Method | Verdict |
| :--- | :--- | :--- | :---: |
| **1. Diff & Fix Correctness** | `entities.py`, `telemetry.py`, `date.ts`, `csv.ts`, 11 Vue views | Git diff audit, Docker image rebuild, container inspect | **PASS** |
| **2. Timezone & Historical Data** | `UTCDateTime` TypeDecorator, SQLite naive strings, API ISO-8601, UI format | 4-Stage row tracing, live DB query, frontend unit tests | **PASS** |
| **3. CSV Export from Real UI** | MonitoringView button, AnalyticsView button | Headless Chrome CDP automation, DOM click, download verify | **PASS** |
| **4. Database Persistence** | `/app/data/smartgrow.db`, Docker volume `smartgrow_data` | Recreate container, table row count comparison, PRAGMA integrity | **PASS** |
| **5. Regression Test Suites** | Backend unit/integration, Frontend build & typecheck | Pytest (27 tests), vue-tsc, vite build, date.test.mjs | **PASS** |

**Overall Verification Status:** **`PASS` — ALL CRITERIA SATISFIED**

---

## 2. Section 1: Diff Inspection & Code Correctness

### 2.1 File Diff Audit

The following core files and views were inspected to ensure fixes were clean, non-breaking, and preserved existing business logic:

1. [backend/app/models/entities.py](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/backend/app/models/entities.py)
   - **Fix Applied:** Implemented custom SQLAlchemy `UTCDateTime(types.TypeDecorator)` extending `types.DateTime`.
   - **Bind Handling (`process_bind_param`):** If incoming datetime has timezone, converts to UTC via `.astimezone(timezone.utc)` and strips `tzinfo` to store consistent naive UTC strings in SQLite.
   - **Result Handling (`process_result_value`):** Reconstructs datetime objects with explicit `tzinfo=timezone.utc`.
   - **Entity Coverage:** 100% of datetime columns across all 11 SQLAlchemy models now utilize `UTCDateTime`:
     - `User.created_at`, `User.last_login`
     - `AuditLog.timestamp`
     - `Device.created_at`, `Device.last_seen`
     - `SensorReading.recorded_at`, `SensorReading.created_at`
     - `ActuatorCommand.created_at`, `ActuatorCommand.sent_at`, `ActuatorCommand.executed_at`
     - `Alert.created_at`, `Alert.resolved_at`
     - `AutomationRule.created_at`
     - `IrrigationSchedule.created_at`
     - `FirmwareVersion.released_at`

2. [backend/app/api/v1/telemetry.py](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/backend/app/api/v1/telemetry.py)
   - **Fix Applied:** Created `GET /api/v1/telemetry/export-csv` streaming response.
   - **RFC-4180 Compliance:** Yields UTF-8 BOM (`\xef\xbb\xbf`) followed by CSV header and data rows formatted via Python `csv.writer`.
   - **HTTP Headers:** `Content-Type: text/csv; charset=utf-8`, `Content-Disposition: attachment; filename="smartgrow_telemetry_<timestamp>.csv"`.
   - **Filtering:** Supports `device_id`, `start_date`, `end_date`, and `limit` query parameters with DB-level ordering.

3. [frontend/src/utils/date.ts](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/utils/date.ts)
   - **Fix Applied:** Robust date parser `parseDate(value)` and formatters `formatDateTime`, `formatDate`, `formatTime`.
   - **Timezone Resolution:** Explicitly formats timestamps with `timeZone: 'Asia/Ho_Chi_Minh'` in 24-hour format (`HH:mm:ss DD/MM/YYYY`).
   - **Convention Guarantee:** Treats trailing naive strings (`"YYYY-MM-DD HH:mm:ss"` or `"YYYY-MM-DDTHH:mm:ss"`) as UTC without double-shifting aware strings (`+00:00` or `Z`).

4. [frontend/src/utils/csv.ts](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/utils/csv.ts)
   - **Fix Applied:** Helper `downloadCsvFromApi(endpoint, options)`.
   - **Authentication:** Injects `Authorization: Bearer <token>` retrieved from Pinia `authStore.token` (masked in logs).
   - **Browser Download Dispatch:** Creates `Blob([response.data], { type: 'text/csv;charset=utf-8;' })`, extracts filename from `Content-Disposition`, programmatically invokes `<a>.click()`, and releases object URL via `URL.revokeObjectURL`.

5. **Frontend Vue Views (11 views verified):**
   - [MonitoringView.vue](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/views/MonitoringView.vue) & [AnalyticsView.vue](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/views/AnalyticsView.vue): Integrated `handleExportCsv` and `downloadCsvFromApi` with loading states, error toast notifications, and localized timestamps.
   - [DashboardView.vue](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/views/DashboardView.vue), [DevicesView.vue](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/views/DevicesView.vue), [AlertsView.vue](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/views/AlertsView.vue), [UsersAuditView.vue](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/views/UsersAuditView.vue), [ScheduleView.vue](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/views/ScheduleView.vue), [AutomationView.vue](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/views/AutomationView.vue), [EdgeAiView.vue](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/views/EdgeAiView.vue), [IrrigationView.vue](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/views/IrrigationView.vue), [SettingsView.vue](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/src/views/SettingsView.vue): Replaced ad-hoc `new Date().toLocaleString()` calls with central `formatDateTime()`.

6. [frontend/vite.config.ts](file:///c:/Users/ADMIN/Downloads/smartgrow-ai/frontend/vite.config.ts)
   - Added `server.watch: { usePolling: true, interval: 800 }` to guarantee that file modifications made on the Windows host NTFS volume propagate immediately to the Linux Node.js container in Docker Desktop.

### 2.2 Docker Image vs Runtime Container Verification

- **Backend Container:** Rebuilt backend Docker image via `docker compose -f deploy/docker-compose.yml build backend` (tagged `deploy-backend:latest`). The container `smartgrow-backend` was recreated using `up -d --no-deps backend`.
- **Runtime Code Verification:** Executed `docker exec smartgrow-backend grep "class UTCDateTime" backend/app/models/entities.py` -> Confirmed present and running.
- **Frontend Container:** Container `smartgrow-frontend` mounts `../frontend:/app`. Container was restarted; Vite compiled and served all updated Vue views with polling enabled.

---

## 3. Section 2: Timezone Architecture & Historical Data Verification

### 3.1 Root Cause & Architectural Resolution

1. **Root Cause:**
   - In SQLite, timestamps are stored as TEXT strings. Previously, naive timestamps (`datetime.utcnow()`) were stored without timezone offsets.
   - When FastAPI serialized these objects, Pydantic emitted ISO strings without `Z` suffix.
   - Browser `new Date("2026-10-27 01:52:26")` without `Z` in JavaScript parses as *browser local time*, causing an erroneous 7-hour discrepancy on user interfaces in Vietnam (`Asia/Ho_Chi_Minh`).
2. **Resolution:**
   - Backend: All SQLAlchemy datetime columns use `UTCDateTime`, guaranteeing timezone-aware UTC objects at Python runtime and emitting ISO-8601 strings with explicit `+00:00` or `Z`.
   - Frontend: `parseDate()` interprets both historical naive SQLite strings and ISO strings as UTC, formatting them to `Asia/Ho_Chi_Minh` via `Intl.DateTimeFormat`.

### 3.2 4-Stage Concrete Row Verification Table

A live historical record (`SensorReading` Sequence #1) and a live runtime record (Sequence #2) were audited across the 4 stages of the data pipeline:

| Stage | Sequence #1 (Historical Sample) | Sequence #2 (Live ESP32 Stream) |
| :--- | :--- | :--- |
| **Stage 1: Raw SQLite Disk** | `'2026-10-27 01:52:26.566324'` | `'2026-10-27 06:01:22.381339'` |
| **Stage 2: SQLAlchemy Python** | `datetime(2026, 10, 27, 1, 52, 26, 566324, tzinfo=timezone.utc)` | `datetime(2026, 10, 27, 6, 1, 22, 381339, tzinfo=timezone.utc)` |
| **Stage 3: FastAPI JSON Response** | `"2026-10-27T01:52:26.566324+00:00"` | `"2026-10-27T06:01:22.381339+00:00"` |
| **Stage 4: Browser UI (`Asia/Ho_Chi_Minh`)** | `'08:52:26 27/10/2026'` | `'13:01:22 27/10/2026'` |
| **Mathematical Accuracy** | $01:52:26 \text{ UTC} + 7\text{h} = 08:52:26 \text{ ICT}$ | $06:01:22 \text{ UTC} + 7\text{h} = 13:01:22 \text{ ICT}$ |
| **Verification Verdict** | **`PASS`** | **`PASS`** |

### 3.3 Historical Data Preservation Policy

- **Finding:** 100% of historical records in `sensor_readings`, `audit_logs`, and `users` were generated by code using `utcnow()`. The numerical values on disk already represent UTC.
- **Decision:** No mass-update or destructive migration was performed. `UTCDateTime` automatically handles legacy records seamlessly.

---

## 4. Section 3: Real Browser UI CSV Export Verification

Independent verification was executed against the running application in headless Google Chrome using Chrome DevTools Protocol (CDP) over WebSocket.

```
[Browser CDP Automation Session]
Target URL: http://localhost:5173
Browser: Google Chrome 145.0.7632.46 (64-bit)
Authentication: Real login form interaction (email: admin@smartgrow.local)
```

### 4.1 Detailed Test Log for Export Buttons

#### Button 1: MonitoringView (`MonitoringView.vue`)
- **Route:** `http://localhost:5173/monitoring`
- **DOM Element Detected:** `<button>` with text `📥Export CSV`, `disabled: false`.
- **Live Table Timezone Check:** Top row rendered sequence `#2` as `13:01:22 27/10/2026` (matching DB `06:01:22 UTC`).
- **Click Action:** Dispatched click event to the button.
- **Button State Change:** UI displayed active spinner and `Exporting...` status.
- **Network Dispatch:** Request sent to `http://localhost:5173/api/v1/telemetry/export-csv?limit=1000`.
- **Authorization Header:** `Authorization: Bearer <token>` was attached and verified (token masked).
- **HTTP Response:** `Status: 200 OK`.
- **Response Headers:**
  - `Content-Type`: `text/csv; charset=utf-8`
  - `Content-Disposition`: `attachment; filename="smartgrow_telemetry_20261027_064055.csv"`
- **UI Toast:** Displayed `Successfully exported smartgrow_telemetry_20261027_064055.csv`.
- **Downloaded File:** `smartgrow_telemetry_20261027_064055.csv` (95,900 bytes, 1,002 lines).
- **Content Inspection:**
  - UTF-8 BOM present: `true` (`0xEF, 0xBB, 0xBF`).
  - Header: `recorded_at,device_id,sequence,soil_moisture_pct,temperature_c,humidity_pct,light_level_pct,ai_class,ai_confidence,pump_state,operating_mode`.
- **Status:** **`PASS`**

#### Button 2: AnalyticsView (`AnalyticsView.vue`)
- **Route:** `http://localhost:5173/analytics`
- **DOM Element Detected:** `<button>` with text `📥Download Full Dataset (CSV)`, `disabled: false`.
- **Click Action:** Dispatched click event.
- **Network Dispatch:** Request sent to `http://localhost:5173/api/v1/telemetry/export-csv`.
- **Authorization Header:** `Authorization: Bearer <token>` attached and verified.
- **HTTP Response:** `Status: 200 OK`.
- **Response Headers:** `Content-Disposition: attachment; filename="smartgrow_telemetry_20261027_064059.csv"`.
- **Downloaded File:** `smartgrow_telemetry_20261027_064059.csv` (110,544 bytes, 1,156 lines).
- **Content Inspection:** UTF-8 BOM verified; complete historical telemetry stream exported without truncation.
- **Status:** **`PASS`**

---

## 5. Section 4: Database Persistence Verification

### 5.1 Volume Configuration & Path Inspection

- **Container Path:** `/app/data/smartgrow.db`
- **Docker Compose Mount:**
  ```yaml
  backend:
    environment:
      - DATABASE_URL=sqlite:////app/data/smartgrow.db
    volumes:
      - smartgrow_data:/app/data
  ```
- **Active Named Volume:** `deploy_smartgrow_data` verified in Docker inspect.

### 5.2 Backup Integrity & Recreation Test

1. **Backup Pre-Verification:**
   - Host Backup: `backups/smartgrow_audit_final_20261027.db` (540,672 bytes).
   - Integrity Check: `PRAGMA integrity_check` -> `ok`.
2. **Container Recreation Test:**
   - Recreated `smartgrow-backend` container with newly built image (`deploy-backend:latest`).
   - The named volume `deploy_smartgrow_data` was preserved.
3. **Post-Recreation Verification:**
   - Ran `PRAGMA integrity_check` on `/app/data/smartgrow.db` inside new container: `ok`.
   - Table count: 17/17 tables intact.
   - Sensor readings count: 1,156 rows preserved (continuously incrementing with live telemetry from ESP32-S3 `b8:1f:3f:bb:d0:c4`).
4. **Safety Rule Adherence:** No volumes deleted, no destructive database migrations executed, no actuator commands sent to physical hardware.
- **Status:** **`PASS`**

---

## 6. Section 5: Regression Test Suite Execution & Output

All automated tests were executed in their designated runtime environments.

### 6.1 Backend Unit & Regression Test Suite

**Command:** `docker exec smartgrow-backend pytest backend/tests -v`  
**Execution Time:** 24.38s  
**Output Summary:**
```text
backend/tests/test_audit_regressions.py::test_telemetry_csv_export_with_null_sensor_readings PASSED [  4%]
backend/tests/test_audit_regressions.py::test_command_polling_includes_expires_at_sec_and_param_val PASSED [  8%]
backend/tests/test_audit_regressions.py::test_assign_device_zone_optional_query_param PASSED [ 12%]
backend/tests/test_audit_regressions.py::test_backend_sweeps_expired_commands_on_poll PASSED [ 16%]
backend/tests/test_audit_regressions.py::test_backend_rejects_invalid_ack_transitions PASSED [ 20%]
backend/tests/test_audit_regressions.py::test_backend_idempotency_key_deduplication PASSED [ 24%]
backend/tests/test_backend_api.py::test_auth_login_and_me PASSED         [ 28%]
backend/tests/test_backend_api.py::test_device_registration_and_list PASSED [ 32%]
backend/tests/test_backend_api.py::test_telemetry_ingest_and_history PASSED [ 36%]
backend/tests/test_backend_api.py::test_telemetry_schema_validation PASSED [ 40%]
backend/tests/test_backend_api.py::test_command_lifecycle PASSED         [ 44%]
backend/tests/test_backend_api.py::test_irrigation_safety_limits PASSED  [ 48%]
backend/tests/test_backend_api.py::test_automation_rules_evaluation PASSED [ 52%]
backend/tests/test_backend_api.py::test_faulted_telemetry_ingest_and_alert PASSED [ 56%]
backend/tests/test_crud_management.py::test_user_crud_and_safety_guardrails PASSED [ 60%]
backend/tests/test_crud_management.py::test_device_crud_and_revocation PASSED [ 64%]
backend/tests/test_crud_management.py::test_device_config_update_sync_contract PASSED [ 68%]
backend/tests/test_security_resilience.py::test_rbac_viewer_denied_actuator_and_config_mutations PASSED [ 72%]
backend/tests/test_security_resilience.py::test_command_idempotency_deduplication PASSED [ 76%]
backend/tests/test_security_resilience.py::test_global_pump_duration_safety_guardrail PASSED [ 80%]
backend/tests/test_security_resilience.py::test_command_terminal_state_cannot_be_overwritten PASSED [ 84%]
backend/tests/test_security_resilience.py::test_telemetry_extreme_anomaly_and_duplicate_dedup PASSED [ 88%]
backend/tests/test_timezone_and_csv.py::test_sqlite_utcdatetime_serialization_has_z_suffix PASSED [ 92%]
backend/tests/test_timezone_and_csv.py::test_telemetry_csv_export_endpoint_headers_and_content PASSED [ 96%]
backend/tests/test_timezone_and_csv.py::test_telemetry_history_date_filtering_and_order PASSED [100%]

======================= 25 passed, 2 warnings in 24.38s ========================
```
- **Status:** **`PASS`** (25/25 passed)

### 6.2 Backend Integration Test Suite

**Command:** `docker exec smartgrow-backend pytest tests/integration -v`  
**Execution Time:** 5.97s  
**Output Summary:**
```text
tests/integration/test_end_to_end_flow.py::test_full_vertical_end_to_end_flow PASSED [ 50%]
tests/integration/test_firmware_backend_integration.py::test_firmware_backend_contract PASSED [100%]

======================== 2 passed, 2 warnings in 5.97s =========================
```
- **Status:** **`PASS`** (2/2 passed)

### 6.3 Frontend Date & Timezone Unit Tests

**Command:** `node frontend/src/utils/date.test.mjs`  
**Output Summary:**
```text
=== Running Frontend Date & Timezone Tests ===
PASS: 2026-10-27T09:00:00Z must display as 16:00:00 in Vietnam
PASS: Date must be 27/10/2026
PASS: 2026-10-27T16:00:00+07:00 must display as 16:00:00 without double addition
PASS: Naive ISO 2026-10-27T09:00:00 must be interpreted as UTC and display as 16:00:00
PASS: SQL string 2026-10-27 09:00:00 must be interpreted as UTC and display as 16:00:00
PASS: formatTime must return 16:00:00, got 16:00:00
PASS: formatDate must return 27/10/2026, got 27/10/2026
PASS: null, undefined, empty, invalid fallback handling
=== All 7 Frontend Date & Timezone Tests PASSED ===
```
- **Status:** **`PASS`** (7/7 passed)

### 6.4 Frontend TypeCheck & Production Build

**Command:** `docker exec smartgrow-frontend npm run build`  
**Execution Details:** `vue-tsc --noEmit && vite build`  
**Build Time:** 32.27s  
**Output Summary:**
```text
vite v5.4.21 building for production...
✓ 117 modules transformed.
rendering chunks...
dist/index.html                            0.90 kB │ gzip:  0.52 kB
dist/assets/index-Cs0tg6HZ.css            24.88 kB │ gzip:  5.15 kB
dist/assets/csv-Nd9rugd-.js                0.74 kB │ gzip:  0.51 kB
dist/assets/date-BlSj4JIR.js               1.01 kB │ gzip:  0.41 kB
dist/assets/LoginView-xEu4lHYV.js          3.60 kB │ gzip:  1.51 kB
dist/assets/AnalyticsView-BVDsuDrI.js      4.09 kB │ gzip:  1.82 kB
dist/assets/MonitoringView-CJ9xeVXs.js     9.06 kB │ gzip:  3.07 kB
dist/assets/index-Bwkb2gmg.js            157.82 kB │ gzip: 60.14 kB
✓ built in 32.27s
```
- **Status:** **`PASS`** (0 TypeScript errors, production bundle valid)

---

## 7. Section 6: Residual Risks & Safe Rollback Procedure

### 7.1 Residual Risks & Operational Considerations

1. **Docker Desktop Windows Filesystem Watch Latency:**
   - *Observation:* On Windows with Docker Desktop WSL2/Hyper-V volume mounts, Linux inotify events from the Windows NTFS host may fail to trigger Vite HMR automatically.
   - *Mitigation:* `frontend/vite.config.ts` was updated with `watch: { usePolling: true, interval: 800 }`. If hot-reloads stall, running `docker restart smartgrow-frontend` immediately refreshes module state.
2. **Backend Image Rebuild on Code Changes:**
   - *Observation:* `backend/Dockerfile` copies code via `COPY backend/app /app/backend/app` without host volume mount for production parity.
   - *Mitigation:* Any host modifications to `backend/` require `docker compose -f deploy/docker-compose.yml build backend` and container recreation to take effect.
3. **Physical Controller Telemetry Influx:**
   - *Observation:* ESP32-S3 `b8:1f:3f:bb:d0:c4` sends telemetry every 4 seconds.
   - *Mitigation:* During testing or maintenance, avoid clearing or resetting `sensor_readings` directly without offline backup.

### 7.2 Safe Rollback Procedure

In the unlikely event that a rollback is required:

1. **Database Rollback:**
   ```bash
   # 1. Stop backend container
   docker stop smartgrow-backend

   # 2. Restore verified consistent backup into Docker volume
   docker run --rm -v deploy_smartgrow_data:/data -v "c:\Users\ADMIN\Downloads\smartgrow-ai\backups:/backup" alpine cp /backup/smartgrow_audit_final_20261027.db /data/smartgrow.db

   # 3. Restart backend container
   docker start smartgrow-backend
   ```
2. **Code Rollback:**
   ```bash
   git checkout master -- backend/app/models/entities.py backend/app/api/v1/telemetry.py frontend/src/utils/ frontend/src/views/
   docker compose -f deploy/docker-compose.yml build backend
   docker compose -f deploy/docker-compose.yml up -d --no-deps backend
   docker restart smartgrow-frontend
   ```

---

## 8. Summary of Verification Verdicts

- **Diff & Code Correctness:** **`PASS`**
- **Timezone Serialization & Parsing:** **`PASS`**
- **Historical Data Integrity:** **`PASS`**
- **CSV Export (Monitoring & Analytics):** **`PASS`**
- **Database Persistence & Volume Mount:** **`PASS`**
- **Full Test Suite & Build:** **`PASS`**
