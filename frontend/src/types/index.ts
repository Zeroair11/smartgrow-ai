export interface User {
  id: number
  email: string
  full_name?: string
  role: 'admin' | 'operator' | 'viewer'
  is_active: boolean
  is_superuser: boolean
  created_at: string
}

export interface Greenhouse {
  id: number
  name: string
  code: string
  description?: string
  location?: string
  is_active: boolean
  created_at: string
  updated_at: string
}

export interface Zone {
  id: number
  greenhouse_id: number
  name: string
  code: string
  crop_type?: string
  target_soil_moisture_min: number
  target_soil_moisture_max: number
  target_temp_min: number
  target_temp_max: number
  is_active: boolean
  created_at: string
}

export interface Device {
  id: number
  device_id: string
  name: string
  zone_id?: number
  status: 'online' | 'offline' | 'degraded'
  firmware_version: string
  model_version: string
  ip_address?: string
  mac_address?: string
  rssi?: number
  last_seen_at?: string
  registered_at: string
}

export interface DeviceConfig {
  device_id: string
  telemetry_interval_s: number
  safety_max_runtime_s: number
  safety_min_cooldown_s: number
  soil_dry_adc: number
  soil_wet_adc: number
  operating_mode: string
  is_synced: boolean
  updated_at: string
}

export interface SensorReading {
  id: number
  device_id: string
  zone_id?: number
  sequence: number
  soil_moisture_pct: number
  temperature_c: number
  humidity_pct: number
  light_level_pct: number
  ai_class: number
  ai_confidence: number
  model_version: string
  firmware_version: string
  pump_state: boolean
  operating_mode: string
  health_status: string
  recorded_at: string
}

export interface DeviceCommand {
  id: number
  command_id: string
  device_id: string
  command_type: string
  payload_json?: string
  status: 'pending' | 'acknowledged' | 'executing' | 'completed' | 'failed' | 'expired' | 'rejected'
  created_at: string
  expires_at: string
  acknowledged_at?: string
  completed_at?: string
  result_json?: string
  error_code?: number
}

export interface AutomationRule {
  id: number
  name: string
  zone_id?: number
  is_active: boolean
  sensor_metric: string
  operator: string
  threshold_val: number
  duration_s: number
  action_type: string
  action_param_val: number
  cooldown_s: number
  last_triggered_at?: string
  execution_count: number
  created_at: string
}

export interface AutomationRuleEvent {
  id: number
  rule_id: number
  device_id: string
  triggered_at: string
  metric_val: number
  result_status: string
  message?: string
}

export interface IrrigationEvent {
  id: number
  device_id: string
  zone_id?: number
  start_reason: string
  stop_reason: string
  started_at: string
  stopped_at?: string
  duration_seconds: number
  volume_liters_est: number
  status: string
}

export interface IrrigationSchedule {
  id: number
  zone_id?: number
  device_id?: string
  name: string
  cron_expression: string
  duration_seconds: number
  is_active: boolean
  next_run_at?: string
  last_run_at?: string
  created_at: string
}

export interface Alert {
  id: number
  device_id?: string
  zone_id?: number
  severity: 'info' | 'warning' | 'error' | 'critical'
  category: 'sensor' | 'connectivity' | 'safety' | 'actuator' | 'ai'
  title: string
  message: string
  is_acknowledged: boolean
  is_resolved: boolean
  created_at: string
  acknowledged_at?: string
  resolved_at?: string
}

export interface AIInferenceEvent {
  id: number
  device_id: string
  zone_id?: number
  recorded_at: string
  soil_moisture_pct: number
  temperature_c: number
  humidity_pct: number
  light_level_pct: number
  predicted_class: number
  predicted_label: string
  confidence: number
  model_version: string
  safety_override: boolean
  final_action: string
}

export interface DashboardSummary {
  total_greenhouses: number
  total_zones: number
  total_devices: number
  online_devices: number
  offline_devices: number
  pump_active_count: number
  open_alerts_count: number
  today_irrigation_cycles: number
  today_water_duration_seconds: number
  latest_reading?: SensorReading
}

export interface AuditLog {
  id: number
  user_id?: number
  user_email?: string
  action: string
  resource_type: string
  resource_id?: string
  details_json?: string
  ip_address?: string
  created_at: string
}

export interface FirmwareRelease {
  id: number
  version: string
  checksum_sha256: string
  file_size_bytes: number
  release_notes?: string
  is_active: boolean
  created_at: string
}
