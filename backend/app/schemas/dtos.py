from datetime import datetime
from typing import Optional, List, Dict, Any
from pydantic import BaseModel, Field, EmailStr

# Auth Schemas
class TokenResponse(BaseModel):
    access_token: str
    token_type: str = "bearer"
    role: str
    user_id: int
    email: str

class UserLogin(BaseModel):
    email: EmailStr
    password: str

class UserRegister(BaseModel):
    email: EmailStr
    password: str = Field(min_length=8)
    full_name: Optional[str] = None
    role: str = "operator"

class UserRead(BaseModel):
    id: int
    email: str
    full_name: Optional[str] = None
    role: str
    is_active: bool
    is_superuser: bool
    created_at: datetime

    class Config:
        from_attributes = True

# Greenhouse & Zone
class GreenhouseCreate(BaseModel):
    name: str
    code: str
    description: Optional[str] = None
    location: Optional[str] = None

class GreenhouseUpdate(BaseModel):
    name: Optional[str] = None
    description: Optional[str] = None
    location: Optional[str] = None
    is_active: Optional[bool] = None

class GreenhouseRead(BaseModel):
    id: int
    name: str
    code: str
    description: Optional[str] = None
    location: Optional[str] = None
    is_active: bool
    created_at: datetime
    updated_at: datetime

    class Config:
        from_attributes = True

class ZoneCreate(BaseModel):
    greenhouse_id: int
    name: str
    code: str
    crop_type: Optional[str] = None
    target_soil_moisture_min: float = 40.0
    target_soil_moisture_max: float = 70.0
    target_temp_min: float = 18.0
    target_temp_max: float = 32.0

class ZoneUpdate(BaseModel):
    name: Optional[str] = None
    crop_type: Optional[str] = None
    target_soil_moisture_min: Optional[float] = None
    target_soil_moisture_max: Optional[float] = None
    target_temp_min: Optional[float] = None
    target_temp_max: Optional[float] = None
    is_active: Optional[bool] = None

class ZoneRead(BaseModel):
    id: int
    greenhouse_id: int
    name: str
    code: str
    crop_type: Optional[str] = None
    target_soil_moisture_min: float
    target_soil_moisture_max: float
    target_temp_min: float
    target_temp_max: float
    is_active: bool
    created_at: datetime

    class Config:
        from_attributes = True

# Device Schemas
class DeviceRegister(BaseModel):
    device_id: str
    name: str
    zone_id: Optional[int] = None
    firmware_version: Optional[str] = "1.0.0"
    model_version: Optional[str] = "v1.0"
    mac_address: Optional[str] = None
    registration_token: str

class DeviceRead(BaseModel):
    id: int
    device_id: str
    name: str
    zone_id: Optional[int] = None
    status: str
    firmware_version: str
    model_version: str
    ip_address: Optional[str] = None
    mac_address: Optional[str] = None
    rssi: Optional[int] = None
    last_seen_at: Optional[datetime] = None
    registered_at: datetime

    class Config:
        from_attributes = True

class DeviceConfigUpdate(BaseModel):
    telemetry_interval_s: Optional[int] = None
    safety_max_runtime_s: Optional[int] = None
    safety_min_cooldown_s: Optional[int] = None
    soil_dry_adc: Optional[int] = None
    soil_wet_adc: Optional[int] = None
    operating_mode: Optional[str] = None

class DeviceConfigRead(BaseModel):
    device_id: str
    telemetry_interval_s: int
    safety_max_runtime_s: int
    safety_min_cooldown_s: int
    soil_dry_adc: int
    soil_wet_adc: int
    operating_mode: str
    is_synced: bool
    updated_at: datetime

    class Config:
        from_attributes = True

# Telemetry Schemas
class TelemetryIngest(BaseModel):
    schema_version: str = "1.0.0"
    device_id: str
    boot_id: Optional[str] = None
    sequence: int
    timestamp_ms: Optional[int] = None
    soil_moisture_pct: float = Field(ge=0.0, le=100.0)
    temperature_c: float = Field(ge=-20.0, le=80.0)
    humidity_pct: float = Field(ge=0.0, le=100.0)
    light_level_pct: float = Field(ge=0.0, le=100.0)
    ai_class: Optional[int] = 0
    ai_confidence: Optional[float] = 1.0
    model_version: Optional[str] = "v1.0"
    firmware_version: Optional[str] = "1.0.0"
    pump_state: bool = False
    operating_mode: Optional[str] = "AUTO"
    health_status: Optional[str] = "HEALTHY"

class SensorReadingRead(BaseModel):
    id: int
    device_id: str
    zone_id: Optional[int] = None
    sequence: int
    soil_moisture_pct: float
    temperature_c: float
    humidity_pct: float
    light_level_pct: float
    ai_class: int
    ai_confidence: float
    model_version: str
    firmware_version: str
    pump_state: bool
    operating_mode: str
    health_status: str
    recorded_at: datetime

    class Config:
        from_attributes = True

# Command Schemas
class CommandCreate(BaseModel):
    command_type: str  # PUMP_START, PUMP_STOP, VENT_SET, BUZZER_BEEP, RESTART
    param_val: Optional[float] = 0.0
    duration_s: Optional[int] = 30
    ttl_s: Optional[int] = 300

class CommandAck(BaseModel):
    command_id: str
    status: str  # acknowledged, executing, completed, failed, rejected
    result_message: Optional[str] = None
    error_code: Optional[int] = 0

class CommandRead(BaseModel):
    id: int
    command_id: str
    device_id: str
    command_type: str
    payload_json: Optional[str] = None
    status: str
    created_at: datetime
    expires_at: datetime
    acknowledged_at: Optional[datetime] = None
    completed_at: Optional[datetime] = None
    result_json: Optional[str] = None
    error_code: Optional[int] = None

    class Config:
        from_attributes = True

# Automation Rule Schemas
class AutomationRuleCreate(BaseModel):
    name: str
    zone_id: Optional[int] = None
    sensor_metric: str  # soil_moisture_pct, temperature_c, humidity_pct, light_level_pct
    operator: str       # <, <=, >, >=, ==
    threshold_val: float
    duration_s: int = 0
    action_type: str    # PUMP_START, PUMP_STOP, VENT_SET, ALERT
    action_param_val: float = 30.0
    cooldown_s: int = 300

class AutomationRuleRead(BaseModel):
    id: int
    name: str
    zone_id: Optional[int] = None
    is_active: bool
    sensor_metric: str
    operator: str
    threshold_val: float
    duration_s: int
    action_type: str
    action_param_val: float
    cooldown_s: int
    last_triggered_at: Optional[datetime] = None
    execution_count: int
    created_at: datetime

    class Config:
        from_attributes = True

# Irrigation Schemas
class IrrigationManualStart(BaseModel):
    device_id: str
    duration_seconds: int = Field(ge=5, le=120)

class IrrigationManualStop(BaseModel):
    device_id: str

class IrrigationEventRead(BaseModel):
    id: int
    device_id: str
    zone_id: Optional[int] = None
    start_reason: str
    stop_reason: str
    started_at: datetime
    stopped_at: Optional[datetime] = None
    duration_seconds: int
    volume_liters_est: float
    status: str

    class Config:
        from_attributes = True

class IrrigationScheduleCreate(BaseModel):
    name: str
    zone_id: Optional[int] = None
    device_id: Optional[str] = None
    cron_expression: str = "0 8 * * *"
    duration_seconds: int = 45

class IrrigationScheduleRead(BaseModel):
    id: int
    zone_id: Optional[int] = None
    device_id: Optional[str] = None
    name: str
    cron_expression: str
    duration_seconds: int
    is_active: bool
    next_run_at: Optional[datetime] = None
    last_run_at: Optional[datetime] = None
    created_at: datetime

    class Config:
        from_attributes = True

# Alert Schemas
class AlertCreate(BaseModel):
    device_id: Optional[str] = None
    zone_id: Optional[int] = None
    severity: str = "warning"
    category: str = "sensor"
    title: str
    message: str

class AlertRead(BaseModel):
    id: int
    device_id: Optional[str] = None
    zone_id: Optional[int] = None
    severity: str
    category: str
    title: str
    message: str
    is_acknowledged: bool
    is_resolved: bool
    created_at: datetime
    acknowledged_at: Optional[datetime] = None
    resolved_at: Optional[datetime] = None

    class Config:
        from_attributes = True

# Edge AI Schemas
class AIInferenceEventRead(BaseModel):
    id: int
    device_id: str
    zone_id: Optional[int] = None
    recorded_at: datetime
    soil_moisture_pct: float
    temperature_c: float
    humidity_pct: float
    light_level_pct: float
    predicted_class: int
    predicted_label: str
    confidence: float
    model_version: str
    safety_override: bool
    final_action: str

    class Config:
        from_attributes = True

class AIModelRead(BaseModel):
    id: int
    model_version: str
    model_type: str
    accuracy: float
    f1_score: float
    input_features: str
    is_active: bool
    deployed_at: datetime

    class Config:
        from_attributes = True

# Analytics / Overview Dashboard Schemas
class DashboardSummary(BaseModel):
    total_greenhouses: int
    total_zones: int
    total_devices: int
    online_devices: int
    offline_devices: int
    pump_active_count: int
    open_alerts_count: int
    today_irrigation_cycles: int
    today_water_duration_seconds: int
    latest_reading: Optional[SensorReadingRead] = None

# Audit Log
class AuditLogRead(BaseModel):
    id: int
    user_id: Optional[int] = None
    user_email: Optional[str] = None
    action: str
    resource_type: str
    resource_id: Optional[str] = None
    details_json: Optional[str] = None
    ip_address: Optional[str] = None
    created_at: datetime

    class Config:
        from_attributes = True

# OTA Schemas
class FirmwareReleaseCreate(BaseModel):
    version: str
    release_notes: Optional[str] = None

class FirmwareReleaseRead(BaseModel):
    id: int
    version: str
    checksum_sha256: str
    file_size_bytes: int
    release_notes: Optional[str] = None
    is_active: bool
    created_at: datetime

    class Config:
        from_attributes = True

class OTADeploymentRead(BaseModel):
    id: int
    release_id: int
    device_id: str
    status: str
    progress_pct: int
    created_at: datetime
    updated_at: datetime

    class Config:
        from_attributes = True
