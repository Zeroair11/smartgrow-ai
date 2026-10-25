<template>
  <div class="space-y-6">
    <!-- Header with Filters & Actions -->
    <div class="flex flex-col sm:flex-row sm:items-center justify-between gap-4">
      <div>
        <h2 class="text-xl font-bold text-white tracking-tight">Live Sensor Telemetry</h2>
        <p class="text-xs text-slate-400 font-mono mt-0.5">High-frequency time-series streams and environmental history</p>
      </div>

      <div class="flex flex-wrap items-center gap-2">
        <select
          v-model="selectedDeviceId"
          @change="fetchTelemetry"
          class="px-3 py-1.5 rounded-lg bg-slate-900 border border-slate-800 text-xs font-mono text-slate-200 focus:outline-none focus:border-emerald-500"
        >
          <option value="">All Controllers</option>
          <option v-for="dev in devices" :key="dev.id" :value="dev.device_id">{{ dev.name }} ({{ dev.device_id }})</option>
        </select>

        <a
          href="/api/v1/telemetry/export-csv"
          target="_blank"
          class="px-3 py-1.5 rounded-lg bg-slate-800 hover:bg-slate-700 text-slate-200 text-xs font-medium border border-slate-700 transition-all flex items-center gap-1.5"
        >
          <span>📥</span> Export CSV
        </a>

        <button
          @click="fetchTelemetry"
          class="px-3 py-1.5 rounded-lg bg-emerald-500 hover:bg-emerald-600 text-white text-xs font-semibold shadow-md shadow-emerald-500/20 transition-all flex items-center gap-1.5"
        >
          <span>🔄</span> Refresh
        </button>
      </div>
    </div>

    <!-- Live Telemetry Stream Cards Grid -->
    <div class="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
      <div class="p-4 rounded-xl bg-slate-900/60 border border-slate-800/80">
        <div class="flex items-center justify-between text-xs font-mono text-slate-400">
          <span>Avg Soil Moisture</span>
          <span class="text-cyan-400 font-bold">💧</span>
        </div>
        <div class="mt-2 text-2xl font-bold font-mono text-cyan-400">
          {{ avgSoilMoisture.toFixed(1) }}%
        </div>
        <div class="mt-1 text-[11px] text-slate-400 font-mono">Based on latest {{ readings.length }} samples</div>
      </div>

      <div class="p-4 rounded-xl bg-slate-900/60 border border-slate-800/80">
        <div class="flex items-center justify-between text-xs font-mono text-slate-400">
          <span>Avg Temperature</span>
          <span class="text-amber-400 font-bold">🌡️</span>
        </div>
        <div class="mt-2 text-2xl font-bold font-mono text-amber-400">
          {{ avgTemperature.toFixed(1) }}°C
        </div>
        <div class="mt-1 text-[11px] text-slate-400 font-mono">Range: {{ minTemp.toFixed(1) }}°C - {{ maxTemp.toFixed(1) }}°C</div>
      </div>

      <div class="p-4 rounded-xl bg-slate-900/60 border border-slate-800/80">
        <div class="flex items-center justify-between text-xs font-mono text-slate-400">
          <span>Avg Air Humidity</span>
          <span class="text-indigo-400 font-bold">💨</span>
        </div>
        <div class="mt-2 text-2xl font-bold font-mono text-indigo-400">
          {{ avgHumidity.toFixed(1) }}%
        </div>
        <div class="mt-1 text-[11px] text-slate-400 font-mono">Vapor pressure balance</div>
      </div>

      <div class="p-4 rounded-xl bg-slate-900/60 border border-slate-800/80">
        <div class="flex items-center justify-between text-xs font-mono text-slate-400">
          <span>Avg Sunlight Level</span>
          <span class="text-yellow-400 font-bold">☀️</span>
        </div>
        <div class="mt-2 text-2xl font-bold font-mono text-yellow-400">
          {{ avgLight.toFixed(1) }}%
        </div>
        <div class="mt-1 text-[11px] text-slate-400 font-mono">Photoperiod status</div>
      </div>
    </div>

    <!-- Telemetry Packets Log Table -->
    <div class="rounded-2xl bg-slate-900/60 border border-slate-800/80 overflow-hidden backdrop-blur-sm">
      <div class="p-4 border-b border-slate-800 flex items-center justify-between">
        <h3 class="text-sm font-semibold text-white font-mono flex items-center gap-2">
          <span>📋</span> Real-time Ingestion Feed ({{ readings.length }} Records)
        </h3>
        <span class="text-xs font-mono text-slate-400">Polling every 4 seconds</span>
      </div>

      <div class="overflow-x-auto">
        <table class="w-full text-left text-xs border-collapse">
          <thead>
            <tr class="border-b border-slate-800 bg-slate-900/80 font-mono text-slate-400">
              <th class="p-3">Seq #</th>
              <th class="p-3">Timestamp</th>
              <th class="p-3">Device ID</th>
              <th class="p-3">Soil (%)</th>
              <th class="p-3">Temp (°C)</th>
              <th class="p-3">Hum (%)</th>
              <th class="p-3">Light (%)</th>
              <th class="p-3">Edge AI</th>
              <th class="p-3">Pump</th>
            </tr>
          </thead>
          <tbody class="divide-y divide-slate-800/60 font-mono text-[11px]">
            <tr v-for="r in readings" :key="r.id" class="hover:bg-slate-800/30 transition-colors">
              <td class="p-3 font-semibold text-slate-300">#{{ r.sequence }}</td>
              <td class="p-3 text-slate-400">{{ new Date(r.recorded_at).toLocaleTimeString() }}</td>
              <td class="p-3 text-emerald-400">{{ r.device_id }}</td>
              <td class="p-3 text-cyan-400 font-semibold">{{ r.soil_moisture_pct.toFixed(1) }}%</td>
              <td class="p-3 text-amber-400 font-semibold">{{ r.temperature_c.toFixed(1) }}°C</td>
              <td class="p-3 text-indigo-400">{{ r.humidity_pct.toFixed(1) }}%</td>
              <td class="p-3 text-yellow-400">{{ r.light_level_pct.toFixed(1) }}%</td>
              <td class="p-3">
                <span
                  class="px-2 py-0.5 rounded text-[10px] font-bold"
                  :class="{
                    'bg-emerald-500/10 text-emerald-400': r.ai_class === 0,
                    'bg-cyan-500/10 text-cyan-400': r.ai_class === 1,
                    'bg-amber-500/10 text-amber-400': r.ai_class === 2,
                    'bg-purple-500/10 text-purple-400': r.ai_class === 3,
                    'bg-rose-500/10 text-rose-400': r.ai_class === 4
                  }"
                >
                  {{ getAiLabel(r.ai_class) }}
                </span>
              </td>
              <td class="p-3">
                <span
                  class="px-2 py-0.5 rounded text-[10px] font-bold"
                  :class="r.pump_state ? 'bg-cyan-500/15 text-cyan-400 border border-cyan-500/30' : 'bg-slate-800 text-slate-400'"
                >
                  {{ r.pump_state ? 'ON' : 'OFF' }}
                </span>
              </td>
            </tr>

            <tr v-if="readings.length === 0">
              <td colspan="9" class="p-8 text-center text-slate-500 font-mono">
                No telemetry packets recorded yet. Ensure devices or simulation are running.
              </td>
            </tr>
          </tbody>
        </table>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted } from 'vue'
import apiClient from '@/api/client'
import type { SensorReading, Device } from '@/types'

const readings = ref<SensorReading[]>([])
const devices = ref<Device[]>([])
const selectedDeviceId = ref('')

let pollTimer: any = null

const AI_LABELS: Record<number, string> = {
  0: 'NORMAL',
  1: 'NEED_WATER',
  2: 'HIGH_TEMP',
  3: 'LOW_LIGHT',
  4: 'SENSOR_FAULT'
}

function getAiLabel(classId: number): string {
  return AI_LABELS[classId] || 'UNKNOWN'
}

async function fetchTelemetry() {
  try {
    const url = selectedDeviceId.value
      ? `/telemetry/history?device_id=${selectedDeviceId.value}&limit=100`
      : '/telemetry/history?limit=100'
    const res = await apiClient.get<SensorReading[]>(url)
    readings.value = res.data
  } catch (err) {
    console.error('Failed to fetch telemetry history', err)
  }
}

async function fetchDevices() {
  try {
    const res = await apiClient.get<Device[]>('/devices')
    devices.value = res.data
  } catch (err) {
    console.error('Failed to fetch devices', err)
  }
}

const avgSoilMoisture = computed(() => {
  if (readings.value.length === 0) return 0
  const sum = readings.value.reduce((acc, r) => acc + r.soil_moisture_pct, 0)
  return sum / readings.value.length
})

const avgTemperature = computed(() => {
  if (readings.value.length === 0) return 0
  const sum = readings.value.reduce((acc, r) => acc + r.temperature_c, 0)
  return sum / readings.value.length
})

const minTemp = computed(() => {
  if (readings.value.length === 0) return 0
  return Math.min(...readings.value.map(r => r.temperature_c))
})

const maxTemp = computed(() => {
  if (readings.value.length === 0) return 0
  return Math.max(...readings.value.map(r => r.temperature_c))
})

const avgHumidity = computed(() => {
  if (readings.value.length === 0) return 0
  const sum = readings.value.reduce((acc, r) => acc + r.humidity_pct, 0)
  return sum / readings.value.length
})

const avgLight = computed(() => {
  if (readings.value.length === 0) return 0
  const sum = readings.value.reduce((acc, r) => acc + r.light_level_pct, 0)
  return sum / readings.value.length
})

onMounted(() => {
  fetchDevices()
  fetchTelemetry()
  pollTimer = setInterval(fetchTelemetry, 4000)
})

onUnmounted(() => {
  if (pollTimer) clearInterval(pollTimer)
})
</script>
