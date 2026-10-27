import uuid
import json
from datetime import datetime, timezone, timedelta
from typing import Optional, List
from sqlalchemy.orm import Session
from fastapi import HTTPException
from backend.app.models.entities import DeviceCommand, Device, DeviceConfig
from backend.app.schemas.dtos import CommandCreate, CommandAck

VALID_STATUS_TRANSITIONS = {
    "pending": {"acknowledged", "executing", "completed", "failed", "rejected", "expired"},
    "acknowledged": {"executing", "completed", "failed", "rejected", "expired"},
    "executing": {"completed", "failed", "rejected"},
    "completed": set(),
    "failed": set(),
    "rejected": set(),
    "expired": set()
}

def to_utc(dt: Optional[datetime]) -> Optional[datetime]:
    if dt is None:
        return None
    if dt.tzinfo is None:
        return dt.replace(tzinfo=timezone.utc)
    return dt

def create_command(db: Session, device_id: str, cmd_in: CommandCreate) -> DeviceCommand:
    device = db.query(Device).filter(Device.device_id == device_id).first()
    if not device:
        raise HTTPException(status_code=404, detail=f"Device {device_id} not found")

    # Global Safety Rule: Maximum continuous pump run time is 120 seconds
    if cmd_in.command_type == "PUMP_START":
        duration = cmd_in.duration_s if cmd_in.duration_s is not None else int(cmd_in.param_val or 0)
        if duration > 120:
            raise HTTPException(
                status_code=400,
                detail="Requested pump duration exceeds maximum hardware safety limit (120 seconds)"
            )

    now = datetime.now(timezone.utc)

    # Idempotency check: If an idempotency key was supplied, check for active unexpired command
    if cmd_in.idempotency_key:
        candidates = db.query(DeviceCommand).filter(
            DeviceCommand.device_id == device_id,
            DeviceCommand.idempotency_key == cmd_in.idempotency_key
        ).all()
        for cand in candidates:
            cand_exp = to_utc(cand.expires_at)
            if cand_exp and cand_exp > now:
                return cand

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
        idempotency_key=cmd_in.idempotency_key,
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
    # Sweep and mark expired commands
    pending_cmds = db.query(DeviceCommand).filter(
        DeviceCommand.device_id == device_id,
        DeviceCommand.status == "pending"
    ).all()
    has_expired = False
    valid_pending = []
    for cmd in pending_cmds:
        cmd_exp = to_utc(cmd.expires_at)
        if cmd_exp and cmd_exp < now:
            cmd.status = "expired"
            cmd.error_code = 101
            cmd.result_json = json.dumps({"error": "Command expired before device poll"})
            has_expired = True
        else:
            valid_pending.append(cmd)

    if has_expired:
        db.commit()

    return valid_pending

def acknowledge_command(db: Session, device_id: str, ack: CommandAck) -> DeviceCommand:
    cmd = db.query(DeviceCommand).filter(
        DeviceCommand.command_id == ack.command_id,
        DeviceCommand.device_id == device_id
    ).first()

    if not cmd:
        raise HTTPException(status_code=404, detail="Command not found for this device")

    now = datetime.now(timezone.utc)
    cmd_exp = to_utc(cmd.expires_at)

    # Check for TTL expiry on acknowledge
    if cmd.status == "pending" and cmd_exp and cmd_exp < now:
        cmd.status = "expired"
        cmd.error_code = 101
        cmd.result_json = json.dumps({"error": "Command expired"})
        db.commit()
        db.refresh(cmd)
        raise HTTPException(status_code=400, detail="Command has expired and cannot be executed")

    # Strict state transition verification
    allowed = VALID_STATUS_TRANSITIONS.get(cmd.status, set())
    if ack.status not in allowed:
        raise HTTPException(
            status_code=400,
            detail=f"Invalid command state transition: cannot move from '{cmd.status}' to '{ack.status}'"
        )

    cmd.status = ack.status
    if not cmd.acknowledged_at:
        cmd.acknowledged_at = now
    
    if ack.status in ["completed", "failed", "rejected"]:
        cmd.completed_at = now
        # Update config sync status if command was a CONFIG_UPDATE
        if cmd.command_type == "CONFIG_UPDATE":
            dev_cfg = db.query(DeviceConfig).filter(DeviceConfig.device_id == device_id).first()
            if dev_cfg:
                if ack.status == "completed" and (ack.error_code is None or ack.error_code == 0):
                    dev_cfg.is_synced = True
                else:
                    dev_cfg.is_synced = False
    
    cmd.error_code = ack.error_code
    cmd.result_json = json.dumps({"message": ack.result_message or ""})
    db.commit()
    db.refresh(cmd)
    return cmd
