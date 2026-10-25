import json
from datetime import datetime, timezone
from typing import Optional, Any
from sqlalchemy.orm import Session
from backend.app.models.entities import AuditLog

def log_audit(
    db: Session,
    user_id: Optional[int],
    user_email: Optional[str],
    action: str,
    resource_type: str,
    resource_id: Optional[str] = None,
    details: Optional[Any] = None,
    ip_address: Optional[str] = None
) -> AuditLog:
    details_str = json.dumps(details) if details else None
    audit = AuditLog(
        user_id=user_id,
        user_email=user_email,
        action=action,
        resource_type=resource_type,
        resource_id=str(resource_id) if resource_id is not None else None,
        details_json=details_str,
        ip_address=ip_address,
        created_at=datetime.now(timezone.utc)
    )
    db.add(audit)
    db.commit()
    return audit
