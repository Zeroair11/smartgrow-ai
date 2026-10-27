from datetime import datetime, timezone
from sqlalchemy.orm import Session
from fastapi import HTTPException
from backend.app.models.entities import SensorReading, Device, AIInferenceEvent, Alert
from backend.app.schemas.dtos import TelemetryIngest
from backend.app.services.automation_engine import evaluate_automation_rules

AI_CLASS_LABELS = {
    0: "NORMAL",
    1: "NEED_WATER",
    2: "HIGH_TEMPERATURE",
    3: "LOW_LIGHT",
    4: "SENSOR_FAULT"
}

def ingest_telemetry(db: Session, telemetry: TelemetryIngest) -> SensorReading:
    device = db.query(Device).filter(Device.device_id == telemetry.device_id).first()
    if not device:
        raise HTTPException(status_code=404, detail=f"Device {telemetry.device_id} not registered")
    if device.status in ["revoked", "decommissioned"]:
        raise HTTPException(status_code=403, detail="Device registration is revoked or decommissioned")

    now = datetime.now(timezone.utc)

    # Update device online state and versions
    device.status = "online"
    device.last_seen_at = now
    if telemetry.firmware_version:
        device.firmware_version = telemetry.firmware_version
    if telemetry.model_version:
        device.model_version = telemetry.model_version

    # Duplicate check on sequence for same boot_id
    if telemetry.boot_id:
        existing = db.query(SensorReading).filter(
            SensorReading.device_id == telemetry.device_id,
            SensorReading.boot_id == telemetry.boot_id,
            SensorReading.sequence == telemetry.sequence
        ).first()
        if existing:
            # Idempotent skip: return existing without double-counting
            return existing

    soil_valid = bool(telemetry.soil_valid) if telemetry.soil_valid is not None else (telemetry.soil_moisture_pct is not None)
    dht_valid = bool(telemetry.dht_valid) if telemetry.dht_valid is not None else (telemetry.temperature_c is not None and telemetry.humidity_pct is not None)
    light_valid = bool(telemetry.light_valid) if telemetry.light_valid is not None else (telemetry.light_level_pct is not None)

    reading = SensorReading(
        device_id=telemetry.device_id,
        zone_id=device.zone_id,
        sequence=telemetry.sequence,
        boot_id=telemetry.boot_id,
        soil_moisture_pct=telemetry.soil_moisture_pct if soil_valid else None,
        temperature_c=telemetry.temperature_c if dht_valid else None,
        humidity_pct=telemetry.humidity_pct if dht_valid else None,
        light_level_pct=telemetry.light_level_pct if light_valid else None,
        soil_valid=soil_valid,
        dht_valid=dht_valid,
        light_valid=light_valid,
        ai_class=telemetry.ai_class or 0,
        ai_confidence=telemetry.ai_confidence or 1.0,
        model_version=telemetry.model_version or "v1.0",
        firmware_version=telemetry.firmware_version or "1.0.0",
        pump_state=telemetry.pump_state,
        operating_mode=telemetry.operating_mode or "AUTO",
        health_status=telemetry.health_status or "HEALTHY",
        recorded_at=now
    )
    db.add(reading)

    # Record Edge AI Inference Event
    ai_label = AI_CLASS_LABELS.get(telemetry.ai_class or 0, "UNKNOWN")
    ai_event = AIInferenceEvent(
        device_id=telemetry.device_id,
        zone_id=device.zone_id,
        recorded_at=now,
        soil_moisture_pct=telemetry.soil_moisture_pct if soil_valid else None,
        temperature_c=telemetry.temperature_c if dht_valid else None,
        humidity_pct=telemetry.humidity_pct if dht_valid else None,
        light_level_pct=telemetry.light_level_pct if light_valid else None,
        predicted_class=telemetry.ai_class or 0,
        predicted_label=ai_label,
        confidence=telemetry.ai_confidence or 1.0,
        model_version=telemetry.model_version or "v1.0",
        safety_override=False,
        final_action="PUMP_START" if telemetry.pump_state else "MONITOR"
    )
    db.add(ai_event)

    # Sensor Acquisition Fault Alert
    if not soil_valid or not dht_valid or not light_valid:
        faulty = []
        if not soil_valid:
            faulty.append("Soil Moisture ADC")
        if not dht_valid:
            faulty.append("DHT11 (Temp/Hum)")
        if not light_valid:
            faulty.append("Light Sensor")
        fault_alert = Alert(
            device_id=telemetry.device_id,
            zone_id=device.zone_id,
            severity="warning",
            category="sensor",
            title=f"Sensor Acquisition Fault: {', '.join(faulty)}",
            message=f"Device {telemetry.device_id} detected plausible reading/hardware fault. Irrigation pump held OFF by fail-safe.",
            created_at=now
        )
        db.add(fault_alert)

    # Safety: Extreme temperature anomaly alert check (only when DHT11 measurement is valid)
    if dht_valid and telemetry.temperature_c is not None:
        if telemetry.temperature_c > 45.0 or telemetry.temperature_c < 0.0:
            alert = Alert(
                device_id=telemetry.device_id,
                zone_id=device.zone_id,
                severity="critical" if telemetry.temperature_c > 50.0 else "warning",
                category="sensor",
                title=f"Extreme Temperature Reading: {telemetry.temperature_c:.1f}°C",
                message=f"Device {telemetry.device_id} detected environmental temperature outside safe growth range.",
                created_at=now
            )
            db.add(alert)

    db.commit()
    db.refresh(reading)

    # Run automation rules evaluation
    evaluate_automation_rules(db, reading)

    return reading
