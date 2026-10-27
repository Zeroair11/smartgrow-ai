import io
import csv
from typing import List, Optional
from datetime import datetime, timezone
from fastapi import APIRouter, Depends, HTTPException, status, Query, Response
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
        rec_at = ""
        if r.recorded_at:
            if r.recorded_at.tzinfo is None:
                rec_at = r.recorded_at.replace(tzinfo=timezone.utc).isoformat()
            else:
                rec_at = r.recorded_at.astimezone(timezone.utc).isoformat()
        writer.writerow([
            rec_at,
            r.device_id,
            r.sequence,
            f"{r.soil_moisture_pct:.2f}" if r.soil_moisture_pct is not None else "",
            f"{r.temperature_c:.2f}" if r.temperature_c is not None else "",
            f"{r.humidity_pct:.2f}" if r.humidity_pct is not None else "",
            f"{r.light_level_pct:.2f}" if r.light_level_pct is not None else "",
            r.ai_class if r.ai_class is not None else "",
            f"{r.ai_confidence:.3f}" if r.ai_confidence is not None else "",
            r.pump_state,
            r.operating_mode
        ])
    
    # UTF-8 with BOM (\ufeff) ensures Excel correctly displays Unicode characters without manual import
    csv_bytes = ("\ufeff" + output.getvalue()).encode("utf-8")
    timestamp_str = datetime.now(timezone.utc).strftime("%Y%m%d_%H%M%S")
    filename = f"smartgrow_telemetry_{timestamp_str}.csv"

    return Response(
        content=csv_bytes,
        media_type="text/csv; charset=utf-8",
        headers={
            "Content-Disposition": f'attachment; filename="{filename}"',
            "Access-Control-Expose-Headers": "Content-Disposition"
        }
    )
