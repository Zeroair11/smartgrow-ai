from datetime import datetime, timezone, timedelta
from typing import Dict, Any, List
from fastapi import APIRouter, Depends
from sqlalchemy.orm import Session
from sqlalchemy import func
from backend.app.core.database import get_db
from backend.app.models.entities import (
    Greenhouse, Zone, Device, SensorReading, Alert, IrrigationEvent, User
)
from backend.app.schemas.dtos import DashboardSummary, SensorReadingRead
from backend.app.services.auth_service import get_current_user

router = APIRouter(prefix="/analytics", tags=["Analytics"])

@router.get("/dashboard-summary", response_model=DashboardSummary)
def get_dashboard_summary(db: Session = Depends(get_db), current_user: User = Depends(get_current_user)):
    total_greenhouses = db.query(Greenhouse).count()
    total_zones = db.query(Zone).count()
    total_devices = db.query(Device).count()
    online_devices = db.query(Device).filter(Device.status == "online").count()
    offline_devices = total_devices - online_devices

    latest_reading = db.query(SensorReading).order_by(SensorReading.recorded_at.desc()).first()
    pump_active_count = 1 if (latest_reading and latest_reading.pump_state) else 0

    open_alerts_count = db.query(Alert).filter(Alert.is_resolved == False).count()

    today_start = datetime.now(timezone.utc).replace(hour=0, minute=0, second=0, microsecond=0)
    today_irrigations = db.query(IrrigationEvent).filter(IrrigationEvent.started_at >= today_start).all()
    today_irrigation_cycles = len(today_irrigations)
    today_water_duration_seconds = sum(e.duration_seconds for e in today_irrigations)

    return DashboardSummary(
        total_greenhouses=total_greenhouses,
        total_zones=total_zones,
        total_devices=total_devices,
        online_devices=online_devices,
        offline_devices=offline_devices,
        pump_active_count=pump_active_count,
        open_alerts_count=open_alerts_count,
        today_irrigation_cycles=today_irrigation_cycles,
        today_water_duration_seconds=today_water_duration_seconds,
        latest_reading=latest_reading
    )

@router.get("/trends")
def get_environmental_trends(hours: int = 24, db: Session = Depends(get_db), current_user: User = Depends(get_current_user)):
    cutoff = datetime.now(timezone.utc) - timedelta(hours=hours)
    readings = db.query(SensorReading).filter(
        SensorReading.recorded_at >= cutoff
    ).order_by(SensorReading.recorded_at.asc()).limit(500).all()

    points = []
    for r in readings:
        points.append({
            "timestamp": r.recorded_at.isoformat() if r.recorded_at else "",
            "soil_moisture_pct": r.soil_moisture_pct,
            "temperature_c": r.temperature_c,
            "humidity_pct": r.humidity_pct,
            "light_level_pct": r.light_level_pct,
            "pump_state": r.pump_state
        })
    return {"data": points}

@router.get("/device-health")
def get_devices_health(db: Session = Depends(get_db), current_user: User = Depends(get_current_user)):
    devices = db.query(Device).all()
    res = []
    for d in devices:
        readings_count = db.query(SensorReading).filter(SensorReading.device_id == d.device_id).count()
        alerts_count = db.query(Alert).filter(Alert.device_id == d.device_id, Alert.is_resolved == False).count()
        now = datetime.now(timezone.utc)
        if d.status == "online" and d.last_seen_at:
            last_seen = d.last_seen_at if d.last_seen_at.tzinfo else d.last_seen_at.replace(tzinfo=timezone.utc)
            seconds_ago = max(0.0, (now - last_seen).total_seconds())
            uptime = max(0.0, round(100.0 - min(100.0, (seconds_ago / 300.0) * 100.0), 1))
        else:
            uptime = 0.0

        res.append({
            "device_id": d.device_id,
            "name": d.name,
            "status": d.status,
            "firmware_version": d.firmware_version,
            "model_version": d.model_version,
            "last_seen_at": d.last_seen_at.isoformat() if d.last_seen_at else None,
            "total_telemetry_packets": readings_count,
            "active_alerts": alerts_count,
            "uptime_pct": uptime
        })
    return res
