from typing import List, Optional
from datetime import datetime, timezone
from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy.orm import Session
from backend.app.core.database import get_db
from backend.app.models.entities import Alert, User
from backend.app.schemas.dtos import AlertRead
from backend.app.services.auth_service import get_current_user, require_operator
from backend.app.services.audit_service import log_audit

router = APIRouter(prefix="/alerts", tags=["Alerts"])

@router.get("", response_model=List[AlertRead])
def list_alerts(
    severity: Optional[str] = None,
    is_resolved: Optional[bool] = None,
    limit: int = 50,
    db: Session = Depends(get_db),
    current_user: User = Depends(get_current_user)
):
    q = db.query(Alert)
    if severity:
        q = q.filter(Alert.severity == severity)
    if is_resolved is not None:
        q = q.filter(Alert.is_resolved == is_resolved)
    return q.order_by(Alert.created_at.desc()).limit(limit).all()

@router.post("/{alert_id}/acknowledge", response_model=AlertRead)
def acknowledge_alert(alert_id: int, db: Session = Depends(get_db), current_user: User = Depends(require_operator)):
    alert = db.query(Alert).filter(Alert.id == alert_id).first()
    if not alert:
        raise HTTPException(status_code=404, detail="Alert not found")
    alert.is_acknowledged = True
    alert.acknowledged_at = datetime.now(timezone.utc)
    db.commit()
    db.refresh(alert)
    log_audit(db, current_user.id, current_user.email, "ACKNOWLEDGE_ALERT", "ALERT", str(alert_id))
    return alert

@router.post("/{alert_id}/resolve", response_model=AlertRead)
def resolve_alert(alert_id: int, db: Session = Depends(get_db), current_user: User = Depends(require_operator)):
    alert = db.query(Alert).filter(Alert.id == alert_id).first()
    if not alert:
        raise HTTPException(status_code=404, detail="Alert not found")
    alert.is_resolved = True
    alert.resolved_at = datetime.now(timezone.utc)
    alert.resolved_by_user_id = current_user.id
    db.commit()
    db.refresh(alert)
    log_audit(db, current_user.id, current_user.email, "RESOLVE_ALERT", "ALERT", str(alert_id))
    return alert
