from typing import List
from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy.orm import Session
from backend.app.core.database import get_db
from backend.app.models.entities import Zone, Greenhouse, User
from backend.app.schemas.dtos import ZoneCreate, ZoneUpdate, ZoneRead
from backend.app.services.auth_service import get_current_user, require_operator
from backend.app.services.audit_service import log_audit

router = APIRouter(prefix="/zones", tags=["Zones"])

@router.get("", response_model=List[ZoneRead])
def list_zones(greenhouse_id: int = None, db: Session = Depends(get_db), current_user: User = Depends(get_current_user)):
    q = db.query(Zone)
    if greenhouse_id is not None:
        q = q.filter(Zone.greenhouse_id == greenhouse_id)
    return q.order_by(Zone.id.asc()).all()

@router.post("", response_model=ZoneRead, status_code=status.HTTP_201_CREATED)
def create_zone(zone_in: ZoneCreate, db: Session = Depends(get_db), current_user: User = Depends(require_operator)):
    gh = db.query(Greenhouse).filter(Greenhouse.id == zone_in.greenhouse_id).first()
    if not gh:
        raise HTTPException(status_code=404, detail="Parent greenhouse not found")
    
    zone = Zone(**zone_in.model_dump())
    db.add(zone)
    db.commit()
    db.refresh(zone)
    log_audit(db, current_user.id, current_user.email, "CREATE_ZONE", "ZONE", str(zone.id), {"code": zone.code})
    return zone

@router.get("/{zone_id}", response_model=ZoneRead)
def get_zone(zone_id: int, db: Session = Depends(get_db), current_user: User = Depends(get_current_user)):
    zone = db.query(Zone).filter(Zone.id == zone_id).first()
    if not zone:
        raise HTTPException(status_code=404, detail="Zone not found")
    return zone

@router.put("/{zone_id}", response_model=ZoneRead)
def update_zone(zone_id: int, zone_in: ZoneUpdate, db: Session = Depends(get_db), current_user: User = Depends(require_operator)):
    zone = db.query(Zone).filter(Zone.id == zone_id).first()
    if not zone:
        raise HTTPException(status_code=404, detail="Zone not found")
    
    for field, val in zone_in.dict(exclude_unset=True).items():
        setattr(zone, field, val)
    
    db.commit()
    db.refresh(zone)
    log_audit(db, current_user.id, current_user.email, "UPDATE_ZONE", "ZONE", str(zone.id), zone_in.dict(exclude_unset=True))
    return zone

@router.delete("/{zone_id}", status_code=status.HTTP_204_NO_CONTENT)
def delete_zone(zone_id: int, db: Session = Depends(get_db), current_user: User = Depends(require_operator)):
    zone = db.query(Zone).filter(Zone.id == zone_id).first()
    if not zone:
        raise HTTPException(status_code=404, detail="Zone not found")
    
    db.delete(zone)
    db.commit()
    log_audit(db, current_user.id, current_user.email, "DELETE_ZONE", "ZONE", str(zone_id))
    return None
