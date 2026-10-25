<template>
  <div class="space-y-6">
    <div class="flex flex-col sm:flex-row sm:items-center justify-between gap-4">
      <div>
        <h2 class="text-xl font-bold text-white tracking-tight">ESP32-S3 Edge Controllers</h2>
        <p class="text-xs text-slate-400 font-mono mt-0.5">Device fleet health, firmware versions, calibration and command history</p>
      </div>
      <div class="flex items-center gap-2">
        <button
          @click="fetchDevices"
          class="px-3.5 py-2 rounded-lg bg-slate-800 hover:bg-slate-700 text-slate-200 text-xs font-semibold border border-slate-700 transition-all flex items-center gap-1.5"
        >
          <span>🔄</span> Refresh Fleet
        </button>
      </div>
    </div>

    <!-- Device Fleet Table -->
    <div class="rounded-2xl bg-slate-900/60 border border-slate-800/80 overflow-hidden backdrop-blur-sm">
      <div class="overflow-x-auto">
        <table class="w-full text-left border-collapse text-xs">
          <thead>
            <tr class="border-b border-slate-800 bg-slate-900/80 text-slate-400 font-mono">
              <th class="p-4 font-semibold">Device ID & Name</th>
              <th class="p-4 font-semibold">Status</th>
              <th class="p-4 font-semibold">Zone</th>
              <th class="p-4 font-semibold">Firmware / Model</th>
              <th class="p-4 font-semibold">Last Seen</th>
              <th class="p-4 font-semibold text-right">Actions</th>
            </tr>
          </thead>
          <tbody class="divide-y divide-slate-800/60">
            <tr v-for="dev in devices" :key="dev.id" class="hover:bg-slate-800/30 transition-colors">
              <td class="p-4">
                <div class="font-bold text-white flex items-center gap-2">
                  <span>📟</span>
                  <span>{{ dev.name }}</span>
                </div>
                <div class="text-[11px] font-mono text-slate-400 mt-0.5">{{ dev.device_id }}</div>
              </td>

              <td class="p-4">
                <span
                  class="inline-flex items-center gap-1.5 px-2.5 py-1 rounded-full text-[11px] font-mono font-medium border"
                  :class="dev.status === 'online' ? 'bg-emerald-500/10 text-emerald-400 border-emerald-500/30' : 'bg-slate-800 text-slate-400 border-slate-700'"
                >
                  <span class="w-1.5 h-1.5 rounded-full" :class="dev.status === 'online' ? 'bg-emerald-400 animate-pulse' : 'bg-slate-500'"></span>
                  {{ dev.status.toUpperCase() }}
                </span>
              </td>

              <td class="p-4 text-slate-300">
                <select
                  :value="dev.zone_id"
                  @change="onZoneChange(dev.device_id, ($event.target as HTMLSelectElement).value)"
                  class="bg-slate-950 border border-slate-800 rounded px-2 py-1 text-xs text-slate-300 focus:outline-none focus:border-emerald-500"
                >
                  <option :value="null">Unassigned</option>
                  <option v-for="z in zones" :key="z.id" :value="z.id">{{ z.name }}</option>
                </select>
              </td>

              <td class="p-4 font-mono">
                <div class="text-slate-200">FW: v{{ dev.firmware_version }}</div>
                <div class="text-[11px] text-slate-400">ML: {{ dev.model_version }}</div>
              </td>

              <td class="p-4 font-mono text-slate-400">
                {{ dev.last_seen_at ? new Date(dev.last_seen_at).toLocaleTimeString() : 'Never' }}
              </td>

              <td class="p-4 text-right space-x-2">
                <button
                  @click="openConfigModal(dev.device_id)"
                  class="px-2.5 py-1 rounded bg-slate-800 hover:bg-slate-700 text-slate-200 font-mono text-[11px] border border-slate-700 transition-all"
                >
                  Config
                </button>
                <button
                  @click="openCommandHistory(dev.device_id)"
                  class="px-2.5 py-1 rounded bg-slate-800 hover:bg-slate-700 text-slate-200 font-mono text-[11px] border border-slate-700 transition-all"
                >
                  Commands
                </button>
                <button
                  @click="restartDevice(dev.device_id)"
                  class="px-2.5 py-1 rounded bg-rose-500/10 hover:bg-rose-500/20 text-rose-400 font-mono text-[11px] border border-rose-500/30 transition-all"
                >
                  Reboot
                </button>
              </td>
            </tr>

            <tr v-if="devices.length === 0">
              <td colspan="6" class="p-8 text-center text-slate-500 font-mono">
                No ESP32-S3 devices enrolled yet. Run device registration or launch the simulator.
              </td>
            </tr>
          </tbody>
        </table>
      </div>
    </div>

    <!-- Device Config Modal -->
    <div v-if="selectedConfig" class="fixed inset-0 bg-black/60 backdrop-blur-sm flex items-center justify-center p-4 z-50">
      <div class="w-full max-w-lg bg-slate-900 border border-slate-800 rounded-2xl p-6 shadow-2xl">
        <h3 class="text-base font-bold text-white mb-1">Device Configuration</h3>
        <p class="text-xs font-mono text-slate-400 mb-4">Controller: {{ selectedConfig.device_id }}</p>

        <form @submit.prevent="submitSaveConfig" class="space-y-4">
          <div class="grid grid-cols-2 gap-4">
            <div>
              <label class="block text-xs font-mono text-slate-300 mb-1">Telemetry Rate (sec)</label>
              <input v-model.number="selectedConfig.telemetry_interval_s" type="number" min="1" max="60" required class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white font-mono" />
            </div>
            <div>
              <label class="block text-xs font-mono text-slate-300 mb-1">Operating Mode</label>
              <select v-model="selectedConfig.operating_mode" class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white font-mono">
                <option value="AUTO">AUTO (Edge AI)</option>
                <option value="MANUAL">MANUAL</option>
                <option value="SAFE">SAFE (Failsafe)</option>
              </select>
            </div>
          </div>

          <div class="p-3 bg-slate-950/60 rounded-xl border border-slate-800/80 space-y-3">
            <p class="text-xs font-mono font-semibold text-emerald-400">Firmware Safety Guardrails</p>
            <div class="grid grid-cols-2 gap-4">
              <div>
                <label class="block text-xs font-mono text-slate-300 mb-1">Max Pump Run (s)</label>
                <input v-model.number="selectedConfig.safety_max_runtime_s" type="number" min="10" max="120" required class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white font-mono" />
              </div>
              <div>
                <label class="block text-xs font-mono text-slate-300 mb-1">Min Cooldown (s)</label>
                <input v-model.number="selectedConfig.safety_min_cooldown_s" type="number" min="30" max="600" required class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white font-mono" />
              </div>
            </div>
          </div>

          <div class="p-3 bg-slate-950/60 rounded-xl border border-slate-800/80 space-y-3">
            <p class="text-xs font-mono font-semibold text-cyan-400">Soil Moisture ADC Calibration</p>
            <div class="grid grid-cols-2 gap-4">
              <div>
                <label class="block text-xs font-mono text-slate-300 mb-1">Dry Air ADC (0%)</label>
                <input v-model.number="selectedConfig.soil_dry_adc" type="number" required class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white font-mono" />
              </div>
              <div>
                <label class="block text-xs font-mono text-slate-300 mb-1">Water ADC (100%)</label>
                <input v-model.number="selectedConfig.soil_wet_adc" type="number" required class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white font-mono" />
              </div>
            </div>
          </div>

          <div class="flex justify-end gap-2 pt-2">
            <button type="button" @click="selectedConfig = null" class="px-4 py-2 text-xs text-slate-400 hover:text-white">Cancel</button>
            <button type="submit" class="px-4 py-2 bg-emerald-500 hover:bg-emerald-600 text-white text-xs font-semibold rounded-lg">Save Configuration</button>
          </div>
        </form>
      </div>
    </div>

    <!-- Command History Modal -->
    <div v-if="activeCommandsDeviceId" class="fixed inset-0 bg-black/60 backdrop-blur-sm flex items-center justify-center p-4 z-50">
      <div class="w-full max-w-2xl bg-slate-900 border border-slate-800 rounded-2xl p-6 shadow-2xl space-y-4">
        <div class="flex items-center justify-between">
          <div>
            <h3 class="text-base font-bold text-white">Downlink Command History</h3>
            <p class="text-xs font-mono text-slate-400">Device: {{ activeCommandsDeviceId }}</p>
          </div>
          <button @click="activeCommandsDeviceId = null" class="text-slate-400 hover:text-white text-sm">✕</button>
        </div>

        <div class="max-h-80 overflow-y-auto rounded-xl border border-slate-800 bg-slate-950/60">
          <table class="w-full text-left text-xs border-collapse">
            <thead>
              <tr class="border-b border-slate-800 bg-slate-900 font-mono text-slate-400">
                <th class="p-3">Command ID</th>
                <th class="p-3">Type</th>
                <th class="p-3">Status</th>
                <th class="p-3">Time</th>
                <th class="p-3">Result</th>
              </tr>
            </thead>
            <tbody class="divide-y divide-slate-800/60 font-mono text-[11px]">
              <tr v-for="cmd in commandHistory" :key="cmd.id" class="hover:bg-slate-800/30">
                <td class="p-3 text-slate-300 font-semibold">{{ cmd.command_id }}</td>
                <td class="p-3 text-emerald-400">{{ cmd.command_type }}</td>
                <td class="p-3">
                  <span
                    class="px-2 py-0.5 rounded text-[10px]"
                    :class="{
                      'bg-emerald-500/10 text-emerald-400 border border-emerald-500/30': cmd.status === 'completed',
                      'bg-amber-500/10 text-amber-400 border border-amber-500/30': cmd.status === 'pending',
                      'bg-rose-500/10 text-rose-400 border border-rose-500/30': ['failed', 'rejected', 'expired'].includes(cmd.status)
                    }"
                  >
                    {{ cmd.status }}
                  </span>
                </td>
                <td class="p-3 text-slate-400">{{ new Date(cmd.created_at).toLocaleTimeString() }}</td>
                <td class="p-3 text-slate-300 truncate max-w-xs">{{ cmd.result_json || 'Pending ACK' }}</td>
              </tr>
              <tr v-if="commandHistory.length === 0">
                <td colspan="5" class="p-4 text-center text-slate-500">No commands issued yet.</td>
              </tr>
            </tbody>
          </table>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'
import apiClient from '@/api/client'
import type { Device, Zone, DeviceConfig, DeviceCommand } from '@/types'

const devices = ref<Device[]>([])
const zones = ref<Zone[]>([])
const selectedConfig = ref<DeviceConfig | null>(null)
const activeCommandsDeviceId = ref<string | null>(null)
const commandHistory = ref<DeviceCommand[]>([])

async function fetchDevices() {
  try {
    const [devRes, zoneRes] = await Promise.all([
      apiClient.get<Device[]>('/devices'),
      apiClient.get<Zone[]>('/zones')
    ])
    devices.value = devRes.data
    zones.value = zoneRes.data
  } catch (err) {
    console.error('Failed to load devices/zones', err)
  }
}

async function onZoneChange(deviceId: string, zoneIdStr: string) {
  const zoneId = zoneIdStr ? parseInt(zoneIdStr) : null
  try {
    await apiClient.put(`/devices/${deviceId}/zone?zone_id=${zoneId || ''}`)
    await fetchDevices()
  } catch (err: any) {
    alert(err.response?.data?.detail || 'Failed to update device zone')
  }
}

async function openConfigModal(deviceId: string) {
  try {
    const res = await apiClient.get<DeviceConfig>(`/devices/${deviceId}/config`)
    selectedConfig.value = res.data
  } catch (err: any) {
    alert(err.response?.data?.detail || 'Failed to load configuration')
  }
}

async function submitSaveConfig() {
  if (!selectedConfig.value) return
  try {
    await apiClient.put(`/devices/${selectedConfig.value.device_id}/config`, selectedConfig.value)
    alert('Configuration saved successfully! Sent to device on next sync.')
    selectedConfig.value = null
  } catch (err: any) {
    alert(err.response?.data?.detail || 'Failed to save configuration')
  }
}

async function openCommandHistory(deviceId: string) {
  activeCommandsDeviceId.value = deviceId
  try {
    const res = await apiClient.get<DeviceCommand[]>(`/devices/${deviceId}/commands`)
    commandHistory.value = res.data
  } catch (err: any) {
    alert(err.response?.data?.detail || 'Failed to load command history')
  }
}

async function restartDevice(deviceId: string) {
  if (!confirm(`Send reboot command to controller ${deviceId}?`)) return
  try {
    await apiClient.post(`/devices/${deviceId}/restart`)
    alert(`Reboot command dispatched to ${deviceId}`)
  } catch (err: any) {
    alert(err.response?.data?.detail || 'Failed to send reboot command')
  }
}

onMounted(() => {
  fetchDevices()
})
</script>
