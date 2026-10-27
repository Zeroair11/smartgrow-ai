from typing import List, Optional
from datetime import datetime, timezone
from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy.orm import Session
from backend.app.core.database import get_db
from backend.app.models.entities import IrrigationEvent, IrrigationSchedule, Device, User
from backend.app.schemas.dtos import (
    IrrigationManualStart, IrrigationManualStop, IrrigationEventRead,
    IrrigationScheduleCreate, IrrigationScheduleRead, CommandCreate, CommandRead
)
from backend.app.services.auth_service import get_current_user, require_operator
from backend.app.services.command_service import create_command
from backend.app.services.audit_service import log_audit

router = APIRouter(prefix="/irrigation", tags=["Irrigation"])

@router.post("/start", response_model=CommandRead)
def manual_start_irrigation(
    req: IrrigationManualStart,
    db: Session = Depends(get_db),
    current_user: User = Depends(require_operator)
):
    device = db.query(Device).filter(Device.device_id == req.device_id).first()
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")
    
    # Safety Check: Enforce maximum continuous watering duration (120 seconds)
    if req.duration_seconds > 120:
        raise HTTPException(status_code=400, detail="Requested duration exceeds safety maximum (120 seconds)")
    
    now = datetime.now(timezone.utc)
    # Check if device is offline
    if device.status == "offline":
        raise HTTPException(status_code=400, detail="Cannot start irrigation: Target device is currently offline")

    # Create command
    cmd_in = CommandCreate(
        command_type="PUMP_START",
        param_val=req.duration_seconds,
        duration_s=req.duration_seconds,
        ttl_s=180
    )
    cmd = create_command(db, req.device_id, cmd_in)

    # Record irrigation event
    event = IrrigationEvent(
        device_id=req.device_id,
        zone_id=device.zone_id,
        start_reason="MANUAL",
        stop_reason="IN_PROGRESS",
        started_at=now,
        duration_seconds=req.duration_seconds,
        volume_liters_est=round(req.duration_seconds * 0.05, 2), # approx 3 L/min = 0.05 L/s
        status="in_progress"
    )
    db.add(event)
    db.commit()

    log_audit(db, current_user.id, current_user.email, "MANUAL_IRRIGATION_START", "DEVICE", req.device_id, {"duration_s": req.duration_seconds})
    return cmd

@router.post("/stop", response_model=CommandRead)
def manual_stop_irrigation(
    req: IrrigationManualStop,
    db: Session = Depends(get_db),
    current_user: User = Depends(require_operator)
):
    device = db.query(Device).filter(Device.device_id == req.device_id).first()
    if not device:
        raise HTTPException(status_code=404, detail="Device not found")
    
    cmd_in = CommandCreate(command_type="PUMP_STOP", param_val=0, ttl_s=60)
    cmd = create_command(db, req.device_id, cmd_in)

    now = datetime.now(timezone.utc)
    active_event = db.query(IrrigationEvent).filter(
        IrrigationEvent.device_id == req.device_id,
        IrrigationEvent.status == "in_progress"
    ).order_by(IrrigationEvent.started_at.desc()).first()
    
    if active_event:
        active_event.status = "completed"
        active_event.stop_reason = "MANUAL_STOP"
        active_event.stopped_at = now
        started = active_event.started_at
        if started.tzinfo is None:
            started = started.replace(tzinfo=timezone.utc)
        active_event.duration_seconds = int((now - started).total_seconds())
        active_event.volume_liters_est = round(active_event.duration_seconds * 0.05, 2)
        db.commit()

    log_audit(db, current_user.id, current_user.email, "MANUAL_IRRIGATION_STOP", "DEVICE", req.device_id)
    return cmd

@router.get("/events", response_model=List[IrrigationEventRead])
def get_irrigation_events(
    device_id: Optional[str] = None,
    limit: int = 50,
    db: Session = Depends(get_db),
    current_user: User = Depends(get_current_user)
):
    q = db.query(IrrigationEvent)
    if device_id:
        q = q.filter(IrrigationEvent.device_id == device_id)
    return q.order_by(IrrigationEvent.started_at.desc()).limit(limit).all()

@router.get("/schedules", response_model=List[IrrigationScheduleRead])
def get_irrigation_schedules(db: Session = Depends(get_db), current_user: User = Depends(get_current_user)):
    return db.query(IrrigationSchedule).order_by(IrrigationSchedule.id.asc()).all()

@router.post("/schedules", response_model=IrrigationScheduleRead, status_code=status.HTTP_201_CREATED)
def create_irrigation_schedule(
    sch_in: IrrigationScheduleCreate,
    db: Session = Depends(get_db),
    current_user: User = Depends(require_operator)
):
    sch = IrrigationSchedule(**sch_in.model_dump())
    db.add(sch)
    db.commit()
    db.refresh(sch)
    log_audit(db, current_user.id, current_user.email, "CREATE_IRRIGATION_SCHEDULE", "SCHEDULE", str(sch.id), sch_in.model_dump())
    return sch

@router.delete("/schedules/{schedule_id}", status_code=status.HTTP_204_NO_CONTENT)
def delete_irrigation_schedule(
    schedule_id: int,
    db: Session = Depends(get_db),
    current_user: User = Depends(require_operator)
):
    sch = db.query(IrrigationSchedule).filter(IrrigationSchedule.id == schedule_id).first()
    if not sch:
        raise HTTPException(status_code=404, detail="Schedule not found")
    db.delete(sch)
    db.commit()
    log_audit(db, current_user.id, current_user.email, "DELETE_IRRIGATION_SCHEDULE", "SCHEDULE", str(schedule_id))
    return None
