import datetime
from sqlalchemy import (
    Column, Integer, String, Float, Boolean, DateTime, ForeignKey, Text, Index
)
from sqlalchemy.orm import relationship
from backend.app.core.database import Base

def utcnow():
    return datetime.datetime.now(datetime.timezone.utc)

class User(Base):
    __tablename__ = "users"

    id = Column(Integer, primary_key=True, index=True)
    email = Column(String(255), unique=True, index=True, nullable=False)
    hashed_password = Column(String(255), nullable=False)
    full_name = Column(String(255), nullable=True)
    role = Column(String(50), default="operator", nullable=False)  # admin, operator, viewer
    is_active = Column(Boolean, default=True, nullable=False)
    is_superuser = Column(Boolean, default=False, nullable=False)
    created_at = Column(DateTime(timezone=True), default=utcnow, nullable=False)
    updated_at = Column(DateTime(timezone=True), default=utcnow, onupdate=utcnow, nullable=False)


class Greenhouse(Base):
    __tablename__ = "greenhouses"

    id = Column(Integer, primary_key=True, index=True)
    name = Column(String(255), nullable=False)
    code = Column(String(50), unique=True, index=True, nullable=False)
    description = Column(Text, nullable=True)
    location = Column(String(255), nullable=True)
    is_active = Column(Boolean, default=True, nullable=False)
    created_at = Column(DateTime(timezone=True), default=utcnow, nullable=False)
    updated_at = Column(DateTime(timezone=True), default=utcnow, onupdate=utcnow, nullable=False)

    zones = relationship("Zone", back_populates="greenhouse", cascade="all, delete-orphan")


class Zone(Base):
    __tablename__ = "zones"

    id = Column(Integer, primary_key=True, index=True)
    greenhouse_id = Column(Integer, ForeignKey("greenhouses.id", ondelete="CASCADE"), nullable=False)
    name = Column(String(255), nullable=False)
    code = Column(String(50), nullable=False)
    crop_type = Column(String(100), nullable=True)
    target_soil_moisture_min = Column(Float, default=40.0)
    target_soil_moisture_max = Column(Float, default=70.0)
    target_temp_min = Column(Float, default=18.0)
    target_temp_max = Column(Float, default=32.0)
    is_active = Column(Boolean, default=True, nullable=False)
    created_at = Column(DateTime(timezone=True), default=utcnow, nullable=False)

    greenhouse = relationship("Greenhouse", back_populates="zones")
    devices = relationship("Device", back_populates="zone")


class Device(Base):
    __tablename__ = "devices"

    id = Column(Integer, primary_key=True, index=True)
    device_id = Column(String(64), unique=True, index=True, nullable=False)
    name = Column(String(255), nullable=False)
    zone_id = Column(Integer, ForeignKey("zones.id", ondelete="SET NULL"), nullable=True)
    token = Column(String(128), nullable=False)
    status = Column(String(50), default="offline")  # online, offline, degraded
    firmware_version = Column(String(50), default="1.0.0")
    model_version = Column(String(50), default="v1.0-baseline")
    ip_address = Column(String(50), nullable=True)
    mac_address = Column(String(50), nullable=True)
    rssi = Column(Integer, nullable=True)
    last_seen_at = Column(DateTime(timezone=True), nullable=True)
    registered_at = Column(DateTime(timezone=True), default=utcnow, nullable=False)

    zone = relationship("Zone", back_populates="devices")
    readings = relationship("SensorReading", back_populates="device", cascade="all, delete-orphan")
    commands = relationship("DeviceCommand", back_populates="device", cascade="all, delete-orphan")


class DeviceConfig(Base):
    __tablename__ = "device_configs"

    id = Column(Integer, primary_key=True, index=True)
    device_id = Column(String(64), unique=True, index=True, nullable=False)
    telemetry_interval_s = Column(Integer, default=5, nullable=False)
    safety_max_runtime_s = Column(Integer, default=120, nullable=False)
    safety_min_cooldown_s = Column(Integer, default=180, nullable=False)
    soil_dry_adc = Column(Integer, default=3200, nullable=False)
    soil_wet_adc = Column(Integer, default=1200, nullable=False)
    operating_mode = Column(String(20), default="AUTO", nullable=False)
    is_synced = Column(Boolean, default=True, nullable=False)
    updated_at = Column(DateTime(timezone=True), default=utcnow, onupdate=utcnow, nullable=False)


class SensorReading(Base):
    __tablename__ = "sensor_readings"

    id = Column(Integer, primary_key=True, index=True)
    device_id = Column(String(64), ForeignKey("devices.device_id", ondelete="CASCADE"), nullable=False)
    zone_id = Column(Integer, nullable=True)
    sequence = Column(Integer, nullable=False)
    boot_id = Column(String(64), nullable=True)
    soil_moisture_pct = Column(Float, nullable=False)
    temperature_c = Column(Float, nullable=False)
    humidity_pct = Column(Float, nullable=False)
    light_level_pct = Column(Float, nullable=False)
    ai_class = Column(Integer, default=0)
    ai_confidence = Column(Float, default=1.0)
    model_version = Column(String(50), default="v1.0")
    firmware_version = Column(String(50), default="1.0.0")
    pump_state = Column(Boolean, default=False)
    operating_mode = Column(String(20), default="AUTO")
    health_status = Column(String(20), default="HEALTHY")
    recorded_at = Column(DateTime(timezone=True), default=utcnow, nullable=False)

    device = relationship("Device", back_populates="readings")

    __table_args__ = (
        Index("idx_sensor_device_time", "device_id", "recorded_at"),
    )


class DeviceCommand(Base):
    __tablename__ = "device_commands"

    id = Column(Integer, primary_key=True, index=True)
    command_id = Column(String(64), unique=True, index=True, nullable=False)
    device_id = Column(String(64), ForeignKey("devices.device_id", ondelete="CASCADE"), nullable=False)
    command_type = Column(String(50), nullable=False)
    payload_json = Column(Text, nullable=True)
    status = Column(String(50), default="pending", nullable=False)  # pending, acknowledged, executing, completed, failed, expired, rejected
    created_at = Column(DateTime(timezone=True), default=utcnow, nullable=False)
    expires_at = Column(DateTime(timezone=True), nullable=False)
    acknowledged_at = Column(DateTime(timezone=True), nullable=True)
    completed_at = Column(DateTime(timezone=True), nullable=True)
    result_json = Column(Text, nullable=True)
    error_code = Column(Integer, nullable=True)

    device = relationship("Device", back_populates="commands")

    __table_args__ = (
        Index("idx_cmd_device_status", "device_id", "status"),
    )


class AutomationRule(Base):
    __tablename__ = "automation_rules"

    id = Column(Integer, primary_key=True, index=True)
    name = Column(String(255), nullable=False)
    zone_id = Column(Integer, nullable=True)
    is_active = Column(Boolean, default=True, nullable=False)
    sensor_metric = Column(String(50), nullable=False)  # soil_moisture_pct, temperature_c, humidity_pct, light_level_pct
    operator = Column(String(10), nullable=False)       # <, <=, >, >=, ==
    threshold_val = Column(Float, nullable=False)
    duration_s = Column(Integer, default=0)
    action_type = Column(String(50), nullable=False)    # PUMP_START, PUMP_STOP, VENT_SET, ALERT
    action_param_val = Column(Float, default=30.0)      # e.g. 30 seconds for pump, or vent angle
    cooldown_s = Column(Integer, default=300)
    last_triggered_at = Column(DateTime(timezone=True), nullable=True)
    execution_count = Column(Integer, default=0)
    created_at = Column(DateTime(timezone=True), default=utcnow, nullable=False)


class AutomationRuleEvent(Base):
    __tablename__ = "automation_rule_events"

    id = Column(Integer, primary_key=True, index=True)
    rule_id = Column(Integer, ForeignKey("automation_rules.id", ondelete="CASCADE"), nullable=False)
    device_id = Column(String(64), nullable=False)
    triggered_at = Column(DateTime(timezone=True), default=utcnow, nullable=False)
    metric_val = Column(Float, nullable=False)
    result_status = Column(String(50), nullable=False)
    message = Column(Text, nullable=True)


class IrrigationEvent(Base):
    __tablename__ = "irrigation_events"

    id = Column(Integer, primary_key=True, index=True)
    device_id = Column(String(64), nullable=False)
    zone_id = Column(Integer, nullable=True)
    start_reason = Column(String(50), default="MANUAL")  # MANUAL, AUTOMATION, AI, SCHEDULE
    stop_reason = Column(String(50), default="DURATION_REACHED") # DURATION_REACHED, SAFETY_WATCHDOG, MANUAL_STOP, SENSOR_FAULT
    started_at = Column(DateTime(timezone=True), default=utcnow, nullable=False)
    stopped_at = Column(DateTime(timezone=True), nullable=True)
    duration_seconds = Column(Integer, default=0)
    volume_liters_est = Column(Float, default=0.0)
    status = Column(String(50), default="completed")


class IrrigationSchedule(Base):
    __tablename__ = "irrigation_schedules"

    id = Column(Integer, primary_key=True, index=True)
    zone_id = Column(Integer, nullable=True)
    device_id = Column(String(64), nullable=True)
    name = Column(String(255), nullable=False)
    cron_expression = Column(String(100), default="0 8 * * *")  # Every morning at 8:00 AM
    duration_seconds = Column(Integer, default=45)
    is_active = Column(Boolean, default=True)
    next_run_at = Column(DateTime(timezone=True), nullable=True)
    last_run_at = Column(DateTime(timezone=True), nullable=True)
    created_at = Column(DateTime(timezone=True), default=utcnow, nullable=False)


class Alert(Base):
    __tablename__ = "alerts"

    id = Column(Integer, primary_key=True, index=True)
    device_id = Column(String(64), nullable=True)
    zone_id = Column(Integer, nullable=True)
    severity = Column(String(50), default="warning")  # info, warning, error, critical
    category = Column(String(50), default="sensor")   # sensor, connectivity, safety, actuator, ai
    title = Column(String(255), nullable=False)
    message = Column(Text, nullable=False)
    is_acknowledged = Column(Boolean, default=False)
    is_resolved = Column(Boolean, default=False)
    created_at = Column(DateTime(timezone=True), default=utcnow, nullable=False)
    acknowledged_at = Column(DateTime(timezone=True), nullable=True)
    resolved_at = Column(DateTime(timezone=True), nullable=True)
    resolved_by_user_id = Column(Integer, nullable=True)


class AIInferenceEvent(Base):
    __tablename__ = "ai_inference_events"

    id = Column(Integer, primary_key=True, index=True)
    device_id = Column(String(64), nullable=False)
    zone_id = Column(Integer, nullable=True)
    recorded_at = Column(DateTime(timezone=True), default=utcnow, nullable=False)
    soil_moisture_pct = Column(Float, nullable=False)
    temperature_c = Column(Float, nullable=False)
    humidity_pct = Column(Float, nullable=False)
    light_level_pct = Column(Float, nullable=False)
    predicted_class = Column(Integer, nullable=False)
    predicted_label = Column(String(50), nullable=False)
    confidence = Column(Float, default=1.0)
    model_version = Column(String(50), default="v1.0")
    safety_override = Column(Boolean, default=False)
    final_action = Column(String(50), default="NONE")


class AIModel(Base):
    __tablename__ = "ai_models"

    id = Column(Integer, primary_key=True, index=True)
    model_version = Column(String(50), unique=True, index=True, nullable=False)
    model_type = Column(String(50), default="decision_tree")
    accuracy = Column(Float, default=0.0)
    f1_score = Column(Float, default=0.0)
    input_features = Column(String(255), default="soil,temp,hum,light")
    is_active = Column(Boolean, default=True)
    deployed_at = Column(DateTime(timezone=True), default=utcnow, nullable=False)


class FirmwareRelease(Base):
    __tablename__ = "firmware_releases"

    id = Column(Integer, primary_key=True, index=True)
    version = Column(String(50), unique=True, index=True, nullable=False)
    checksum_sha256 = Column(String(64), nullable=False)
    file_path = Column(String(255), nullable=False)
    file_size_bytes = Column(Integer, nullable=False)
    release_notes = Column(Text, nullable=True)
    is_active = Column(Boolean, default=True)
    created_at = Column(DateTime(timezone=True), default=utcnow, nullable=False)


class OTADeployment(Base):
    __tablename__ = "ota_deployments"

    id = Column(Integer, primary_key=True, index=True)
    release_id = Column(Integer, ForeignKey("firmware_releases.id", ondelete="CASCADE"), nullable=False)
    device_id = Column(String(64), nullable=False)
    status = Column(String(50), default="pending")  # pending, downloading, applying, success, failed, rolled_back
    progress_pct = Column(Integer, default=0)
    created_at = Column(DateTime(timezone=True), default=utcnow, nullable=False)
    updated_at = Column(DateTime(timezone=True), default=utcnow, onupdate=utcnow, nullable=False)


class AuditLog(Base):
    __tablename__ = "audit_logs"

    id = Column(Integer, primary_key=True, index=True)
    user_id = Column(Integer, nullable=True)
    user_email = Column(String(255), nullable=True)
    action = Column(String(100), nullable=False)
    resource_type = Column(String(100), nullable=False)
    resource_id = Column(String(100), nullable=True)
    details_json = Column(Text, nullable=True)
    ip_address = Column(String(50), nullable=True)
    created_at = Column(DateTime(timezone=True), default=utcnow, nullable=False)
