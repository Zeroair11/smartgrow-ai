# SmartGrow AI — Comprehensive Implementation, Hardening & Verification Plan

**Version:** 2.0.0  
**Status:** In Progress (Audit Complete, Hardening Active)  
**Target Board:** ESP32-S3 DevModule N16R8 (16MB Flash, 8MB Octal PSRAM)  
**Host Platform:** Windows 10/11 x64, GCC 16.1.0, Python 3.11.0, Node.js v24.11.1, Docker 29.8.0  

---

## 1. Executive Strategy

This plan follows a strict verification-first approach:
1. **Audit & Baseline (Complete):** Identified toolchains, baseline test outputs, deprecation warnings, missing tests, and safety logic flaws.
2. **Firmware & Embedded Hardening (Phase 1):** Fix actuator disconnect during watchdog trips, pass sensor readings into command safety checks, guard against disconnected ADC floating probes, fix static buffer bug in `ota_manager.c`, and add unit tests for OTA.
3. **Edge AI / TinyML Verification (Phase 2):** Document synthetic dataset boundaries, evaluate model against noisy/boundary test samples, test decision tree boundary transitions in C, and ensure AI never overrides safety rules.
4. **Backend Platform Hardening (Phase 3):** Modernize FastAPI lifespan and Pydantic V2 models, enforce strict command lifecycle transitions, add `idempotency_key` deduplication, enforce $\le 120$s pump limit globally, fix hardcoded analytics, and expand RBAC/security tests.
5. **Frontend Flow Validation (Phase 4):** Verify that all 12 frontend views bind to real API contracts and handle loading, error, and stale telemetry states.
6. **OTA & Dual-Partition Lifecycle (Phase 5):** Verify partition table alignments, SHA-256 verification, version downgrade rejection, and backend deployment tracking.
7. **HIL Test Suite Stratification (Phase 6):** Refactor `hil_runner.py` into distinct Level A (Native Unit), Level B (Firmware Simulation & End-to-End), and Level C (Physical Board) layers with CLI controls (`--sim-only`, `--probe`).
8. **DevOps & CI/CD Hardening (Phase 7):** Update `.github/workflows/ci.yml` and `docker-compose.yml`.
9. **Final Acceptance & Reporting (Phase 8-9):** Generate all handover documentation with reproducible verification commands.

---

## 2. Detailed Task Breakdown & Progress Tracking

### Phase 1: Firmware & Hardware Layer Hardening
- [x] Baseline audit of C source files (`firmware/src/` and `firmware/include/`).
- [ ] Fix `safety_manager_tick()`: Call `actuator_manager_set_pump(false)` immediately when safety limits are tripped.
- [ ] Fix `command_manager_execute()`: Pass active `SensorReadings*` to `safety_manager_can_start_pump()`; reject pump start on sensor fault or extreme temperature.
- [ ] Fix `sensor_calibration` & `sensor_manager`: Detect out-of-range ADC ($> 3800$ or $< 100$) as sensor faults rather than silently clamping to 0% moisture.
- [ ] Fix `ota_manager.c`: Remove function-local static variable in `ota_manager_write_chunk()` and add reset in `ota_manager_init()`.
- [ ] Expand native test suite (`test_firmware_native.c`) to cover OTA, timer rollover, stuck ADC detection, and actuator emergency cutoff.

### Phase 2: Edge AI / TinyML Hardening
- [x] Baseline verification: 500/500 C parity match.
- [ ] Document dataset generation heuristics and evaluate against boundary conditions.
- [ ] Create `ml/evaluation/evaluate_model.py` to produce classification report, confusion matrix, and out-of-distribution robustness metrics.
- [ ] Expand `test_ai_parity.c` to test numerical edge cases ($\pm 0.001$ threshold boundaries, out-of-domain temperatures, and NaN/Inf guards).

### Phase 3: Backend, Database & API Hardening
- [x] Baseline verification: 7/7 pytest passed.
- [ ] Eliminate 21 deprecation warnings: Migrate from `on_event` to FastAPI `lifespan`, replace Pydantic V1 `class Config` with `ConfigDict(from_attributes=True)`, and `.dict()` with `.model_dump()`.
- [ ] Command service hardening:
  - Add `idempotency_key` deduplication.
  - Enforce command state transitions (`pending -> acknowledged -> executing -> completed / failed / rejected`).
  - Block ACKs from resurrecting `expired`, `rejected`, or `failed` commands.
  - Enforce $\le 120$s pump duration limit inside `create_command()`.
- [ ] Analytics API: Compute real uptime percentage based on device telemetry instead of hardcoding 99.4%.
- [ ] Add comprehensive tests: `backend/tests/test_security_resilience.py` covering RBAC (Viewer forbidden from actuator/config mutations), idempotency, command expiry, and sensor fault handling.

### Phase 4: Frontend Vue 3 Hardening & E2E Verification
- [x] Baseline verification: `npm run build` succeeds without errors.
- [ ] Fix `tests/integration/test_end_to_end_flow.py` so it executes independently with proper `sys.path`.
- [ ] Expand E2E vertical integration suite to cover all 12 key user flows (auth, telemetry, commands, automation, alerts, schedules, OTA).

### Phase 5: OTA & Partitioning Verification
- [ ] Inspect and verify `partitions_16mb.csv` offsets (0x320000 and 0x920000 for dual 6MB OTA partitions).
- [ ] Verify SHA-256 integrity checks and version downgrade guards in firmware and backend.

### Phase 6: HIL Runner Refactoring
- [ ] Refactor `tests/hil/hil_runner.py`:
  - Add argparse options: `--sim-only`, `--probe`, `--port`.
  - Add timestamped structured logging.
  - Report distinct statuses: `PHYSICAL_PASS`, `SIMULATION_PASS`, `SKIPPED`, `FAILED`.
  - Ensure zero false-positive passes when physical board is missing.

### Phase 7: CI/CD & Deployment Hardening
- [ ] Update `.github/workflows/ci.yml` to compile `ota_manager.c` and execute the full expanded test suite.
- [ ] Update `deploy/docker-compose.yml` (remove deprecated version attribute, ensure production/dev separation).

### Phase 8 & 9: Test Evidence & Handover Documentation
- [ ] Compile full test evidence log.
- [ ] Update `docs/testing.md`, `docs/known-limitations.md`, `docs/hardware-validation.md`, `docs/security-review.md`, and `docs/release-checklist.md`.
- [ ] Produce Final Acceptance Report.
