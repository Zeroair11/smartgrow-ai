import io
import csv
from typing import List, Optional
from datetime import datetime
from fastapi import APIRouter, Depends, HTTPException, status, Query
from fastapi.responses import StreamingResponse
from sqlalchemy.orm import Session
from backend.app.core.database import get_db
from backend.app.models.entities import SensorReading, User
from backend.app.schemas.dtos import TelemetryIngest, SensorReadingRead
from backend.app.services.telemetry_service import ingest_telemetry
from backend.app.services.auth_service import get_current_user

router = APIRouter(prefix="/telemetry", tags=["Telemetry"])

@router.post("", response_model=SensorReadingRead, status_code=status.HTTP_201_CREATED)
def post_telemetry(telemetry: TelemetryIngest, db: Session = Depends(get_db)):
    """Primary telemetry ingest endpoint called by ESP32-S3 firmware"""
    return ingest_telemetry(db, telemetry)

@router.get("/history", response_model=List[SensorReadingRead])
def get_telemetry_history(
    device_id: Optional[str] = None,
    zone_id: Optional[int] = None,
    limit: int = Query(default=100, le=1000),
    db: Session = Depends(get_db),
    current_user: User = Depends(get_current_user)
):
    q = db.query(SensorReading)
    if device_id:
        q = q.filter(SensorReading.device_id == device_id)
    if zone_id is not None:
        q = q.filter(SensorReading.zone_id == zone_id)
    
    return q.order_by(SensorReading.recorded_at.desc()).limit(limit).all()

@router.get("/latest", response_model=Optional[SensorReadingRead])
def get_latest_telemetry(
    device_id: Optional[str] = None,
    zone_id: Optional[int] = None,
    db: Session = Depends(get_db),
    current_user: User = Depends(get_current_user)
):
    q = db.query(SensorReading)
    if device_id:
        q = q.filter(SensorReading.device_id == device_id)
    if zone_id is not None:
        q = q.filter(SensorReading.zone_id == zone_id)
    
    return q.order_by(SensorReading.recorded_at.desc()).first()

@router.get("/export-csv")
def export_telemetry_csv(
    device_id: Optional[str] = None,
    limit: int = Query(default=1000, le=5000),
    db: Session = Depends(get_db),
    current_user: User = Depends(get_current_user)
):
    q = db.query(SensorReading)
    if device_id:
        q = q.filter(SensorReading.device_id == device_id)
    readings = q.order_by(SensorReading.recorded_at.desc()).limit(limit).all()

    output = io.StringIO()
    writer = csv.writer(output)
    writer.writerow([
        "recorded_at", "device_id", "sequence", "soil_moisture_pct",
        "temperature_c", "humidity_pct", "light_level_pct",
        "ai_class", "ai_confidence", "pump_state", "operating_mode"
    ])
    for r in readings:
        writer.writerow([
            r.recorded_at.isoformat() if r.recorded_at else "",
            r.device_id,
            r.sequence,
            f"{r.soil_moisture_pct:.2f}",
            f"{r.temperature_c:.2f}",
            f"{r.humidity_pct:.2f}",
            f"{r.light_level_pct:.2f}",
            r.ai_class,
            f"{r.ai_confidence:.3f}",
            r.pump_state,
            r.operating_mode
        ])
    output.seek(0)
    return StreamingResponse(
        output,
        media_type="text/csv",
        headers={"Content-Disposition": "attachment; filename=smartgrow_telemetry.csv"}
    )
