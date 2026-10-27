# SmartGrow AI — Security Review & Threat Hardening Report

**Version:** 2.0.0  
**Date:** 2026-10-25  

This document evaluates the security architecture of SmartGrow AI across authentication, role-based access control, command lifecycle protection, secret management, and threat mitigations.

---

## 1. Authentication & Identity Management

### 1.1. User Authentication
- **Mechanism:** JWT (JSON Web Tokens) signed via HMAC-SHA256 (`HS256`).
- **Password Storage:** Hashed using `bcrypt` (Passlib). Plaintext passwords are never stored, logged, or serialized in response DTOs.
- **Token Expiry:** Access tokens carry a standard expiry window (`ACCESS_TOKEN_EXPIRE_MINUTES = 120`). Expired tokens are rejected with `401 Unauthorized`.
- **Token Revocation:** Tested in E2E Flow 12. Once logged out, clients drop local tokens and subsequent requests are rejected.

### 1.2. Edge Device Authentication
- **Mechanism:** ESP32-S3 devices authenticate using an enrollment pre-shared key during provisioning, receiving a cryptographically random device secret token.
- **Request Header:** All device ingestion and polling requests transmit `X-Device-Token`. Telemetry is rejected if the token does not match the database device record.

---

## 2. Role-Based Access Control (RBAC) Enforcement

The platform enforces three distinct user roles:

| Role | Permissions | Security Boundary |
| :--- | :--- | :--- |
| **Admin** | Full system configuration, user provisioning, device deletion, OTA release deployment | Unrestricted API access |
| **Operator** | Telemetry viewing, actuator command dispatch, rule toggles, alert resolution | Restricted from user management & OTA release creation |
| **Viewer** | Read-only dashboards, live charts, historical queries, reports | **Blocked (403 Forbidden)** from all mutation endpoints |

### Verification Evidence:
- Server-side RBAC is implemented via `require_role(["ADMIN", "OPERATOR"])` dependency injectors on mutating routers (`backend/app/api/v1/commands.py`, `devices.py`, `automation.py`).
- Dedicated security test: `backend/tests/test_security_resilience.py::test_rbac_viewer_cannot_dispatch_command` asserts that a Viewer attempting to start the pump receives `403 Forbidden`.
- Verified in E2E Integration Flow 3.

---

## 3. Command Lifecycle Security & Anti-Replay

### 3.1. Idempotency Key Deduplication
- **Threat:** Network jitter or retry storms causing an operator command (e.g., `PUMP_START` for 30s) to be ingested multiple times, running the pump repeatedly.
- **Protection:** All command requests support an optional `idempotency_key` (UUID v4 or unique string). The backend checks the indexed column in `device_commands`. Duplicate keys return the existing record with `200 OK` rather than generating a new database entry or queue item.
- **Verified:** Tested in `test_security_resilience.py::test_command_idempotency` and E2E Flow 7.

### 3.2. Terminal State Transition Protection
- **Threat:** A delayed ACK arriving over a flaky cellular or Wi-Fi link resurrecting an already `expired`, `rejected`, or `failed` command back to `acknowledged`.
- **Protection:** `command_service.py` validates that state transitions are strictly monotonic. Attempting to ACK a command already in a terminal state returns the existing record unchanged and logs a security warning.
- **Verified:** Tested in `test_security_resilience.py::test_command_cannot_overwrite_terminal_state`.

### 3.3. Actuator Duration Safety Guardrail
- **Threat:** Malicious or erroneous payload requesting a pump duration of 3600 seconds (1 hour), causing flooding or pump motor burnout.
- **Protection:** Both `command_service.py` and `automation_engine.py` reject commands requesting duration $> 120$ seconds with `400 Bad Request`. Firmware also enforces an independent 120s hardware cutoff.
- **Verified:** Tested in `test_security_resilience.py::test_command_rejects_excessive_duration` and E2E Flow 9.

---

## 4. Secret Management & Sensitive Data Hygiene

- **No Hardcoded Secrets in Git:** All secrets (`SECRET_KEY`, `DATABASE_URL`) are read from environment variables or `.env` files via Pydantic V2 `SettingsConfigDict`.
- **Git Hygiene:** `.gitignore` excludes `.env`, `*.db`, `__pycache__`, `node_modules`, and build artifacts (`*.exe`, `dist/`).
- **Audit Logging:** System mutations (command dispatch, alert resolution, rule creation) create immutable entries in `audit_logs` recording user ID, action, and timestamp.

---

## 5. Production Hardening Recommendations (Before Public Deployment)

1. **TLS / HTTPS Termination:** Deploy backend behind a reverse proxy (Nginx, Caddy, or Cloudflare) with modern TLS 1.3 ciphers.
2. **Rate Limiting:** Implement token-bucket rate limiting on `/api/v1/auth/login` to prevent credential stuffing.
3. **Hardware Secure Boot:** Enable Espressif Secure Boot v2 and Flash Encryption on ESP32-S3 hardware eFuses.
4. **PostgreSQL Migration:** Migrate from SQLite to PostgreSQL with managed backups and connection pooling.
