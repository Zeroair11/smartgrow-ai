import hashlib
from typing import List
from datetime import datetime, timezone
from fastapi import APIRouter, Depends, HTTPException, UploadFile, File, Form, status
from sqlalchemy.orm import Session
from backend.app.core.database import get_db
from backend.app.models.entities import FirmwareRelease, OTADeployment, Device, User
from backend.app.schemas.dtos import FirmwareReleaseRead, OTADeploymentRead
from backend.app.services.auth_service import get_current_user, require_admin
from backend.app.services.audit_service import log_audit

router = APIRouter(prefix="/ota", tags=["OTA Firmware Management"])

@router.get("/releases", response_model=List[FirmwareReleaseRead])
def list_firmware_releases(db: Session = Depends(get_db), current_user: User = Depends(get_current_user)):
    return db.query(FirmwareRelease).order_by(FirmwareRelease.created_at.desc()).all()

@router.post("/releases", response_model=FirmwareReleaseRead, status_code=status.HTTP_201_CREATED)
async def upload_firmware_release(
    version: str = Form(...),
    release_notes: str = Form(""),
    firmware_bin: UploadFile = File(...),
    db: Session = Depends(get_db),
    current_user: User = Depends(require_admin)
):
    existing = db.query(FirmwareRelease).filter(FirmwareRelease.version == version).first()
    if existing:
        raise HTTPException(status_code=400, detail=f"Firmware release version {version} already exists")

    content = await firmware_bin.read()
    checksum = hashlib.sha256(content).hexdigest()
    size_bytes = len(content)

    rel = FirmwareRelease(
        version=version,
        checksum_sha256=checksum,
        file_path=f"firmware_storage/{version}.bin",
        file_size_bytes=size_bytes,
        release_notes=release_notes,
        is_active=True,
        created_at=datetime.now(timezone.utc)
    )
    db.add(rel)
    db.commit()
    db.refresh(rel)
    log_audit(db, current_user.id, current_user.email, "UPLOAD_FIRMWARE_RELEASE", "FIRMWARE_RELEASE", str(rel.id), {"version": version, "sha256": checksum})
    return rel

@router.post("/deploy/{release_id}/{device_id}")
def deploy_ota_to_device(
    release_id: int,
    device_id: str,
    db: Session = Depends(get_db),
    current_user: User = Depends(require_admin)
):
    rel = db.query(FirmwareRelease).filter(FirmwareRelease.id == release_id).first()
    if not rel:
        raise HTTPException(status_code=404, detail="Release not found")
    dev = db.query(Device).filter(Device.device_id == device_id).first()
    if not dev:
        raise HTTPException(status_code=404, detail="Device not found")
    if dev.status in ["revoked", "decommissioned"]:
        raise HTTPException(status_code=403, detail="Cannot deploy firmware to a revoked or decommissioned device")
    
    deployment = OTADeployment(
        release_id=release_id,
        device_id=device_id,
        status="pending",
        progress_pct=0,
        created_at=datetime.now(timezone.utc),
        updated_at=datetime.now(timezone.utc)
    )
    db.add(deployment)
    db.commit()
    db.refresh(deployment)
    log_audit(db, current_user.id, current_user.email, "TRIGGER_OTA_DEPLOYMENT", "OTA_DEPLOYMENT", str(deployment.id), {"device_id": device_id, "version": rel.version})
    return {"message": "OTA deployment queued successfully", "deployment_id": deployment.id}

@router.get("/deployments")
def list_deployments(db: Session = Depends(get_db), current_user: User = Depends(get_current_user)):
    return db.query(OTADeployment).order_by(OTADeployment.created_at.desc()).all()
