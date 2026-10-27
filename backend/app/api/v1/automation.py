from typing import List, Optional
from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy.orm import Session
from backend.app.core.database import get_db
from backend.app.models.entities import AutomationRule, AutomationRuleEvent, User
from backend.app.schemas.dtos import AutomationRuleCreate, AutomationRuleRead
from backend.app.services.auth_service import get_current_user, require_operator
from backend.app.services.audit_service import log_audit

router = APIRouter(prefix="/automation", tags=["Automation"])

@router.get("/rules", response_model=List[AutomationRuleRead])
def list_rules(zone_id: Optional[int] = None, db: Session = Depends(get_db), current_user: User = Depends(get_current_user)):
    q = db.query(AutomationRule)
    if zone_id is not None:
        q = q.filter(AutomationRule.zone_id == zone_id)
    return q.order_by(AutomationRule.id.asc()).all()

@router.post("/rules", response_model=AutomationRuleRead, status_code=status.HTTP_201_CREATED)
def create_rule(rule_in: AutomationRuleCreate, db: Session = Depends(get_db), current_user: User = Depends(require_operator)):
    rule = AutomationRule(**rule_in.model_dump())
    db.add(rule)
    db.commit()
    db.refresh(rule)
    log_audit(db, current_user.id, current_user.email, "CREATE_AUTOMATION_RULE", "AUTOMATION_RULE", str(rule.id), {"name": rule.name})
    return rule

@router.put("/rules/{rule_id}", response_model=AutomationRuleRead)
def update_rule(rule_id: int, rule_in: AutomationRuleCreate, db: Session = Depends(get_db), current_user: User = Depends(require_operator)):
    rule = db.query(AutomationRule).filter(AutomationRule.id == rule_id).first()
    if not rule:
        raise HTTPException(status_code=404, detail="Rule not found")
    for k, v in rule_in.model_dump().items():
        setattr(rule, k, v)
    db.commit()
    db.refresh(rule)
    log_audit(db, current_user.id, current_user.email, "UPDATE_AUTOMATION_RULE", "AUTOMATION_RULE", str(rule.id), rule_in.model_dump())
    return rule

@router.delete("/rules/{rule_id}", status_code=status.HTTP_204_NO_CONTENT)
def delete_rule(rule_id: int, db: Session = Depends(get_db), current_user: User = Depends(require_operator)):
    rule = db.query(AutomationRule).filter(AutomationRule.id == rule_id).first()
    if not rule:
        raise HTTPException(status_code=404, detail="Rule not found")
    db.delete(rule)
    db.commit()
    log_audit(db, current_user.id, current_user.email, "DELETE_AUTOMATION_RULE", "AUTOMATION_RULE", str(rule_id))
    return None

@router.post("/rules/{rule_id}/toggle", response_model=AutomationRuleRead)
def toggle_rule(rule_id: int, db: Session = Depends(get_db), current_user: User = Depends(require_operator)):
    rule = db.query(AutomationRule).filter(AutomationRule.id == rule_id).first()
    if not rule:
        raise HTTPException(status_code=404, detail="Rule not found")
    rule.is_active = not rule.is_active
    db.commit()
    db.refresh(rule)
    log_audit(db, current_user.id, current_user.email, "TOGGLE_AUTOMATION_RULE", "AUTOMATION_RULE", str(rule.id), {"active": rule.is_active})
    return rule

@router.get("/events")
def list_rule_events(limit: int = 50, db: Session = Depends(get_db), current_user: User = Depends(get_current_user)):
    return db.query(AutomationRuleEvent).order_by(AutomationRuleEvent.triggered_at.desc()).limit(limit).all()
