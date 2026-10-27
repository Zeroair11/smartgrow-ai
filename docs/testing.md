# SmartGrow AI — Comprehensive Testing & Quality Assurance Guide

**Version:** 2.0.0  
**Status:** Hardened & Verified  
**Date:** 2026-10-25  

SmartGrow AI employs a multi-tiered test suite ensuring safety, deterministic edge computation, and reliable backend-frontend integration.

---

## 1. Test Architecture & Stratification

| Test Level | Scope | Environment | Execution Command | Result |
| :--- | :--- | :--- | :--- | :--- |
| **Level A** | Firmware Native Unit Tests | Host GCC (MSYS2 / Linux) | `gcc ... && .\test_firmware_native.exe` | **50 / 50 PASS (100%)** |
| **Level A** | TinyML Edge AI C Parity | Host GCC | `gcc ... && .\test_ai_parity.exe` | **500 / 500 + 4 boundary PASS (100%)** |
| **Level B** | Backend Unit & Security Resilience | Python 3.11 / Pytest | `pytest backend/tests -v` | **12 / 12 PASS (100%)** |
| **Level B** | Vertical End-to-End Integration | Python 3.11 / SQLite | `python tests/integration/test_end_to_end_flow.py` | **12 / 12 Flows PASS (100%)** |
| **Level B** | HIL Simulator Suite | Python 3.11 | `python tests/hil/hil_runner.py --sim-only` | **SIMULATION_PASS** |
| **Level C** | Physical HIL (Target ESP32-S3) | ESP32-S3 USB COM Port | `python tests/hil/hil_runner.py` | **SKIPPED** *(No USB device connected)* |
| **Frontend**| Vue 3 + TypeScript Build | Node.js 24 / Vite | `npm run build` (in `frontend/`) | **115 modules PASS** |

---

## 2. Level A: Embedded Firmware Native Unit Tests

Tests sensor calibration routines, plausible ADC range checking ($150 \le \text{ADC} \le 3850$), safety watchdog cutoffs (actuator stopped after 120s max runtime), mandatory cooldown interlocks, timer rollover arithmetic safety, idempotency, command parsing, sensor fault rejection, and dual-partition OTA chunk processing with SHA-256 validation.

### Compilation and Execution:
```powershell
gcc -Wall -Wextra -I firmware/include `
  firmware/src/sensor_calibration.c `
  firmware/src/sensor_manager.c `
  firmware/src/safety_manager.c `
  firmware/src/actuator_manager.c `
  firmware/src/command_manager.c `
  firmware/src/telemetry_client.c `
  firmware/src/edge_ai_model.c `
  firmware/src/edge_ai.c `
  firmware/src/ota_manager.c `
  firmware/test/test_firmware_native.c `
  -o firmware/test/test_firmware_native.exe

.\firmware\test\test_firmware_native.exe
```

### Verified Assertions:
1. `sensor_calibration_raw_to_moisture_pct` (dry/wet/midpoint/inverted protection)
2. `sensor_calibration_is_adc_plausible` (open-circuit, short-circuit, and valid ranges)
3. `sensor_manager_process_raw` (valid conversion and open-circuit fault detection)
4. `safety_manager_can_start_pump` (cooldown enforcement, max runtime, safe defaults)
5. `safety_manager_tick` (pump actuator forced shut off upon 120s timeout, 32-bit rollover safe)
6. `command_manager_execute` (valid dispatch, sensor fault blocking, max duration enforcement, deduplication)
7. `ota_manager` (version checking, chunk writing, state reset, SHA-256 validation)
8. `telemetry_client_format_payload` (JSON serialization, ISO-8601 formatting, sequence monotonicity)

**Result:** `50 / 50 passed (100.0%)`.

---

## 3. Level A: TinyML Edge AI C Parity & Boundary Suite

Evaluates 500 independent samples from the test partition through the C transpiled decision tree against scikit-learn reference predictions, plus 4 edge boundary conditions.

### Compilation and Execution:
```powershell
gcc -Wall -Wextra -I firmware/include `
  firmware/src/edge_ai_model.c `
  firmware/test/test_ai_parity.c `
  -o firmware/test/test_ai_parity.exe

.\firmware\test\test_ai_parity.exe
```

### Verified Boundaries:
- Random test samples: 500 / 500 matches ($100.00\%$)
- Sub-zero temperature ($-20.0^\circ$C): Correctly classified
- Extreme heat ($85.0^\circ$C): Correctly classified
- Class label enum mapping: Verified `IRRIGATE_LIGHT` $\to$ `"IRRIGATE_LIGHT"`
- Out-of-bounds class ID: Safely returns `"UNKNOWN"` without memory violation

**Result:** `500 / 500 parity matches + 4 boundary assertions passed`.

---

## 4. Level B: Backend Unit & Security Resilience Suite

Runs 12 unit and integration tests covering API endpoints, device enrollment, JWT expiration, RBAC enforcement, command idempotency, terminal state transition locks, and excessive duration rejection.

### Execution:
```powershell
$env:PYTHONPATH="."
.\backend\.venv\Scripts\python -m pytest backend/tests -v
```

### Verified Test Cases:
1. `test_health_check`: Server status and version
2. `test_login_and_token`: Admin JWT issuance
3. `test_device_enrollment`: Device token authentication
4. `test_telemetry_ingestion_and_query`: Valid storage and reading
5. `test_command_lifecycle`: Pending $\to$ Acknowledged $\to$ Completed
6. `test_automation_rule_evaluation`: Rule triggers on conditions
7. `test_irrigation_safety_limits`: Enforces cooldown and daily quotas
8. `test_rbac_viewer_cannot_dispatch_command`: 403 Forbidden for Viewer role
9. `test_command_idempotency`: Duplicate requests return identical record
10. `test_command_cannot_overwrite_terminal_state`: Prevents resurrection of expired/completed commands
11. `test_command_rejects_excessive_duration`: Rejects $> 120$s commands with 400 Bad Request
12. `test_command_auto_expiry_on_query`: Expired commands flagged correctly

**Result:** `12 passed in 1.48s, 0 warnings`.

---

## 5. Level B: Full Vertical End-to-End Integration Flow

Executes all 12 complete user scenarios defined in Phase 4 of the audit specification:

```powershell
$env:PYTHONPATH="."
.\backend\.venv\Scripts\python tests/integration/test_end_to_end_flow.py
```

### Verified Flows:
1. **Flow 1:** Valid authentication (Admin JWT acquired)
2. **Flow 2:** Invalid credentials rejected (401 Unauthorized)
3. **Flow 3:** RBAC enforcement (Viewer denied actuator mutations with 403)
4. **Flow 4:** Greenhouse, Zone, and ESP32-S3 device creation
5. **Flow 5:** High-frequency telemetry ingestion with TinyML state
6. **Flow 6:** Live dashboard queries & telemetry range filtering
7. **Flow 7:** Idempotent command dispatch
8. **Flow 8:** Device polling and command ACK state transition
9. **Flow 9:** Safety rejection of excessive duration ($>120$s)
10. **Flow 10:** Dynamic automation rule creation and toggling
11. **Flow 11:** Operator alert creation, acknowledgment, and resolution
12. **Flow 12:** Logout and protected route verification

**Result:** `ALL 12 VERTICAL END-TO-END FLOWS FULLY VERIFIED PASS`.

---

## 6. Level B/C: Hardware-in-the-Loop (HIL) Test Runner

The HIL runner tests both simulation mode and physical COM port detection without blocking or falsifying results.

### CLI Usage:
```powershell
# Probe serial ports only:
.\backend\.venv\Scripts\python tests/hil/hil_runner.py --probe

# Run simulation test only:
.\backend\.venv\Scripts\python tests/hil/hil_runner.py --sim-only

# Auto-detect target board (or gracefully skip physical test):
.\backend\.venv\Scripts\python tests/hil/hil_runner.py

# Explicit port and custom timeout:
.\backend\.venv\Scripts\python tests/hil/hil_runner.py --port COM3 --timeout 15
```

### Result Behavior:
- Physical Hardware: When no USB-connected ESP32-S3 DevKit is found (only Bluetooth ports like `COM8`/`COM9` detected), the physical test is logged as `SKIPPED (No USB serial ESP32-S3 connected)`.
- Simulation Test: Compiles native C source on-the-fly and executes protocol simulation, exiting with code 0 (`SIMULATION_PASS`).

---

## 7. Frontend: Type Check & Production Build

Verifies all Vue 3 components, Pinia stores, TypeScript definitions, and Vite production bundle.

### Execution:
```powershell
cd frontend
npm run build
```

**Result:** `115 modules transformed, built in 765ms, zero errors`.
