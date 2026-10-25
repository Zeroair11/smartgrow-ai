from datetime import datetime, timezone, timedelta
from sqlalchemy.orm import Session
from backend.app.models.entities import AutomationRule, AutomationRuleEvent, SensorReading, Alert
from backend.app.schemas.dtos import CommandCreate
from backend.app.services.command_service import create_command

def evaluate_automation_rules(db: Session, reading: SensorReading):
    """
    Evaluates active automation rules against newly ingested telemetry reading.
    """
    rules = db.query(AutomationRule).filter(AutomationRule.is_active == True).all()
    if not rules:
        return

    now = datetime.now(timezone.utc)

    for rule in rules:
        # Check metric value from reading
        val = getattr(reading, rule.sensor_metric, None)
        if val is None:
            continue

        # Check condition
        triggered = False
        if rule.operator == "<" and val < rule.threshold_val:
            triggered = True
        elif rule.operator == "<=" and val <= rule.threshold_val:
            triggered = True
        elif rule.operator == ">" and val > rule.threshold_val:
            triggered = True
        elif rule.operator == ">=" and val >= rule.threshold_val:
            triggered = True
        elif rule.operator == "==" and abs(val - rule.threshold_val) < 0.01:
            triggered = True

        if not triggered:
            continue

        # Check cooldown
        if rule.last_triggered_at:
            # Ensure aware datetime
            last_trig = rule.last_triggered_at
            if last_trig.tzinfo is None:
                last_trig = last_trig.replace(tzinfo=timezone.utc)
            if (now - last_trig).total_seconds() < rule.cooldown_s:
                continue  # Cooldown active

        # Rule condition met and cooldown satisfied!
        rule.last_triggered_at = now
        rule.execution_count += 1

        event_status = "executed"
        msg = f"Rule '{rule.name}' triggered: {rule.sensor_metric} ({val}) {rule.operator} {rule.threshold_val}"

        # Action execution
        if rule.action_type in ["PUMP_START", "PUMP_STOP", "VENT_SET"]:
            try:
                cmd_in = CommandCreate(
                    command_type=rule.action_type,
                    param_val=rule.action_param_val,
                    duration_s=int(rule.action_param_val) if rule.action_type == "PUMP_START" else 0,
                    ttl_s=300
                )
                create_command(db, reading.device_id, cmd_in)
                msg += f" -> Dispatched {rule.action_type}"
            except Exception as e:
                event_status = "failed"
                msg += f" -> Command dispatch error: {str(e)}"
        elif rule.action_type == "ALERT":
            alert = Alert(
                device_id=reading.device_id,
                zone_id=reading.zone_id,
                severity="warning",
                category="sensor",
                title=f"Automation Alert: {rule.name}",
                message=msg,
                created_at=now
            )
            db.add(alert)

        rule_event = AutomationRuleEvent(
            rule_id=rule.id,
            device_id=reading.device_id,
            triggered_at=now,
            metric_val=float(val),
            result_status=event_status,
            message=msg
        )
        db.add(rule_event)
        db.commit()
