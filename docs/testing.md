# SmartGrow AI — Comprehensive Testing & Quality Assurance Guide

SmartGrow AI employs a multi-tiered test suite ensuring safety, deterministic edge computation, and reliable backend-frontend integration.

---

## 1. Embedded Firmware Native Unit Tests
Tests sensor calibration routines, runtime safety watchdog cutoffs, mandatory cooldown interlocks, idempotency, command parsing, and telemetry serialization.

### Running Firmware Tests:
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
  firmware/test/test_firmware_native.c `
  -o firmware/test/test_firmware_native.exe

.\firmware\test\test_firmware_native.exe
```
**Expected Outcome:** `30 / 30 tests passed (100.0%)`.

---

## 2. TinyML C Parity Verification Test
Evaluates 500 test samples through the C-transpiled model against scikit-learn predictions to verify exact parity.

### Running Parity Test:
```powershell
gcc -Wall -Wextra -I firmware/include `
  firmware/src/edge_ai_model.c `
  firmware/test/test_ai_parity.c `
  -o firmware/test/test_ai_parity.exe

.\firmware\test\test_ai_parity.exe
```
**Expected Outcome:** `500 / 500 matches (100.00% parity)`.

---

## 3. Backend Pytest Integration Suite
Tests API authentication, device enrollment, telemetry schema validation, command lifecycle, automation rules evaluation, and irrigation safety limits.

### Running Pytest:
```powershell
$env:PYTHONPATH="."
.\backend\.venv\Scripts\python -m pytest backend/tests/test_backend_api.py -v
```
**Expected Outcome:** `7 passed, 0 failures`.

---

## 4. Full Vertical End-to-End Integration Test
Verifies the complete vertical loop:
1. Admin user authenticates and receives JWT.
2. ESP32-S3 registers with secret enrollment token.
3. Telemetry with sensor readings and TinyML class is ingested.
4. Frontend fetches latest telemetry and dashboard KPIs.
5. Operator dispatches pump command.
6. Controller polls, executes, and returns ACK.
7. Safety guardrail validates that excessive runtime (>120s) commands are rejected.

### Running End-to-End Test:
```powershell
$env:PYTHONPATH="."
.\backend\.venv\Scripts\python tests/integration/test_end_to_end_flow.py
```
**Expected Outcome:** `FULL VERTICAL END-TO-END FLOW SUCCESSFULLY VERIFIED`.

---

## 5. Hardware-in-the-Loop (HIL) Test Runner
Checks serial ports for physical ESP32-S3 DevModule N16R8 boards. If no board is detected, it falls back to the high-fidelity simulator and transparently marks physical tests as `SKIPPED (No USB serial board connected)` and simulation as `PASS`.

### Running HIL Runner:
```powershell
.\backend\.venv\Scripts\python tests/hil/hil_runner.py
# Or specify a port explicitly:
.\backend\.venv\Scripts\python tests/hil/hil_runner.py --port COM4
```
