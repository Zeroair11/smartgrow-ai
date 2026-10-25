<template>
  <div class="space-y-6">
    <!-- Header Summary Bar -->
    <div class="flex flex-col sm:flex-row sm:items-center justify-between gap-4">
      <div>
        <h2 class="text-xl font-bold text-white tracking-tight">Greenhouse Live Overview</h2>
        <p class="text-xs text-slate-400 font-mono mt-0.5">Real-time edge telemetry, AI classification and actuator status</p>
      </div>
      <div class="flex items-center gap-2">
        <span class="inline-flex items-center gap-1.5 px-2.5 py-1 rounded-md bg-slate-900 border border-slate-800 text-xs font-mono text-slate-300">
          <span class="w-2 h-2 rounded-full" :class="isPolling ? 'bg-emerald-400 animate-pulse' : 'bg-slate-500'"></span>
          {{ isPolling ? 'Live Polling 3s' : 'Paused' }}
        </span>
        <button
          @click="fetchData"
          class="px-3 py-1 rounded-md bg-slate-800 hover:bg-slate-700 text-slate-200 text-xs font-medium border border-slate-700 transition-all"
        >
          Refresh Now
        </button>
      </div>
    </div>

    <!-- KPI Metric Cards Grid -->
    <div class="grid grid-cols-2 md:grid-cols-4 gap-4">
      <!-- Card 1: Greenhouses & Zones -->
      <div class="p-4 rounded-xl bg-slate-900/60 border border-slate-800/80 backdrop-blur-sm">
        <p class="text-xs font-medium text-slate-400 font-mono">Facilities & Zones</p>
        <div class="mt-2 flex items-baseline gap-2">
          <span class="text-2xl font-bold font-mono text-white">{{ summary?.total_greenhouses || 0 }}</span>
          <span class="text-xs text-slate-400 font-mono">GH / {{ summary?.total_zones || 0 }} Zones</span>
        </div>
        <div class="mt-2 text-[11px] text-emerald-400/90 font-mono">Operational & Monitored</div>
      </div>

      <!-- Card 2: Devices Online -->
      <div class="p-4 rounded-xl bg-slate-900/60 border border-slate-800/80 backdrop-blur-sm">
        <p class="text-xs font-medium text-slate-400 font-mono">ESP32-S3 Controllers</p>
        <div class="mt-2 flex items-baseline gap-2">
          <span class="text-2xl font-bold font-mono text-emerald-400">{{ summary?.online_devices || 0 }}</span>
          <span class="text-xs text-slate-400 font-mono">Online / {{ summary?.total_devices || 0 }} Total</span>
        </div>
        <div class="mt-2 text-[11px] text-slate-400 font-mono">
          <span :class="(summary?.offline_devices || 0) > 0 ? 'text-amber-400' : 'text-slate-400'">
            {{ summary?.offline_devices || 0 }} offline
          </span>
        </div>
      </div>

      <!-- Card 3: Pump & Irrigation -->
      <div class="p-4 rounded-xl bg-slate-900/60 border border-slate-800/80 backdrop-blur-sm">
        <p class="text-xs font-medium text-slate-400 font-mono">Active Irrigation</p>
        <div class="mt-2 flex items-baseline gap-2">
          <span class="text-2xl font-bold font-mono" :class="summary?.pump_active_count ? 'text-cyan-400 animate-pulse' : 'text-slate-300'">
            {{ summary?.pump_active_count ? 'RUNNING' : 'IDLE' }}
          </span>
        </div>
        <div class="mt-2 text-[11px] text-slate-400 font-mono">
          {{ summary?.today_irrigation_cycles || 0 }} cycles today ({{ summary?.today_water_duration_seconds || 0 }}s total)
        </div>
      </div>

      <!-- Card 4: Open Alerts -->
      <div class="p-4 rounded-xl bg-slate-900/60 border border-slate-800/80 backdrop-blur-sm">
        <p class="text-xs font-medium text-slate-400 font-mono">Incident Alerts</p>
        <div class="mt-2 flex items-baseline gap-2">
          <span class="text-2xl font-bold font-mono" :class="(summary?.open_alerts_count || 0) > 0 ? 'text-rose-400' : 'text-emerald-400'">
            {{ summary?.open_alerts_count || 0 }}
          </span>
          <span class="text-xs text-slate-400 font-mono">Unresolved</span>
        </div>
        <div class="mt-2 text-[11px] font-mono" :class="(summary?.open_alerts_count || 0) > 0 ? 'text-rose-400' : 'text-emerald-400/90'">
          {{ (summary?.open_alerts_count || 0) > 0 ? 'Requires attention' : 'All parameters nominal' }}
        </div>
      </div>
    </div>

    <!-- Live Sensor Gauges (Latest Telemetry) -->
    <div class="p-6 rounded-2xl bg-slate-900/60 border border-slate-800/80 backdrop-blur-sm">
      <div class="flex items-center justify-between mb-4">
        <h3 class="text-sm font-semibold text-white uppercase tracking-wider font-mono flex items-center gap-2">
          <span>📡</span> Live Sensor Telemetry (Device: <span class="text-emerald-400">{{ latestReading?.device_id || 'Waiting for edge data...' }}</span>)
        </h3>
        <span v-if="latestReading" class="text-xs font-mono text-slate-400">
          Seq: #{{ latestReading.sequence }} • {{ new Date(latestReading.recorded_at).toLocaleTimeString() }}
        </span>
      </div>

      <div v-if="latestReading" class="grid grid-cols-2 md:grid-cols-4 gap-4">
        <!-- Soil Moisture -->
        <div class="p-4 rounded-xl bg-slate-950/60 border border-slate-800">
          <div class="flex justify-between items-center text-xs text-slate-400 font-mono">
            <span>Soil Moisture</span>
            <span>💧</span>
          </div>
          <div class="mt-2 flex items-baseline gap-1">
            <span class="text-3xl font-extrabold font-mono text-cyan-400">{{ latestReading.soil_moisture_pct.toFixed(1) }}</span>
            <span class="text-sm font-mono text-slate-400">%</span>
          </div>
          <div class="w-full bg-slate-800 rounded-full h-1.5 mt-3 overflow-hidden">
            <div class="bg-cyan-500 h-1.5 rounded-full transition-all duration-500" :style="{ width: `${Math.min(latestReading.soil_moisture_pct, 100)}%` }"></div>
          </div>
        </div>

        <!-- Temperature -->
        <div class="p-4 rounded-xl bg-slate-950/60 border border-slate-800">
          <div class="flex justify-between items-center text-xs text-slate-400 font-mono">
            <span>Air Temperature</span>
            <span>🌡️</span>
          </div>
          <div class="mt-2 flex items-baseline gap-1">
            <span class="text-3xl font-extrabold font-mono text-amber-400">{{ latestReading.temperature_c.toFixed(1) }}</span>
            <span class="text-sm font-mono text-slate-400">°C</span>
          </div>
          <div class="w-full bg-slate-800 rounded-full h-1.5 mt-3 overflow-hidden">
            <div class="bg-amber-500 h-1.5 rounded-full transition-all duration-500" :style="{ width: `${Math.min((latestReading.temperature_c / 50) * 100, 100)}%` }"></div>
          </div>
        </div>

        <!-- Air Humidity -->
        <div class="p-4 rounded-xl bg-slate-950/60 border border-slate-800">
          <div class="flex justify-between items-center text-xs text-slate-400 font-mono">
            <span>Air Humidity</span>
            <span>💨</span>
          </div>
          <div class="mt-2 flex items-baseline gap-1">
            <span class="text-3xl font-extrabold font-mono text-indigo-400">{{ latestReading.humidity_pct.toFixed(1) }}</span>
            <span class="text-sm font-mono text-slate-400">%</span>
          </div>
          <div class="w-full bg-slate-800 rounded-full h-1.5 mt-3 overflow-hidden">
            <div class="bg-indigo-500 h-1.5 rounded-full transition-all duration-500" :style="{ width: `${Math.min(latestReading.humidity_pct, 100)}%` }"></div>
          </div>
        </div>

        <!-- Ambient Light -->
        <div class="p-4 rounded-xl bg-slate-950/60 border border-slate-800">
          <div class="flex justify-between items-center text-xs text-slate-400 font-mono">
            <span>Ambient Light</span>
            <span>☀️</span>
          </div>
          <div class="mt-2 flex items-baseline gap-1">
            <span class="text-3xl font-extrabold font-mono text-yellow-400">{{ latestReading.light_level_pct.toFixed(1) }}</span>
            <span class="text-sm font-mono text-slate-400">%</span>
          </div>
          <div class="w-full bg-slate-800 rounded-full h-1.5 mt-3 overflow-hidden">
            <div class="bg-yellow-500 h-1.5 rounded-full transition-all duration-500" :style="{ width: `${Math.min(latestReading.light_level_pct, 100)}%` }"></div>
          </div>
        </div>
      </div>

      <!-- Empty state when no telemetry yet -->
      <div v-else class="text-center py-10 bg-slate-950/40 rounded-xl border border-dashed border-slate-800">
        <span class="text-3xl">📡</span>
        <h4 class="text-sm font-medium text-slate-300 mt-2 font-mono">Awaiting ESP32-S3 Telemetry Uplink</h4>
        <p class="text-xs text-slate-500 max-w-sm mx-auto mt-1">Start firmware or simulator to see real-time sensor streams and TinyML classification.</p>
      </div>

      <!-- Edge AI Classification & Safety Strip -->
      <div v-if="latestReading" class="mt-4 pt-4 border-t border-slate-800/80 flex flex-wrap items-center justify-between gap-4">
        <div class="flex items-center gap-3">
          <span class="text-xs font-mono text-slate-400">Edge AI Decision:</span>
          <span
            class="px-3 py-1 rounded-md text-xs font-mono font-bold border"
            :class="getAiClassBadge(latestReading.ai_class)"
          >
            {{ getAiClassLabel(latestReading.ai_class) }} (Conf: {{ (latestReading.ai_confidence * 100).toFixed(0) }}%)
          </span>
          <span class="text-xs font-mono text-slate-400">Model: <code class="text-slate-300">{{ latestReading.model_version }}</code></span>
        </div>

        <div class="flex items-center gap-3">
          <span class="text-xs font-mono text-slate-400">Operating Mode:</span>
          <span class="px-2.5 py-0.5 rounded bg-slate-800 border border-slate-700 text-xs font-mono text-slate-300">
            {{ latestReading.operating_mode }}
          </span>
          <span class="text-xs font-mono text-slate-400">Safety Interlock:</span>
          <span class="px-2.5 py-0.5 rounded bg-emerald-500/10 border border-emerald-500/30 text-xs font-mono text-emerald-400">
            ARMED
          </span>
        </div>
      </div>
    </div>

    <!-- Two-Column Section: Active Alerts & Recent History -->
    <div class="grid grid-cols-1 md:grid-cols-2 gap-6">
      <!-- Recent Alerts -->
      <div class="p-5 rounded-2xl bg-slate-900/60 border border-slate-800/80">
        <div class="flex items-center justify-between mb-3">
          <h3 class="text-sm font-semibold text-white font-mono flex items-center gap-2">
            <span>🔔</span> Active Alerts
          </h3>
          <router-link to="/alerts" class="text-xs font-mono text-emerald-400 hover:underline">View All &rarr;</router-link>
        </div>

        <div v-if="alerts.length > 0" class="space-y-2">
          <div
            v-for="alert in alerts.slice(0, 4)"
            :key="alert.id"
            class="p-3 rounded-lg border flex items-start justify-between gap-2 text-xs"
            :class="alert.severity === 'critical' ? 'bg-rose-500/10 border-rose-500/30 text-rose-300' : 'bg-amber-500/10 border-amber-500/30 text-amber-300'"
          >
            <div>
              <p class="font-semibold">{{ alert.title }}</p>
              <p class="text-[11px] text-slate-400 mt-0.5">{{ alert.message }}</p>
            </div>
            <span class="text-[10px] font-mono shrink-0 text-slate-500">{{ new Date(alert.created_at).toLocaleTimeString() }}</span>
          </div>
        </div>
        <div v-else class="text-center py-8 text-xs text-slate-500 font-mono">
          ✅ No active alerts. All greenhouse zones nominal.
        </div>
      </div>

      <!-- Quick Action / Irrigation Control -->
      <div class="p-5 rounded-2xl bg-slate-900/60 border border-slate-800/80">
        <div class="flex items-center justify-between mb-3">
          <h3 class="text-sm font-semibold text-white font-mono flex items-center gap-2">
            <span>⚡</span> Quick Control & Pump Override
          </h3>
          <router-link to="/irrigation" class="text-xs font-mono text-emerald-400 hover:underline">Irrigation Center &rarr;</router-link>
        </div>

        <div class="space-y-3">
          <p class="text-xs text-slate-400">Directly command actuators with firmware-enforced safety cutoffs (max 120s duration).</p>
          
          <div class="flex flex-wrap gap-2 pt-2">
            <button
              @click="triggerQuickIrrigation(30)"
              :disabled="isSubmitting"
              class="px-3.5 py-2 rounded-lg bg-emerald-500/20 hover:bg-emerald-500/30 border border-emerald-500/40 text-emerald-300 text-xs font-mono font-medium flex items-center gap-1.5 transition-all disabled:opacity-50"
            >
              💧 Quick Water (30s)
            </button>
            <button
              @click="triggerQuickIrrigation(60)"
              :disabled="isSubmitting"
              class="px-3.5 py-2 rounded-lg bg-emerald-500/20 hover:bg-emerald-500/30 border border-emerald-500/40 text-emerald-300 text-xs font-mono font-medium flex items-center gap-1.5 transition-all disabled:opacity-50"
            >
              💧 Normal Water (60s)
            </button>
            <button
              @click="triggerQuickStop"
              :disabled="isSubmitting"
              class="px-3.5 py-2 rounded-lg bg-rose-500/20 hover:bg-rose-500/30 border border-rose-500/40 text-rose-300 text-xs font-mono font-medium flex items-center gap-1.5 transition-all disabled:opacity-50"
            >
              🛑 Emergency Stop
            </button>
          </div>

          <div v-if="actionFeedback" class="mt-2 p-2.5 rounded-lg bg-slate-950 border border-slate-800 text-xs font-mono text-emerald-400">
            {{ actionFeedback }}
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, onUnmounted } from 'vue'
import apiClient from '@/api/client'
import type { DashboardSummary, SensorReading, Alert } from '@/types'

const summary = ref<DashboardSummary | null>(null)
const latestReading = ref<SensorReading | null>(null)
const alerts = ref<Alert[]>([])
const isPolling = ref(true)
const isSubmitting = ref(false)
const actionFeedback = ref('')

let pollTimer: any = null

const AI_LABELS: Record<number, string> = {
  0: 'NORMAL',
  1: 'NEED_WATER',
  2: 'HIGH_TEMPERATURE',
  3: 'LOW_LIGHT',
  4: 'SENSOR_FAULT'
}

function getAiClassLabel(classId: number): string {
  return AI_LABELS[classId] || 'UNKNOWN'
}

function getAiClassBadge(classId: number): string {
  switch (classId) {
    case 0: return 'bg-emerald-500/10 border-emerald-500/30 text-emerald-400'
    case 1: return 'bg-cyan-500/10 border-cyan-500/30 text-cyan-400'
    case 2: return 'bg-amber-500/10 border-amber-500/30 text-amber-400'
    case 3: return 'bg-purple-500/10 border-purple-500/30 text-purple-400'
    case 4: return 'bg-rose-500/10 border-rose-500/30 text-rose-400'
    default: return 'bg-slate-800 border-slate-700 text-slate-300'
  }
}

async function fetchData() {
  try {
    const [sumRes, alertsRes, latestRes] = await Promise.all([
      apiClient.get<DashboardSummary>('/analytics/dashboard-summary'),
      apiClient.get<Alert[]>('/alerts?is_resolved=false&limit=5'),
      apiClient.get<SensorReading>('/telemetry/latest')
    ])
    summary.value = sumRes.data
    alerts.value = alertsRes.data
    latestReading.value = latestRes.data || null
  } catch (err) {
    console.error('Failed to fetch dashboard data', err)
  }
}

async function triggerQuickIrrigation(seconds: number) {
  if (!latestReading.value?.device_id) {
    actionFeedback.value = 'No active device available for irrigation command.'
    return
  }
  isSubmitting.value = true
  actionFeedback.value = `Dispatching PUMP_START (${seconds}s)...`
  try {
    const res = await apiClient.post('/irrigation/start', {
      device_id: latestReading.value.device_id,
      duration_seconds: seconds
    })
    actionFeedback.value = `Command dispatched: #${res.data.command_id} (Status: ${res.data.status})`
    await fetchData()
  } catch (err: any) {
    actionFeedback.value = `Error: ${err.response?.data?.detail || err.message}`
  } finally {
    isSubmitting.value = false
  }
}

async function triggerQuickStop() {
  if (!latestReading.value?.device_id) return
  isSubmitting.value = true
  actionFeedback.value = 'Dispatching PUMP_STOP...'
  try {
    const res = await apiClient.post('/irrigation/stop', {
      device_id: latestReading.value.device_id
    })
    actionFeedback.value = `Stop command dispatched: #${res.data.command_id}`
    await fetchData()
  } catch (err: any) {
    actionFeedback.value = `Error: ${err.response?.data?.detail || err.message}`
  } finally {
    isSubmitting.value = false
  }
}

onMounted(() => {
  fetchData()
  pollTimer = setInterval(fetchData, 3000)
})

onUnmounted(() => {
  if (pollTimer) clearInterval(pollTimer)
})
</script>
