import secrets
from typing import List, Optional
from datetime import datetime, timezone
from fastapi import APIRouter, Depends, HTTPException, status, Header, Query
from sqlalchemy.orm import Session
from backend.app.core.config import settings
from backend.app.core.database import get_db
from backend.app.models.entities import Device, DeviceConfig, DeviceCommand, User
from backend.app.schemas.dtos import (
    DeviceRegister, DeviceRead, DeviceUpdate, DeviceConfigUpdate, DeviceConfigRead,
    CommandCreate, CommandRead, CommandAck
)
from backend.app.services.auth_service import get_current_user, require_operator, require_admin
from backend.app.services.command_service import create_command, get_pending_commands_for_device, acknowledge_command
from backend.app.services.audit_service import log_audit

router = APIRouter(prefix="/devices", tags=["Devices"])

@router.post("/register", response_model=DeviceRead, status_code=status.HTTP_201_CREATED)
def register_device(dev_in: DeviceRegister, db: Session = Depends(get_db)):
    if dev_in.registration_token != settings.DEVICE_REGISTRATION_TOKEN:
        raise HTTPException(status_code=401, detail="Invalid device enrollment token")

    existing = db.query(Device).filter(Device.device_id == dev_in.device_id).first()
    if existing:
        existing.name = dev_in.name
        existing.status = "online"
        existing.last_seen_at = datetime.now(timezone.utc)
        if dev_in.zone_id:
            existing.zone_id = dev_in.zone_id
        db.commit()
        db.refresh(existing)
        return existing

    device_token = secrets.token_hex(32)
    device = Device(
        device_id=dev_in.device_id,
        name=dev_in.name,
        zone_id=dev_in.zone_id,
        token=device_token,
        status="online",
        firmware_version=dev_in.firmware_version or "1.0.0",
        model_version=dev_in.model_version or "v1.0",
        mac_address=dev_in.mac_address,
        last_seen_at=datetime.now(timezone.utc),
        registered_at=datetime.now(timezone.utc)
    )
    db.add(device)

    # Create default config for device
    cfg = DeviceConfig(
        device_id=dev_in.device_id,
        telemetry_interval_s=5,
        safety_max_runtime_s=120,
        safety_min_cooldown_s=180,
        soil_dry_adc=3200,
        soil_wet_adc=1200,
        operating_mode="AUTO",
        is_synced=True
    )
    db.add(cfg)
    db.commit()
    db.refresh(device)
    return device

@router.get("", response_model=List[DeviceRead])
def list_devices(zone_id: Optional[int] = None, db: Session = Depends(get_db), current_user: User = Depends(get_current_user)):
    q = db.query(Device)
    if zone_id is not None:
        q = q.filter(Device.zone_id == zone_id)
    return q.order_by(Device.id.asc()).all()

@router.get("/{device_id}", response_model=DeviceRead)
def get_device(device_id: str, db: Session = Depends(get_db), current_user: User = Depends(get_current_user)):
    dev = db.query(Device).filter(Device.device_id == device_id).first()
    if not dev:
        raise HTTPException(status_code=404, detail="Device not found")
    return dev

@router.patch("/{device_id}", response_model=DeviceRead)
def update_device(
    device_id: str,
    dev_in: DeviceUpdate,
    db: Session = Depends(get_db),
    current_user: User = Depends(require_operator)
):
    dev = db.query(Device).filter(Device.device_id == device_id).first()
    if not dev:
        raise HTTPException(status_code=404, detail="Device not found")
    
    if dev_in.name is not None:
        dev.name = dev_in.name
    if dev_in.zone_id is not None:
        dev.zone_id = dev_in.zone_id if dev_in.zone_id > 0 else None
    if dev_in.status is not None:
        dev.status = dev_in.status

    db.commit()
    db.refresh(dev)
    log_audit(db, current_user.id, current_user.email, "UPDATE_DEVICE", "DEVICE", device_id, dev_in.model_dump(exclude_unset=True))
    return dev

@router.post("/{device_id}/revoke", response_model=DeviceRead)
def revoke_device(
    device_id: str,
    db: Session = Depends(get_db),
    current_user: User = Depends(require_admin)
):
    dev = db.query(Device).filter(Device.device_id == device_id).first()
    if not dev:
        raise HTTPException(status_code=404, detail="Device not found")
    
    # Invalidate token and mark status as revoked
    dev.token = secrets.token_hex(32)
    dev.status = "revoked"
    db.commit()
    db.refresh(dev)
    log_audit(db, current_user.id, current_user.email, "REVOKE_DEVICE_CREDENTIALS", "DEVICE", device_id)
    return dev

@router.delete("/{device_id}", status_code=status.HTTP_200_OK)
def delete_device(
    device_id: str,
    db: Session = Depends(get_db),
    current_user: User = Depends(require_admin)
):
    dev = db.query(Device).filter(Device.device_id == device_id).first()
    if not dev:
        raise HTTPException(status_code=404, detail="Device not found")
    
    # Cascade delete config if exists
    cfg = db.query(DeviceConfig).filter(DeviceConfig.device_id == device_id).first()
    if cfg:
        db.delete(cfg)
    
    db.delete(dev)
    db.commit()
    log_audit(db, current_user.id, current_user.email, "DELETE_DEVICE", "DEVICE", device_id)
    return {"message": f"Device {device_id} successfully removed"}

@router.put("/{device_id}/zone", response_model=DeviceRead)
def assign_device_zone(device_id: str, zone_id: Optional[int] = Query(None), db: Session = Depends(get_db), current_user: User = Depends(require_operator)):
    dev = db.query(Device).filter(Device.device_id == device_id).first()
    if not dev:
        raise HTTPException(status_code=404, detail="Device not found")
    dev.zone_id = zone_id
    db.commit()
    db.refresh(dev)
    log_audit(db, current_user.id, current_user.email, "ASSIGN_ZONE", "DEVICE", device_id, {"zone_id": zone_id})
    return dev

@router.get("/{device_id}/config", response_model=DeviceConfigRead)
def get_device_config(device_id: str, db: Session = Depends(get_db)):
    cfg = db.query(DeviceConfig).filter(DeviceConfig.device_id == device_id).first()
    if not cfg:
        raise HTTPException(status_code=404, detail="Device configuration not found")
    return cfg

@router.put("/{device_id}/config", response_model=DeviceConfigRead)
def update_device_config(device_id: str, cfg_in: DeviceConfigUpdate, db: Session = Depends(get_db), current_user: User = Depends(require_operator)):
    dev = db.query(Device).filter(Device.device_id == device_id).first()
    if not dev or dev.status in ["revoked", "decommissioned"]:
        raise HTTPException(status_code=403, detail="Device is not authorized to receive config updates")

    cfg = db.query(DeviceConfig).filter(DeviceConfig.device_id == device_id).first()
    if not cfg:
        raise HTTPException(status_code=404, detail="Device configuration not found")
    
    for field, val in cfg_in.model_dump(exclude_unset=True).items():
        setattr(cfg, field, val)
    cfg.is_synced = False
    db.commit()
    db.refresh(cfg)

    # Enqueue downlink CONFIG_UPDATE command to synchronize device
    mode_map = {"AUTO": 0, "MANUAL": 1, "RULE": 2, "HYBRID": 3, "SAFE": 4}
    target_mode_int = mode_map.get(cfg.operating_mode, 0)
    cmd_in = CommandCreate(
        command_type="CONFIG_UPDATE",
        param_val=float(target_mode_int),
        duration_s=cfg.safety_max_runtime_s,
        ttl_s=300
    )
    create_command(db, device_id, cmd_in)

    log_audit(db, current_user.id, current_user.email, "UPDATE_DEVICE_CONFIG", "DEVICE_CONFIG", device_id, cfg_in.model_dump(exclude_unset=True))
    return cfg

@router.post("/{device_id}/commands", response_model=CommandRead, status_code=status.HTTP_201_CREATED)
def send_device_command(device_id: str, cmd_in: CommandCreate, db: Session = Depends(get_db), current_user: User = Depends(require_operator)):
    dev = db.query(Device).filter(Device.device_id == device_id).first()
    if not dev or dev.status in ["revoked", "decommissioned"]:
        raise HTTPException(status_code=403, detail="Device is not authorized to receive commands")
    cmd = create_command(db, device_id, cmd_in)
    log_audit(db, current_user.id, current_user.email, "SEND_COMMAND", "DEVICE_COMMAND", cmd.command_id, {"type": cmd.command_type})
    return cmd

@router.get("/{device_id}/commands", response_model=List[CommandRead])
def get_device_command_history(device_id: str, db: Session = Depends(get_db), current_user: User = Depends(get_current_user)):
    return db.query(DeviceCommand).filter(DeviceCommand.device_id == device_id).order_by(DeviceCommand.created_at.desc()).limit(50).all()

@router.get("/{device_id}/poll-commands", response_model=List[CommandRead])
def poll_pending_commands(device_id: str, db: Session = Depends(get_db)):
    """Called by firmware to fetch pending commands"""
    dev = db.query(Device).filter(Device.device_id == device_id).first()
    if not dev or dev.status in ["revoked", "decommissioned"]:
        raise HTTPException(status_code=403, detail="Device registration is revoked or decommissioned")
    return get_pending_commands_for_device(db, device_id)

@router.post("/{device_id}/commands/ack", response_model=CommandRead)
def ack_device_command(device_id: str, ack: CommandAck, db: Session = Depends(get_db)):
    """Called by firmware to acknowledge command execution and provide result"""
    return acknowledge_command(db, device_id, ack)

@router.post("/{device_id}/restart", response_model=CommandRead)
def restart_device(device_id: str, db: Session = Depends(get_db), current_user: User = Depends(require_operator)):
    cmd_in = CommandCreate(command_type="SYSTEM_RESTART", param_val=0, ttl_s=60)
    cmd = create_command(db, device_id, cmd_in)
    log_audit(db, current_user.id, current_user.email, "RESTART_DEVICE", "DEVICE", device_id)
    return cmd
