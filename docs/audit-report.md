# SmartGrow AI — Comprehensive Repository Audit & Baseline Report

**Audit Date:** 2026-10-25  
**Auditor:** Senior Embedded Systems & IoT Platform Engineer  
**Workspace Root:** `c:\Users\ADMIN\Downloads\smartgrow-ai`  
**Git Branch:** `master` (Commit: `e24fd7c`)

---

## 1. Executive Summary & Toolchain Audit

An exhaustive, non-destructive audit of the SmartGrow AI codebase was conducted across all subsystems: Embedded Firmware (C), Edge AI (TinyML), Backend (FastAPI/SQLAlchemy), Frontend (Vue 3/TypeScript), Testing & HIL, and DevOps/CI.

### 1.1 Toolchain Verification
| Tool | Expected / Handover Claim | Actual Verified Version | Status | Notes |
|---|---|---|---|---|
| **Git** | Installed | 2.51.0.windows.1 | VERIFIED | Repository on branch `master` |
| **Python** | 3.11+ | Python 3.11.0 (venv) / 3.14.6 (system) | VERIFIED | Virtual environment at `backend/.venv` |
| **Node.js** | 20+ | v24.11.1 | VERIFIED | Compatible with Vite 5 |
| **npm** | 10+ | 11.6.2 | VERIFIED | Clean package resolution |
| **GCC** | 14+ | gcc.exe 16.1.0 (MSYS2 / UCRT64) | VERIFIED | Host native compilation toolchain |
| **PlatformIO / ESP-IDF** | Claimed in build specs | Not found in system PATH (`pio`/`idf.py`) | BLOCKED ON TARGET BUILD | ESP32-S3 cross-compiler not on host PATH |
| **Docker & Compose** | Docker 24+ | Docker 29.8.0 / Compose v5.5.1 | VERIFIED | Docker daemon active |
| **Physical ESP32-S3 Hardware** | Claimed port | COM8, COM9 detected as Bluetooth Serial links | NOT DETECTED | No USB-UART / ESP32-S3 JTAG connected |

---

## 2. Baseline Test Run Results (Pre-Fix Baseline)

Before introducing any modifications, all existing test suites were executed from actual source code:

1. **Firmware Native Tests (`test_firmware_native.c`):**
   - Built from source using `gcc -Wall -Wextra -I firmware/include ...`
   - Result: 30 / 30 PASS.
   - *Finding:* `firmware/src/ota_manager.c` was completely omitted from compilation and unit testing.

2. **Edge AI Parity Tests (`test_ai_parity.c`):**
   - Built from source and executed against `ml/export/test_samples.csv`.
   - Result: 500 / 500 PASS (100.00% parity).
   - *Finding:* Dataset generated with deterministic rule script; no boundary/noisy samples tested in baseline.

3. **Backend API Pytest Suite (`backend/tests/test_backend_api.py`):**
   - Ran with pytest 9.1.1 on Python 3.11.
   - Result: 7 / 7 PASS.
   - *Finding:* 21 deprecation warnings emitted (FastAPI `@app.on_event`, Pydantic V1 `class Config`, `.dict()`).

4. **Integration Test (`tests/integration/test_end_to_end_flow.py`):**
   - Failed initially if run as `python tests/integration/test_end_to_end_flow.py` due to missing `sys.path` configuration (`ModuleNotFoundError: No module named 'backend'`).
   - Succeeded when `.` was added to `sys.path`.

5. **Frontend Production Build (`npm run build`):**
   - Executed `vue-tsc --noEmit && vite build`.
   - Result: PASS (Built in 3.40s, 115 modules transformed, zero type errors).

6. **HIL Runner (`tests/hil/hil_runner.py`):**
   - Fallback simulator passed.
   - *Finding:* Runner lacked timestamped logging, CLI flags (`--sim-only`, `--probe`), and didn't distinguish physical test status properly.

---

## 3. Discrepancies & Real Defects Discovered

### P0 / P1 Critical & High Defects

1. **Safety Cutoff Actuator Disconnect (Firmware P0):**
   In `firmware/src/safety_manager.c`, `safety_manager_tick()` set `s_state.pump_is_running = false`, but did NOT shut off the pump actuator in `actuator_manager` (`actuator_manager_set_pump(false)` was not called). If an application loop relied on the watchdog tick without manual checks, the physical pump remained energized!

2. **Sensor Fault Safety Bypass in Command Execution (Firmware P0):**
   In `firmware/src/command_manager.c`, `CMD_TYPE_PUMP_START` called `safety_manager_can_start_pump(NULL, current_time_ms, &safety_err)`. Passing `NULL` as the sensor readings argument completely bypassed sensor fault (`soil_valid`, `dht_valid`) and temperature safety bounds! An operator or automation rule could start the pump even when sensors were disconnected or damaged.

3. **Silent Open-Circuit / Stuck ADC Conversion to "Dry Soil" (Firmware P1):**
   In `firmware/src/sensor_calibration.c`, raw ADC values $\ge 3200$ were clamped to `0.0%` soil moisture without validating if ADC was floating, disconnected, or shorted ($> 3800$ or $< 100$). A severed soil probe wire floats to 3.3V (ADC $\approx 4095$), converting to "0.0% Dry Soil", causing the AI and automation engine to continuously demand water.

4. **OTA Chunk Buffer Static State Bug (Firmware P1):**
   In `firmware/src/ota_manager.c`, `static size_t bytes_written = 0;` inside `ota_manager_write_chunk()` persisted across OTA sessions, causing any second OTA attempt or re-try to fail or calculate invalid progress percentages.

5. **Command Lifecycle State Overwrite (Backend P1):**
   In `backend/app/services/command_service.py`, `acknowledge_command()` unconditionally overwrote `cmd.status = ack.status` without verifying that the command was still in `pending` or `executing`. This allowed an `expired` or previously `rejected` command to be resurrected to `completed` upon late ACK receipt.

6. **Safety Guardrail Bypass in Backend Command Service (Backend P1):**
   While `manual_start_irrigation` rejected durations $> 120$s, direct API calls to `/api/v1/devices/{device_id}/commands` and `automation_engine.py` allowed arbitrary pump durations (e.g. 600s), bypassing the 120s hardware safety constraint.

7. **Hardcoded Metric in Analytics API (Backend P2):**
   In `backend/app/api/v1/analytics.py`, `uptime_pct` was hardcoded to `99.4 if d.status == "online" else 0.0` instead of dynamically computing the metric from telemetry and registration history.

8. **Committed Binaries in Repository (DevOps P2):**
   `firmware/test/test_firmware_native.exe` and `firmware/test/test_ai_parity.exe` were committed to git history rather than built on-demand from source code.

---

## 4. Remediation Plan

All identified defects will be resolved systematically with strict regression tests added across all layers.
