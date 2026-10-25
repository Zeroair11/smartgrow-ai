import uuid
import json
from datetime import datetime, timezone, timedelta
from typing import Optional, List
from sqlalchemy.orm import Session
from fastapi import HTTPException
from backend.app.models.entities import DeviceCommand, Device
from backend.app.schemas.dtos import CommandCreate, CommandAck

def create_command(db: Session, device_id: str, cmd_in: CommandCreate) -> DeviceCommand:
    device = db.query(Device).filter(Device.device_id == device_id).first()
    if not device:
        raise HTTPException(status_code=404, detail=f"Device {device_id} not found")

    now = datetime.now(timezone.utc)
    expires_at = now + timedelta(seconds=cmd_in.ttl_s if cmd_in.ttl_s else 300)
    cmd_id = f"cmd-{uuid.uuid4().hex[:12]}"

    payload = {
        "command_type": cmd_in.command_type,
        "param_val": cmd_in.param_val,
        "duration_s": cmd_in.duration_s
    }

    command = DeviceCommand(
        command_id=cmd_id,
        device_id=device_id,
        command_type=cmd_in.command_type,
        payload_json=json.dumps(payload),
        status="pending",
        created_at=now,
        expires_at=expires_at
    )
    db.add(command)
    db.commit()
    db.refresh(command)
    return command

def get_pending_commands_for_device(db: Session, device_id: str) -> List[DeviceCommand]:
    now = datetime.now(timezone.utc)
    # Mark expired commands first
    expired = db.query(DeviceCommand).filter(
        DeviceCommand.device_id == device_id,
        DeviceCommand.status == "pending",
        DeviceCommand.expires_at < now
    ).all()
    for exp_cmd in expired:
        exp_cmd.status = "expired"
        exp_cmd.error_code = 101
        exp_cmd.result_json = json.dumps({"error": "Command expired before device poll"})
    if expired:
        db.commit()

    return db.query(DeviceCommand).filter(
        DeviceCommand.device_id == device_id,
        DeviceCommand.status == "pending"
    ).order_by(DeviceCommand.created_at.asc()).all()

def acknowledge_command(db: Session, device_id: str, ack: CommandAck) -> DeviceCommand:
    cmd = db.query(DeviceCommand).filter(
        DeviceCommand.command_id == ack.command_id,
        DeviceCommand.device_id == device_id
    ).first()

    if not cmd:
        raise HTTPException(status_code=404, detail="Command not found for this device")

    now = datetime.now(timezone.utc)
    cmd.status = ack.status
    if not cmd.acknowledged_at:
        cmd.acknowledged_at = now
    
    if ack.status in ["completed", "failed", "rejected"]:
        cmd.completed_at = now
    
    cmd.error_code = ack.error_code
    cmd.result_json = json.dumps({"message": ack.result_message or ""})
    db.commit()
    db.refresh(cmd)
    return cmd
