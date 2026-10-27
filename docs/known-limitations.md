# SmartGrow AI — Known Limitations & Environmental Constraints

**Version:** 2.0.0  
**Date:** 2026-10-25  

This document details all technical limitations, environment constraints, and deferred verifications identified during the repository audit and hardening process.

---

## 1. Physical Hardware & Target Execution Constraints

### 1.1. Absence of Physical ESP32-S3 Target Hardware
- **Current State:** The audit host has only virtual Bluetooth serial ports (`COM8`, `COM9`). No physical ESP32-S3 DevKit (N16R8) was connected via USB.
- **Impact:** Level C Physical HIL testing (flashing, real serial communication, reading real analog sensor voltages) could not be executed on target silicon.
- **Mitigation & Status:** Level C is marked as `SKIPPED`. Level A Native Unit Tests (50 assertions) and Level B Protocol Simulation were executed as certified surrogates. Physical validation must be completed prior to production deployment (see `docs/hardware-validation.md`).

### 1.2. PlatformIO / ESP-IDF Toolchain Availability
- **Current State:** Neither `pio` (PlatformIO CLI) nor `idf.py` (ESP-IDF) were present in the host system PATH.
- **Impact:** Target cross-compilation into Xtensa LX7 ELF binaries was blocked on this host.
- **Mitigation & Status:** Marked as `BLOCKED` for host compilation. The firmware architecture strictly separates platform drivers from pure C business logic (`safety_manager`, `sensor_calibration`, `command_manager`, `edge_ai_model`, `ota_manager`), enabling 100% validation using host GCC 16.1.0.

---

## 2. Edge AI & TinyML Dataset Constraints

### 2.1. Synthetic Dataset & Agronomic Ground Truth
- **Current State:** The TinyML decision tree is trained on a synthetic dataset (`ml/datasets/greenhouse_dataset.csv`) generated via rule-based thresholds.
- **Impact:** The model achieves 100% accuracy on the synthetic test set, and 98.20% accuracy / 97.36% Macro F1 under Gaussian noise ($\sigma = 1.5$). However, this does not constitute empirical agronomic proof across diverse crop varietals, substrate types (coco coir, perlite, peat), or varying salinity levels.
- **Mitigation & Status:** Documented in `docs/ai-model.md`. The platform enforces that TinyML inference acts only as an advisory or bounded request; it can **never** bypass the hardcoded `safety_manager` guardrails (120s runtime cutoff, 180s cooldown, sensor fault interlocks).

---

## 3. Backend & Database Architecture

### 3.1. SQLite Development Database Concurrency
- **Current State:** The local development and automated integration tests run against SQLite (`smartgrow.db`).
- **Impact:** SQLite enforces a database-level write lock. In high-frequency multi-device production deployments with concurrent uvicorn workers, SQLite can produce `OperationalError: database is locked`.
- **Mitigation & Status:** The application architecture is built with SQLAlchemy ORM and supports PostgreSQL via `DATABASE_URL=postgresql://user:pass@host:5432/smartgrow`. Production deployments must switch to PostgreSQL with connection pooling (`deploy/docker-compose.yml` provides production configuration).

### 3.2. Idempotency Key Cleanup
- **Current State:** Idempotency keys are indexed in `device_commands.idempotency_key` and checked on creation.
- **Impact:** Old idempotency records persist in the command history indefinitely unless purged.
- **Mitigation:** In high-volume production, an asynchronous cron or scheduled task should purge idempotency keys older than 24 hours.

---

## 4. Firmware Security & Safety

### 4.1. Relay Boot Glitch Prevention & Polarity Alignment
- **Current State:** GPIO 10 is configured with default Active-HIGH polarity (`PROJECT_RELAY_PUMP_ACTIVE_LEVEL = 1`) in `firmware/include/project_config.h`, with pre-charge logic in `actuator_manager.c` before output enable. If an Active-LOW module is used (`0`), pre-charge similarly sets logic 1.
- **Hardware Requirement:** During cold boot (first ~20ms before firmware boots), GPIO 10 is floating (High-Z). For Active-HIGH relays, an external 10kΩ pull-down resistor to GND is required; for Active-LOW relays, an external 4.7kΩ pull-up resistor to 3.3V is required. Pre-flight verification with a DMM is mandatory (see `docs/HARDWARE_WIRING_AND_GPIO.md`).

### 4.2. Cryptographic Firmware Signature Verification
- **Current State:** The firmware OTA subsystem (`ota_manager.c`) computes and validates SHA-256 digests of downloaded image chunks.
- **Impact:** SHA-256 guarantees image integrity (no corruption in transit), but does not authenticate the origin without public key cryptography.
- **Mitigation:** For production Internet-facing deployments, Espressif Secure Boot v2 (RSA-3072 or ECDSA) must be enabled in eFuses on the ESP32-S3 silicon to prevent flashing untrusted binaries.
