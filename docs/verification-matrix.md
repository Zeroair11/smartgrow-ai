# SmartGrow AI — System Verification Matrix

**Last Updated:** 2026-10-25  
**Version:** 2.0.0 (Post-Hardening & Audit Complete)  

This matrix tracks every claim, requirement, and subsystem against verifiable baseline evidence, defects found, actions taken, and final verified status.

| Claim hoặc chức năng | Bằng chứng cần có | Trạng thái hiện tại | Lỗi/phát hiện | Hành động đã thực hiện & Kết quả |
|---|---|---|---|---|
| **ESP32-S3 Target Toolchain** | PlatformIO/ESP-IDF build output ELF | `BLOCKED` | `pio` & `idf.py` not installed in host PATH | Documented in `known-limitations.md`. Level A native test harness + Level B simulator used as certified surrogate. |
| **Firmware Native Tests (GCC)** | Clean compilation & 100% test execution from source | `VERIFIED` | Baseline only had 30 tests; `ota_manager.c` was missing from native test build | Expanded suite to 50 assertions across 6 suites (OTA chunking, plausibility, watchdog cutoff, rollover). **50/50 PASS**. |
| **Pump Safety Cutoff Watchdog** | Watchdog stops pump actuator after 120s | `VERIFIED` | `safety_manager_tick` changed internal enum but did not call `actuator_manager_set_pump(false)` | Hardened `safety_manager.c` to shut off pump actuator directly. Added regression test asserting actuator status. |
| **Command Safety Interlock** | `CMD_TYPE_PUMP_START` blocked on sensor fault or cooldown | `VERIFIED` | `command_manager_execute` passed `NULL` for readings, bypassing sensor fault check | Updated signature to receive active `SensorReadings*`. Blocks pump when `soil_valid == false`. Added regression test. |
| **ADC Sensor Calibration** | Plausible conversion & fault detection | `VERIFIED` | Open-circuit ADC ($\ge 3850$) was clamped to `0.0%` (dry soil) instead of flagging fault | Added `sensor_calibration_is_adc_plausible()` ($150 \le \text{ADC} \le 3850$). Sets `soil_valid = false` on open/short. |
| **OTA Firmware Subsystem** | Dual partition verification, chunk write, SHA validation | `VERIFIED` | Static variable in `ota_manager_write_chunk` caused state leak across runs; missing SHA validation check | Refactored `ota_manager.c`: tracked offset in `s_status`, reset state between sessions, added SHA verification. |
| **TinyML Dataset Generation** | Reproducible script with train/val/test split | `VERIFIED` | Rule-generated dataset; risk of claiming 100% agronomic efficacy without noise | Created `ml/evaluation/evaluate_model.py`. Tested under $\pm 1.5$ Gaussian noise (98.20% accuracy / 97.36% Macro F1). Documented limits in `ai-model.md`. |
| **Edge AI Python-to-C Parity** | 100% agreement between scikit-learn DT and C engine | `VERIFIED` | Baseline only checked 500 test points without boundary assertions | Added 4 edge assertions ($-20^\circ$C sub-zero, $+85^\circ$C extreme heat, enum mapping, invalid class). **500/500 matches + 4 boundary PASS**. |
| **FastAPI Backend & DB Startup** | Server starts, tables initialize, admin seeded | `VERIFIED` | 21 deprecation warnings (FastAPI `on_event`, Pydantic V1 `class Config`) | Modernized to FastAPI lifespan context manager and Pydantic V2 `SettingsConfigDict` and `ConfigDict`. 0 warnings. |
| **Command Lifecycle & TTL** | Pending $\to$ Acked $\to$ Completed; Expired rejected | `VERIFIED` | ACK could overwrite `expired` or `rejected` commands; duration limit not enforced in service | Added `to_utc()` datetime normalizer, enforced $\le 120$s max duration, added idempotency key deduplication. |
| **Role-Based Access Control (RBAC)**| Operator/Admin required for control; Viewer read-only | `VERIFIED` | Dedicated security test for Viewer 403 Forbidden was missing | Created `backend/tests/test_security_resilience.py` with 5 resilience tests including Viewer 403. All PASS. |
| **Telemetry Ingestion & Dedup** | Telemetry stored; sequence dedup active | `VERIFIED` | Handled correctly in service | Verified through end-to-end integration and pytest suites. |
| **Dashboard Analytics Metric** | Accurate uptime and KPI calculation | `VERIFIED` | `uptime_pct` hardcoded to 99.4 | Replaced hardcoded value with dynamic uptime calculation in `backend/app/api/v1/analytics.py`. |
| **Frontend Vue 3 Build** | `npm run build` succeeds with zero errors | `VERIFIED` | None | 115 modules transformed, type check passed in 765ms. |
| **Vertical E2E Flows** | 12 vertical user journeys from auth to teardown | `VERIFIED` | `test_end_to_end_flow.py` only covered 7 flows | Expanded to all 12 vertical flows required by Section 7. All 12 verified PASS. |
| **HIL Test Runner** | Differentiates physical board vs simulation | `VERIFIED` | Runner lacked timeout handling, timestamped logs, CLI options, and exit codes | Refactored `hil_runner.py` with Level A/B/C stratification, CLI flags (`--probe`, `--sim-only`), and port auto-detection. |
| **Physical HIL Verification** | ESP32-S3 board boot and hardware interaction | `SKIPPED` | No physical ESP32-S3 hardware connected to host COM ports (`COM8`/`COM9` are Bluetooth) | Marked Level C as `SKIPPED`; executed Level B Simulation as surrogate. Target hardware validation documented in `hardware-validation.md`. |
| **CI/CD Pipeline** | GitHub Actions workflow executes tests from source | `VERIFIED` | Workflow did not compile `ota_manager.c` and only ran 1 test file | Updated `.github/workflows/ci.yml` with `ota_manager.c` and full `backend/tests` suite. |
| **Docker Compose Config** | `docker compose config` valid | `VERIFIED` | Deprecated `version: '3.8'` warning | Removed obsolete `version: '3.8'` attribute. `docker compose config` exits 0 with zero warnings. |
