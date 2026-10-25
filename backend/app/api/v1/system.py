import os
from typing import List, Optional
from datetime import datetime, timezone
from fastapi import APIRouter, Depends, Query
from sqlalchemy.orm import Session
from backend.app.core.database import get_db
from backend.app.core.config import settings
from backend.app.models.entities import AuditLog, User
from backend.app.schemas.dtos import AuditLogRead
from backend.app.services.auth_service import get_current_user, require_admin

router = APIRouter(prefix="/system", tags=["System & Settings"])

@router.get("/status")
def get_system_status(db: Session = Depends(get_db)):
    return {
        "app_name": settings.APP_NAME,
        "environment": settings.APP_ENV,
        "status": "healthy",
        "api_version": "1.0.0",
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "database": "connected"
    }

@router.get("/audit-logs", response_model=List[AuditLogRead])
def get_audit_logs(
    limit: int = Query(default=100, le=500),
    db: Session = Depends(get_db),
    current_user: User = Depends(require_admin)
):
    return db.query(AuditLog).order_by(AuditLog.created_at.desc()).limit(limit).all()

@router.get("/settings")
def get_system_settings(db: Session = Depends(get_db), current_user: User = Depends(get_current_user)):
    return {
        "timezone": "UTC+07:00 (Asia/Ho_Chi_Minh)",
        "units": {
            "temperature": "°C",
            "humidity": "%",
            "soil_moisture": "%",
            "light": "%"
        },
        "safety_guardrails": {
            "max_pump_duration_seconds": 120,
            "min_pump_cooldown_seconds": 180,
            "offline_failsafe_timeout_seconds": 300
        },
        "sensor_defaults": {
            "soil_dry_adc": 3200,
            "soil_wet_adc": 1200
        }
    }
