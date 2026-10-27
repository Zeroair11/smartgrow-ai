<template>
  <div class="space-y-6">
    <div class="flex flex-col sm:flex-row sm:items-center justify-between gap-4">
      <div>
        <h2 class="text-xl font-bold text-white tracking-tight">Irrigation Management & Safety Guardrails</h2>
        <p class="text-xs text-slate-400 font-mono mt-0.5">Control water pumps with hard firmware runtime cutoffs and cooldown timers</p>
      </div>

      <div class="flex items-center gap-2">
        <button
          @click="fetchIrrigationHistory"
          class="px-3.5 py-2 rounded-lg bg-slate-800 hover:bg-slate-700 text-slate-200 text-xs font-semibold border border-slate-700 transition-all flex items-center gap-1.5"
        >
          <span>🔄</span> Refresh Logs
        </button>
      </div>
    </div>

    <!-- Pump Manual Control Center Card -->
    <div class="p-6 rounded-2xl bg-slate-900/60 border border-slate-800/80 backdrop-blur-sm space-y-6">
      <div class="flex items-center justify-between">
        <h3 class="text-base font-bold text-white flex items-center gap-2">
          <span>💧</span> Manual Pump Command Center
        </h3>
        <span class="text-xs font-mono text-amber-400/90 bg-amber-500/10 px-2.5 py-1 rounded border border-amber-500/30">
          Max Hardware Runtime: 120s • Cooldown: 180s
        </span>
      </div>

      <div class="grid grid-cols-1 md:grid-cols-3 gap-4">
        <div>
          <label class="block text-xs font-mono text-slate-300 mb-1">Target Controller</label>
          <select
            v-model="selectedDevice"
            class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white font-mono focus:outline-none focus:border-emerald-500"
          >
            <option v-for="dev in devices" :key="dev.id" :value="dev.device_id">
              {{ dev.name }} ({{ dev.device_id }}) - {{ dev.status.toUpperCase() }}
            </option>
          </select>
        </div>

        <div>
          <label class="block text-xs font-mono text-slate-300 mb-1">Duration (Seconds)</label>
          <input
            v-model.number="durationSeconds"
            type="number"
            min="5"
            max="120"
            class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white font-mono focus:outline-none focus:border-emerald-500"
          />
        </div>

        <div class="flex items-end gap-2">
          <button
            @click="startPump"
            :disabled="isSubmitting || !selectedDevice"
            class="flex-1 py-2 px-4 rounded-lg bg-emerald-500 hover:bg-emerald-600 active:scale-[0.99] text-white text-xs font-semibold shadow-lg shadow-emerald-500/20 transition-all disabled:opacity-50"
          >
            Start Pump
          </button>
          <button
            @click="stopPump"
            :disabled="isSubmitting || !selectedDevice"
            class="py-2 px-4 rounded-lg bg-rose-500 hover:bg-rose-600 active:scale-[0.99] text-white text-xs font-semibold shadow-lg shadow-rose-500/20 transition-all disabled:opacity-50"
          >
            Force Cutoff
          </button>
        </div>
      </div>

      <!-- Real-Time Command Lifecycle Tracker -->
      <div v-if="activeCommand" class="p-4 rounded-xl bg-slate-950/80 border border-slate-800 space-y-3">
        <div class="flex items-center justify-between text-xs font-mono">
          <span class="text-slate-400">Command: <strong class="text-white">#{{ activeCommand.command_id }}</strong></span>
          <span
            class="px-2 py-0.5 rounded text-[11px] font-bold"
            :class="{
              'bg-amber-500/10 text-amber-400': activeCommand.status === 'pending',
              'bg-blue-500/10 text-blue-400': activeCommand.status === 'acknowledged',
              'bg-emerald-500/10 text-emerald-400': activeCommand.status === 'completed',
              'bg-rose-500/10 text-rose-400': ['failed', 'rejected', 'expired'].includes(activeCommand.status)
            }"
          >
            {{ activeCommand.status.toUpperCase() }}
          </span>
        </div>

        <!-- Progress step indicator -->
        <div class="flex items-center justify-between text-[11px] font-mono text-slate-400 pt-1">
          <span :class="stepIndex >= 1 ? 'text-emerald-400 font-bold' : ''">1. Dispatched</span>
          <span class="text-slate-600">&rarr;</span>
          <span :class="stepIndex >= 2 ? 'text-emerald-400 font-bold' : ''">2. Firmware Polled</span>
          <span class="text-slate-600">&rarr;</span>
          <span :class="stepIndex >= 3 ? 'text-emerald-400 font-bold' : ''">3. Safety Checked</span>
          <span class="text-slate-600">&rarr;</span>
          <span :class="stepIndex >= 4 ? 'text-emerald-400 font-bold' : ''">4. Executed & ACK</span>
        </div>

        <p v-if="feedbackMsg" class="text-xs font-mono text-emerald-400 mt-1">{{ feedbackMsg }}</p>
      </div>
    </div>

    <!-- Irrigation History Cycles Table -->
    <div class="rounded-2xl bg-slate-900/60 border border-slate-800/80 overflow-hidden backdrop-blur-sm space-y-2">
      <div class="p-4 border-b border-slate-800 flex items-center justify-between">
        <h3 class="text-sm font-semibold text-white font-mono flex items-center gap-2">
          <span>📜</span> Irrigation Cycles & Execution Logs
        </h3>
        <span class="text-xs font-mono text-slate-400">{{ events.length }} Events Recorded</span>
      </div>

      <div class="overflow-x-auto">
        <table class="w-full text-left text-xs border-collapse">
          <thead>
            <tr class="border-b border-slate-800 bg-slate-900/80 font-mono text-slate-400">
              <th class="p-3">Event ID</th>
              <th class="p-3">Device</th>
              <th class="p-3">Trigger Source</th>
              <th class="p-3">Started At</th>
              <th class="p-3">Duration</th>
              <th class="p-3">Est. Volume</th>
              <th class="p-3">Stop Reason</th>
              <th class="p-3">Status</th>
            </tr>
          </thead>
          <tbody class="divide-y divide-slate-800/60 font-mono text-[11px]">
            <tr v-for="ev in events" :key="ev.id" class="hover:bg-slate-800/30">
              <td class="p-3 text-slate-300 font-semibold">#IRR-{{ ev.id }}</td>
              <td class="p-3 text-emerald-400">{{ ev.device_id }}</td>
              <td class="p-3">
                <span class="px-2 py-0.5 rounded bg-slate-800 text-slate-300">{{ ev.start_reason }}</span>
              </td>
              <td class="p-3 text-slate-400">{{ formatDateTime(ev.started_at) }}</td>
              <td class="p-3 text-cyan-400 font-bold">{{ ev.duration_seconds }}s</td>
              <td class="p-3 text-slate-300">{{ ev.volume_liters_est.toFixed(2) }} L</td>
              <td class="p-3 text-slate-400">{{ ev.stop_reason }}</td>
              <td class="p-3">
                <span
                  class="px-2 py-0.5 rounded text-[10px] font-bold"
                  :class="ev.status === 'completed' ? 'bg-emerald-500/10 text-emerald-400' : 'bg-cyan-500/10 text-cyan-400 animate-pulse'"
                >
                  {{ ev.status.toUpperCase() }}
                </span>
              </td>
            </tr>
            <tr v-if="events.length === 0">
              <td colspan="8" class="p-8 text-center text-slate-500 font-mono">
                No irrigation cycles recorded yet.
              </td>
            </tr>
          </tbody>
        </table>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, onMounted } from 'vue'
import apiClient from '@/api/client'
import type { Device, IrrigationEvent, DeviceCommand } from '@/types'
import { formatDateTime } from '@/utils/date'

const devices = ref<Device[]>([])
const selectedDevice = ref('')
const durationSeconds = ref(30)
const isSubmitting = ref(false)
const feedbackMsg = ref('')
const activeCommand = ref<DeviceCommand | null>(null)
const events = ref<IrrigationEvent[]>([])

const stepIndex = computed(() => {
  if (!activeCommand.value) return 0
  if (activeCommand.value.status === 'completed') return 4
  if (activeCommand.value.status === 'executing') return 3
  if (activeCommand.value.status === 'acknowledged') return 2
  return 1
})

async function fetchDevices() {
  try {
    const res = await apiClient.get<Device[]>('/devices')
    devices.value = res.data
    if (devices.value.length > 0 && !selectedDevice.value) {
      selectedDevice.value = devices.value[0].device_id
    }
  } catch (err) {
    console.error('Failed to load devices', err)
  }
}

async function fetchIrrigationHistory() {
  try {
    const res = await apiClient.get<IrrigationEvent[]>('/irrigation/events')
    events.value = res.data
  } catch (err) {
    console.error('Failed to load irrigation events', err)
  }
}

async function startPump() {
  if (!selectedDevice.value) return
  isSubmitting.value = true
  feedbackMsg.value = 'Transmitting command to backend platform...'
  try {
    const res = await apiClient.post<DeviceCommand>('/irrigation/start', {
      device_id: selectedDevice.value,
      duration_seconds: durationSeconds.value
    })
    activeCommand.value = res.data
    feedbackMsg.value = `Command #${res.data.command_id} dispatched. Waiting for ESP32-S3 execution...`
    await fetchIrrigationHistory()
  } catch (err: any) {
    feedbackMsg.value = `Error: ${err.response?.data?.detail || err.message}`
  } finally {
    isSubmitting.value = false
  }
}

async function stopPump() {
  if (!selectedDevice.value) return
  isSubmitting.value = true
  feedbackMsg.value = 'Transmitting stop command...'
  try {
    const res = await apiClient.post<DeviceCommand>('/irrigation/stop', {
      device_id: selectedDevice.value
    })
    activeCommand.value = res.data
    feedbackMsg.value = `Emergency stop command #${res.data.command_id} dispatched.`
    await fetchIrrigationHistory()
  } catch (err: any) {
    feedbackMsg.value = `Error: ${err.response?.data?.detail || err.message}`
  } finally {
    isSubmitting.value = false
  }
}

onMounted(() => {
  fetchDevices()
  fetchIrrigationHistory()
})
</script>
