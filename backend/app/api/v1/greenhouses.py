from typing import List
from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy.orm import Session
from backend.app.core.database import get_db
from backend.app.models.entities import Greenhouse, Zone, User
from backend.app.schemas.dtos import GreenhouseCreate, GreenhouseUpdate, GreenhouseRead
from backend.app.services.auth_service import get_current_user, require_operator
from backend.app.services.audit_service import log_audit

router = APIRouter(prefix="/greenhouses", tags=["Greenhouses"])

@router.get("", response_model=List[GreenhouseRead])
def list_greenhouses(db: Session = Depends(get_db), current_user: User = Depends(get_current_user)):
    return db.query(Greenhouse).order_by(Greenhouse.id.asc()).all()

@router.post("", response_model=GreenhouseRead, status_code=status.HTTP_201_CREATED)
def create_greenhouse(gh_in: GreenhouseCreate, db: Session = Depends(get_db), current_user: User = Depends(require_operator)):
    existing = db.query(Greenhouse).filter(Greenhouse.code == gh_in.code).first()
    if existing:
        raise HTTPException(status_code=400, detail="Greenhouse code already exists")
    
    gh = Greenhouse(**gh_in.model_dump())
    db.add(gh)
    db.commit()
    db.refresh(gh)
    log_audit(db, current_user.id, current_user.email, "CREATE_GREENHOUSE", "GREENHOUSE", str(gh.id), {"code": gh.code})
    return gh

@router.get("/{greenhouse_id}", response_model=GreenhouseRead)
def get_greenhouse(greenhouse_id: int, db: Session = Depends(get_db), current_user: User = Depends(get_current_user)):
    gh = db.query(Greenhouse).filter(Greenhouse.id == greenhouse_id).first()
    if not gh:
        raise HTTPException(status_code=404, detail="Greenhouse not found")
    return gh

@router.put("/{greenhouse_id}", response_model=GreenhouseRead)
def update_greenhouse(greenhouse_id: int, gh_in: GreenhouseUpdate, db: Session = Depends(get_db), current_user: User = Depends(require_operator)):
    gh = db.query(Greenhouse).filter(Greenhouse.id == greenhouse_id).first()
    if not gh:
        raise HTTPException(status_code=404, detail="Greenhouse not found")
    
    for field, val in gh_in.dict(exclude_unset=True).items():
        setattr(gh, field, val)
    
    db.commit()
    db.refresh(gh)
    log_audit(db, current_user.id, current_user.email, "UPDATE_GREENHOUSE", "GREENHOUSE", str(gh.id), gh_in.dict(exclude_unset=True))
    return gh

@router.delete("/{greenhouse_id}", status_code=status.HTTP_204_NO_CONTENT)
def delete_greenhouse(greenhouse_id: int, db: Session = Depends(get_db), current_user: User = Depends(require_operator)):
    gh = db.query(Greenhouse).filter(Greenhouse.id == greenhouse_id).first()
    if not gh:
        raise HTTPException(status_code=404, detail="Greenhouse not found")
    
    db.delete(gh)
    db.commit()
    log_audit(db, current_user.id, current_user.email, "DELETE_GREENHOUSE", "GREENHOUSE", str(greenhouse_id))
    return None
