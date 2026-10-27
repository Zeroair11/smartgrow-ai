<template>
  <div class="space-y-6">
    <div class="flex flex-col sm:flex-row sm:items-center justify-between gap-4">
      <div>
        <h2 class="text-xl font-bold text-white tracking-tight">Alerts & Incident Response</h2>
        <p class="text-xs text-slate-400 font-mono mt-0.5">Sensor threshold breaches, connectivity disconnects and safety trips</p>
      </div>

      <div class="flex items-center gap-2">
        <select
          v-model="severityFilter"
          @change="fetchAlerts"
          class="px-3 py-1.5 rounded-lg bg-slate-900 border border-slate-800 text-xs font-mono text-slate-200 focus:outline-none focus:border-emerald-500"
        >
          <option value="">All Severities</option>
          <option value="critical">Critical</option>
          <option value="warning">Warning</option>
          <option value="info">Info</option>
        </select>

        <select
          v-model="resolvedFilter"
          @change="fetchAlerts"
          class="px-3 py-1.5 rounded-lg bg-slate-900 border border-slate-800 text-xs font-mono text-slate-200 focus:outline-none focus:border-emerald-500"
        >
          <option value="false">Active Only</option>
          <option value="true">Resolved</option>
          <option value="">All Incidents</option>
        </select>

        <button
          @click="fetchAlerts"
          class="px-3 py-1.5 rounded-lg bg-slate-800 hover:bg-slate-700 text-slate-200 text-xs font-semibold border border-slate-700 transition-all"
        >
          Refresh
        </button>
      </div>
    </div>

    <!-- Alerts Table -->
    <div class="rounded-2xl bg-slate-900/60 border border-slate-800/80 overflow-hidden backdrop-blur-sm">
      <div class="overflow-x-auto">
        <table class="w-full text-left text-xs border-collapse">
          <thead>
            <tr class="border-b border-slate-800 bg-slate-900/80 font-mono text-slate-400">
              <th class="p-3.5">Severity</th>
              <th class="p-3.5">Category</th>
              <th class="p-3.5">Incident Title & Details</th>
              <th class="p-3.5">Origin Device</th>
              <th class="p-3.5">Raised At</th>
              <th class="p-3.5">Status</th>
              <th class="p-3.5 text-right">Actions</th>
            </tr>
          </thead>
          <tbody class="divide-y divide-slate-800/60 font-mono text-[11px]">
            <tr v-for="al in alerts" :key="al.id" class="hover:bg-slate-800/30 transition-colors">
              <td class="p-3.5">
                <span
                  class="px-2 py-0.5 rounded text-[10px] font-bold uppercase"
                  :class="{
                    'bg-rose-500/10 text-rose-400 border border-rose-500/30': al.severity === 'critical',
                    'bg-amber-500/10 text-amber-400 border border-amber-500/30': al.severity === 'warning',
                    'bg-blue-500/10 text-blue-400 border border-blue-500/30': al.severity === 'info'
                  }"
                >
                  {{ al.severity }}
                </span>
              </td>

              <td class="p-3.5 text-slate-400 uppercase text-[10px]">
                {{ al.category }}
              </td>

              <td class="p-3.5">
                <p class="font-bold text-white text-xs">{{ al.title }}</p>
                <p class="text-slate-400 text-[11px] font-sans mt-0.5">{{ al.message }}</p>
              </td>

              <td class="p-3.5 text-emerald-400">
                {{ al.device_id || 'System-wide' }}
              </td>

              <td class="p-3.5 text-slate-400">
                {{ formatDateTime(al.created_at) }}
              </td>

              <td class="p-3.5">
                <span v-if="al.is_resolved" class="text-emerald-400 font-bold">RESOLVED</span>
                <span v-else-if="al.is_acknowledged" class="text-amber-400 font-bold">ACKNOWLEDGED</span>
                <span v-else class="text-rose-400 font-bold animate-pulse">ACTIVE</span>
              </td>

              <td class="p-3.5 text-right space-x-2">
                <button
                  v-if="!al.is_acknowledged && !al.is_resolved"
                  @click="acknowledgeAlert(al.id)"
                  class="px-2.5 py-1 rounded bg-slate-800 hover:bg-slate-700 text-slate-200 border border-slate-700 transition-all text-[11px]"
                >
                  Acknowledge
                </button>
                <button
                  v-if="!al.is_resolved"
                  @click="resolveAlert(al.id)"
                  class="px-2.5 py-1 rounded bg-emerald-500/10 hover:bg-emerald-500/20 text-emerald-400 border border-emerald-500/30 transition-all text-[11px]"
                >
                  Resolve
                </button>
              </td>
            </tr>

            <tr v-if="alerts.length === 0">
              <td colspan="7" class="p-8 text-center text-slate-500 font-mono">
                No alerts found matching filter criteria.
              </td>
            </tr>
          </tbody>
        </table>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'
import apiClient from '@/api/client'
import type { Alert } from '@/types'
import { formatDateTime } from '@/utils/date'

const alerts = ref<Alert[]>([])
const severityFilter = ref('')
const resolvedFilter = ref('false')

async function fetchAlerts() {
  try {
    const params = new URLSearchParams()
    if (severityFilter.value) params.append('severity', severityFilter.value)
    if (resolvedFilter.value !== '') params.append('is_resolved', resolvedFilter.value)

    const res = await apiClient.get<Alert[]>(`/alerts?${params.toString()}`)
    alerts.value = res.data
  } catch (err) {
    console.error('Failed to load alerts', err)
  }
}

async function acknowledgeAlert(id: number) {
  try {
    await apiClient.post(`/alerts/${id}/acknowledge`)
    await fetchAlerts()
  } catch (err: any) {
    alert(err.response?.data?.detail || 'Failed to acknowledge alert')
  }
}

async function resolveAlert(id: number) {
  try {
    await apiClient.post(`/alerts/${id}/resolve`)
    await fetchAlerts()
  } catch (err: any) {
    alert(err.response?.data?.detail || 'Failed to resolve alert')
  }
}

onMounted(() => {
  fetchAlerts()
})
</script>
