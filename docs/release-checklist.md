# SmartGrow AI — Release & Production Deployment Checklist

**Version:** 2.0.0  
**Date:** 2026-10-25  

This checklist must be executed and signed off prior to staging or production deployment of the SmartGrow AI IoT platform.

---

## 1. Automated Verification & Code Quality Gates

| Check Item | Validation Command | Acceptance Criteria | Status |
| :--- | :--- | :--- | :--- |
| **Firmware Native Tests** | `gcc ... && .\test_firmware_native.exe` | 50 / 50 passed (100%) | [x] PASS |
| **TinyML Parity Tests** | `gcc ... && .\test_ai_parity.exe` | 500 / 500 parity + 4 boundary | [x] PASS |
| **Backend Unit & Security** | `pytest backend/tests -v` | 12 / 12 passed, 0 warnings | [x] PASS |
| **Vertical E2E Flows** | `python tests/integration/test_end_to_end_flow.py` | All 12 vertical flows PASS | [x] PASS |
| **Frontend Production Build**| `npm run build` (in `frontend/`) | 115 modules transformed, 0 errors | [x] PASS |
| **Docker Compose Config** | `docker compose -f deploy/docker-compose.yml config` | Exits with code 0, 0 warnings | [x] PASS |
| **Secret Audit** | Grep repository for API keys / private keys | Zero committed secrets | [x] PASS |

---

## 2. Infrastructure & Environment Configuration

- [ ] **Secret Key Rotation:** Generate a high-entropy 64-character string (`openssl rand -hex 32`) and assign to `SECRET_KEY`. Never use the development default.
- [ ] **Production Flags:** Ensure `APP_ENV=production` and `DEBUG=false` in the production environment.
- [ ] **Database Connection:** Configure `DATABASE_URL` pointing to PostgreSQL with a connection pool (`pool_size=10, max_overflow=20`).
- [ ] **CORS Policy:** Restrict `ALLOWED_ORIGINS` to the exact public domain of the Vue 3 frontend (e.g. `["https://app.smartgrow.io"]`). Never use wildcard `*` with credentials.
- [ ] **Reverse Proxy & TLS:** Deploy Nginx or Caddy terminating TLS 1.3 with automated Let's Encrypt certificates.
- [ ] **Container Isolation:** Run backend container with a non-root user and read-only root filesystem where feasible.

---

## 3. Physical Hardware Bench Sign-Off (Prior to Field Installation)

- [ ] **Hardware Probe:** Connect ESP32-S3 via USB-C and verify port detection via `python tests/hil/hil_runner.py --port COMx`.
- [ ] **Pull-Up Resistor on GPIO 10:** Verify $4.7\text{k}\Omega$ hardware pull-up on GPIO 10. Confirm multimeter reads $3.3\text{V}$ (Relay OFF) when MCU is in reset or bootloader mode.
- [ ] **Snubber / Flyback Diode:** Verify 1N4007 diode is installed across DC pump terminals.
- [ ] **Power Supply Isolation:** Confirm water pump is powered by dedicated $12\text{V}/5\text{V}$ power brick and does not draw current from the ESP32 $3.3\text{V}$ bus.
- [ ] **Two-Point Soil Calibration:** Measure probe in ambient air (`adc_dry_raw`) and tap water (`adc_wet_raw`); record values in device configuration.
- [ ] **Actuator Timeout Test:** Dispatch a test pump command; verify relay de-energizes and firmware stops the pump after 120 seconds maximum.

---

## 4. OTA Firmware Deployment & Recovery Procedure

- [ ] **Dual Partition Layout:** Verify device partition table reserves $3\text{MB}$ each for `ota_0` and `ota_1`.
- [ ] **SHA-256 Digest Match:** Check firmware binary checksum matches backend release metadata before dispatch.
- [ ] **Rollback Verification:** In staging, simulate a corrupted OTA image; verify ESP32 bootloader reverts to the active working partition within 30 seconds.

---

## 5. Release Go / No-Go Decision

- [x] **Local Development & Simulation:** `ACCEPTED FOR LOCAL DEVELOPMENT`
- [x] **Controlled Hardware Bench Testing:** `ACCEPTED FOR CONTROLLED HARDWARE TESTING`
- [ ] **Field Production Ready:** Pending completion of Section 3 (Physical Hardware Bench Sign-Off) and Section 2 (PostgreSQL & TLS deployment).
