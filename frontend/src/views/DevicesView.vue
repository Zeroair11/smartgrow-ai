<template>
  <div class="space-y-6">
    <div class="flex flex-col sm:flex-row sm:items-center justify-between gap-4">
      <div>
        <h2 class="text-xl font-bold text-white tracking-tight">ESP32-S3 Edge Controllers</h2>
        <p class="text-xs text-slate-400 font-mono mt-0.5">Device fleet health, registration, credential revocation, and command history</p>
      </div>
      <div class="flex items-center gap-2">
        <button
          v-if="authStore.isAdmin || authStore.isOperator"
          @click="openRegisterModal"
          class="px-3.5 py-2 rounded-lg bg-emerald-500 hover:bg-emerald-600 text-white text-xs font-semibold shadow-md shadow-emerald-500/20 transition-all flex items-center gap-1.5"
        >
          <span>+</span> Register Device
        </button>
        <button
          @click="fetchDevices"
          class="px-3.5 py-2 rounded-lg bg-slate-800 hover:bg-slate-700 text-slate-200 text-xs font-semibold border border-slate-700 transition-all flex items-center gap-1.5"
        >
          <span>🔄</span> Refresh Fleet
        </button>
      </div>
    </div>

    <!-- Notification Banner -->
    <div v-if="notification" class="p-3 rounded-xl border text-xs font-mono flex items-center justify-between"
         :class="notification.type === 'success' ? 'bg-emerald-500/10 border-emerald-500/30 text-emerald-300' : 'bg-rose-500/10 border-rose-500/30 text-rose-300'">
      <span>{{ notification.message }}</span>
      <button @click="notification = null" class="text-slate-400 hover:text-white ml-2">✕</button>
    </div>

    <!-- Device Fleet Table & Filters -->
    <div class="rounded-2xl bg-slate-900/60 border border-slate-800/80 overflow-hidden backdrop-blur-sm">
      <div class="p-4 border-b border-slate-800 flex flex-col sm:flex-row sm:items-center justify-between gap-3">
        <div class="flex items-center gap-2">
          <span class="text-sm font-semibold text-white font-mono flex items-center gap-2">
            <span>📡</span> Registered Node Fleet
          </span>
          <span class="text-xs font-mono text-slate-400">({{ filteredDevices.length }} / {{ devices.length }})</span>
        </div>

        <div class="flex flex-wrap items-center gap-2">
          <!-- Search input -->
          <input
            v-model="searchQuery"
            type="text"
            placeholder="Search Device ID or Name..."
            class="px-2.5 py-1.5 rounded-lg bg-slate-950 border border-slate-800 text-xs text-white placeholder-slate-500 focus:outline-none focus:border-emerald-500 font-mono w-44 sm:w-56"
          />

          <!-- Status Filter -->
          <select
            v-model="statusFilter"
            class="px-2.5 py-1.5 rounded-lg bg-slate-950 border border-slate-800 text-xs text-white font-mono focus:outline-none focus:border-emerald-500"
          >
            <option value="all">All Statuses</option>
            <option value="online">Online</option>
            <option value="offline">Offline</option>
            <option value="revoked">Revoked</option>
          </select>

          <!-- Zone Filter -->
          <select
            v-model="zoneFilter"
            class="px-2.5 py-1.5 rounded-lg bg-slate-950 border border-slate-800 text-xs text-white font-mono focus:outline-none focus:border-emerald-500"
          >
            <option value="all">All Zones</option>
            <option v-for="z in zones" :key="z.id" :value="z.id">{{ z.name }}</option>
          </select>
        </div>
      </div>

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
            <tr v-for="dev in filteredDevices" :key="dev.id" class="hover:bg-slate-800/30 transition-colors">
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
                  :class="{
                    'bg-emerald-500/10 text-emerald-400 border-emerald-500/30': dev.status === 'online',
                    'bg-slate-800 text-slate-400 border-slate-700': dev.status === 'offline',
                    'bg-rose-500/10 text-rose-400 border-rose-500/30': dev.status === 'revoked',
                    'bg-amber-500/10 text-amber-400 border-amber-500/30': dev.status === 'degraded'
                  }"
                >
                  <span
                    class="w-1.5 h-1.5 rounded-full"
                    :class="{
                      'bg-emerald-400 animate-pulse': dev.status === 'online',
                      'bg-rose-400': dev.status === 'revoked',
                      'bg-amber-400': dev.status === 'degraded',
                      'bg-slate-500': dev.status === 'offline'
                    }"
                  ></span>
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
                {{ formatDateTime(dev.last_seen_at, 'Never') }}
              </td>

              <td class="p-4 text-right space-x-1.5 whitespace-nowrap">
                <button
                  @click="openEditModal(dev)"
                  class="px-2 py-1 rounded bg-slate-800 hover:bg-slate-700 text-slate-200 font-mono text-[11px] border border-slate-700 transition-all"
                >
                  Edit
                </button>
                <button
                  @click="openConfigModal(dev.device_id)"
                  class="px-2 py-1 rounded bg-slate-800 hover:bg-slate-700 text-slate-200 font-mono text-[11px] border border-slate-700 transition-all"
                >
                  Config
                </button>
                <button
                  @click="openCommandHistory(dev.device_id)"
                  class="px-2 py-1 rounded bg-slate-800 hover:bg-slate-700 text-slate-200 font-mono text-[11px] border border-slate-700 transition-all"
                >
                  Cmds
                </button>
                <button
                  @click="restartDevice(dev.device_id)"
                  :disabled="dev.status === 'revoked'"
                  class="px-2 py-1 rounded bg-rose-500/10 hover:bg-rose-500/20 text-rose-400 font-mono text-[11px] border border-rose-500/30 transition-all disabled:opacity-30 disabled:cursor-not-allowed"
                >
                  Reboot
                </button>
                <button
                  v-if="authStore.isAdmin && dev.status !== 'revoked'"
                  @click="confirmRevokeDevice(dev)"
                  class="px-2 py-1 rounded bg-amber-500/10 hover:bg-amber-500/20 text-amber-400 font-mono text-[11px] border border-amber-500/30 transition-all"
                  title="Revoke device credentials and block further communications"
                >
                  Revoke
                </button>
                <button
                  v-if="authStore.isAdmin"
                  @click="confirmDeleteDevice(dev)"
                  class="px-2 py-1 rounded bg-rose-900/20 hover:bg-rose-900/40 text-rose-300 font-mono text-[11px] border border-rose-800/40 transition-all"
                  title="Permanently remove device record"
                >
                  Delete
                </button>
              </td>
            </tr>

            <tr v-if="filteredDevices.length === 0">
              <td colspan="6" class="p-8 text-center text-slate-500 font-mono">
                No matching devices found in fleet.
              </td>
            </tr>
          </tbody>
        </table>
      </div>
    </div>

    <!-- Register Device Modal -->
    <div v-if="showRegisterModal" class="fixed inset-0 bg-black/60 backdrop-blur-sm flex items-center justify-center p-4 z-50">
      <div class="w-full max-w-md bg-slate-900 border border-slate-800 rounded-2xl p-6 shadow-2xl space-y-4">
        <h3 class="text-base font-bold text-white">Enroll ESP32-S3 Device</h3>
        <form @submit.prevent="submitRegisterDevice" class="space-y-4">
          <div>
            <label class="block text-xs font-mono text-slate-300 mb-1">Device ID (Hardware Identifier)</label>
            <input v-model="registerForm.device_id" required class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white font-mono focus:outline-none focus:border-emerald-500" placeholder="e.g. esp32s3_node_01" />
          </div>
          <div>
            <label class="block text-xs font-mono text-slate-300 mb-1">Display Name</label>
            <input v-model="registerForm.name" required class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white focus:outline-none focus:border-emerald-500" placeholder="e.g. Zone 1 Edge Node" />
          </div>
          <div>
            <label class="block text-xs font-mono text-slate-300 mb-1">Zone Assignment</label>
            <select v-model="registerForm.zone_id" class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white font-mono">
              <option :value="null">Unassigned</option>
              <option v-for="z in zones" :key="z.id" :value="z.id">{{ z.name }}</option>
            </select>
          </div>
          <div>
            <label class="block text-xs font-mono text-slate-300 mb-1">Registration Secret / Token</label>
            <input v-model="registerForm.registration_token" required class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white font-mono focus:outline-none focus:border-emerald-500" placeholder="Default: smartgrow-device-enrollment-secret-2026" />
          </div>
          <div class="flex justify-end gap-2 pt-2">
            <button type="button" @click="showRegisterModal = false" class="px-4 py-2 text-xs text-slate-400 hover:text-white">Cancel</button>
            <button type="submit" class="px-4 py-2 bg-emerald-500 hover:bg-emerald-600 text-white text-xs font-semibold rounded-lg">Register Device</button>
          </div>
        </form>
      </div>
    </div>

    <!-- Edit Device Modal -->
    <div v-if="editDeviceData" class="fixed inset-0 bg-black/60 backdrop-blur-sm flex items-center justify-center p-4 z-50">
      <div class="w-full max-w-md bg-slate-900 border border-slate-800 rounded-2xl p-6 shadow-2xl space-y-4">
        <h3 class="text-base font-bold text-white">Edit Device: {{ editDeviceData.device_id }}</h3>
        <form @submit.prevent="submitEditDevice" class="space-y-4">
          <div>
            <label class="block text-xs font-mono text-slate-300 mb-1">Display Name</label>
            <input v-model="editDeviceForm.name" required class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white focus:outline-none focus:border-emerald-500" />
          </div>
          <div>
            <label class="block text-xs font-mono text-slate-300 mb-1">Zone Assignment</label>
            <select v-model="editDeviceForm.zone_id" class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white font-mono">
              <option :value="null">Unassigned</option>
              <option v-for="z in zones" :key="z.id" :value="z.id">{{ z.name }}</option>
            </select>
          </div>
          <div>
            <label class="block text-xs font-mono text-slate-300 mb-1">Operational Status</label>
            <select v-model="editDeviceForm.status" class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white font-mono">
              <option value="online">Online</option>
              <option value="offline">Offline</option>
              <option value="degraded">Degraded</option>
              <option value="revoked">Revoked</option>
            </select>
          </div>
          <div class="flex justify-end gap-2 pt-2">
            <button type="button" @click="editDeviceData = null" class="px-4 py-2 text-xs text-slate-400 hover:text-white">Cancel</button>
            <button type="submit" class="px-4 py-2 bg-emerald-500 hover:bg-emerald-600 text-white text-xs font-semibold rounded-lg">Save Changes</button>
          </div>
        </form>
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
                <option value="RULE">RULE (Local Rules)</option>
                <option value="HYBRID">HYBRID (AI + Rules)</option>
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
                <td class="p-3 text-slate-400">{{ formatDateTime(cmd.created_at) }}</td>
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
import { ref, computed, onMounted } from 'vue'
import apiClient from '@/api/client'
import { useAuthStore } from '@/stores/authStore'
import type { Device, Zone, DeviceConfig, DeviceCommand } from '@/types'
import { formatDateTime } from '@/utils/date'

const authStore = useAuthStore()
const devices = ref<Device[]>([])
const zones = ref<Zone[]>([])
const selectedConfig = ref<DeviceConfig | null>(null)
const activeCommandsDeviceId = ref<string | null>(null)
const commandHistory = ref<DeviceCommand[]>([])
const notification = ref<{ type: 'success' | 'error'; message: string } | null>(null)

const showRegisterModal = ref(false)
const editDeviceData = ref<Device | null>(null)

const searchQuery = ref('')
const statusFilter = ref('all')
const zoneFilter = ref('all')

const registerForm = ref({
  device_id: '',
  name: '',
  zone_id: null as number | null,
  registration_token: 'smartgrow-device-enrollment-secret-2026'
})

const editDeviceForm = ref({
  name: '',
  zone_id: null as number | null,
  status: 'online'
})

const filteredDevices = computed(() => {
  return devices.value.filter(dev => {
    if (searchQuery.value) {
      const q = searchQuery.value.toLowerCase()
      const matchId = dev.device_id.toLowerCase().includes(q)
      const matchName = dev.name.toLowerCase().includes(q)
      if (!matchId && !matchName) return false
    }
    if (statusFilter.value !== 'all' && dev.status !== statusFilter.value) {
      return false
    }
    if (zoneFilter.value !== 'all') {
      const zId = Number(zoneFilter.value)
      if (dev.zone_id !== zId) return false
    }
    return true
  })
})

function notify(type: 'success' | 'error', message: string) {
  notification.value = { type, message }
  setTimeout(() => {
    if (notification.value?.message === message) {
      notification.value = null
    }
  }, 5000)
}

async function fetchDevices() {
  try {
    const [devRes, zoneRes] = await Promise.all([
      apiClient.get<Device[]>('/devices'),
      apiClient.get<Zone[]>('/zones')
    ])
    devices.value = devRes.data
    zones.value = zoneRes.data
  } catch (err: any) {
    console.error('Failed to load devices/zones', err)
    notify('error', err.response?.data?.detail || 'Failed to load devices/zones')
  }
}

function openRegisterModal() {
  registerForm.value = {
    device_id: '',
    name: '',
    zone_id: null,
    registration_token: 'smartgrow-device-enrollment-secret-2026'
  }
  showRegisterModal.value = true
}

async function submitRegisterDevice() {
  try {
    await apiClient.post('/devices/register', registerForm.value)
    notify('success', `Device ${registerForm.value.device_id} registered successfully`)
    showRegisterModal.value = false
    await fetchDevices()
  } catch (err: any) {
    notify('error', err.response?.data?.detail || 'Failed to register device')
  }
}

function openEditModal(dev: Device) {
  editDeviceData.value = dev
  editDeviceForm.value = {
    name: dev.name,
    zone_id: dev.zone_id ?? null,
    status: dev.status
  }
}

async function submitEditDevice() {
  if (!editDeviceData.value) return
  try {
    await apiClient.patch(`/devices/${editDeviceData.value.device_id}`, editDeviceForm.value)
    notify('success', `Device ${editDeviceData.value.device_id} updated successfully`)
    editDeviceData.value = null
    await fetchDevices()
  } catch (err: any) {
    notify('error', err.response?.data?.detail || 'Failed to update device')
  }
}

async function confirmRevokeDevice(dev: Device) {
  const confirmMsg = `WARNING: Revoking credentials for "${dev.device_id}" will invalidate its token and block all telemetry and command execution. Continue?`
  if (!confirm(confirmMsg)) return
  try {
    await apiClient.post(`/devices/${dev.device_id}/revoke`)
    notify('success', `Credentials revoked for ${dev.device_id}. Status set to REVOKED.`)
    await fetchDevices()
  } catch (err: any) {
    notify('error', err.response?.data?.detail || 'Failed to revoke device')
  }
}

async function confirmDeleteDevice(dev: Device) {
  const confirmMsg = `CAUTION: Are you sure you want to permanently delete device "${dev.device_id}" from the database?`
  if (!confirm(confirmMsg)) return
  try {
    await apiClient.delete(`/devices/${dev.device_id}`)
    notify('success', `Device ${dev.device_id} successfully deleted`)
    await fetchDevices()
  } catch (err: any) {
    notify('error', err.response?.data?.detail || 'Failed to delete device')
  }
}

async function onZoneChange(deviceId: string, zoneIdStr: string) {
  const zoneId = zoneIdStr ? parseInt(zoneIdStr) : null
  try {
    const url = zoneId !== null ? `/devices/${deviceId}/zone?zone_id=${zoneId}` : `/devices/${deviceId}/zone`
    await apiClient.put(url)
    await fetchDevices()
  } catch (err: any) {
    notify('error', err.response?.data?.detail || 'Failed to update device zone')
  }
}

async function openConfigModal(deviceId: string) {
  try {
    const res = await apiClient.get<DeviceConfig>(`/devices/${deviceId}/config`)
    selectedConfig.value = res.data
  } catch (err: any) {
    notify('error', err.response?.data?.detail || 'Failed to load configuration')
  }
}

async function submitSaveConfig() {
  if (!selectedConfig.value) return
  try {
    await apiClient.put(`/devices/${selectedConfig.value.device_id}/config`, selectedConfig.value)
    notify('success', 'Configuration saved successfully! Sent to device on next sync.')
    selectedConfig.value = null
  } catch (err: any) {
    notify('error', err.response?.data?.detail || 'Failed to save configuration')
  }
}

async function openCommandHistory(deviceId: string) {
  activeCommandsDeviceId.value = deviceId
  try {
    const res = await apiClient.get<DeviceCommand[]>(`/devices/${deviceId}/commands`)
    commandHistory.value = res.data
  } catch (err: any) {
    notify('error', err.response?.data?.detail || 'Failed to load command history')
  }
}

async function restartDevice(deviceId: string) {
  if (!confirm(`Send reboot command to controller ${deviceId}?`)) return
  try {
    await apiClient.post(`/devices/${deviceId}/restart`)
    notify('success', `Reboot command dispatched to ${deviceId}`)
  } catch (err: any) {
    notify('error', err.response?.data?.detail || 'Failed to send reboot command')
  }
}

onMounted(() => {
  fetchDevices()
})
</script>
