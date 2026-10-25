<template>
  <div class="space-y-6">
    <div class="flex flex-col sm:flex-row sm:items-center justify-between gap-4">
      <div>
        <h2 class="text-xl font-bold text-white tracking-tight">Environmental Analytics & Fleet Health</h2>
        <p class="text-xs text-slate-400 font-mono mt-0.5">Historical aggregations, uptime reliability and telemetry exports</p>
      </div>

      <div class="flex items-center gap-2">
        <a
          href="/api/v1/telemetry/export-csv"
          target="_blank"
          class="px-3.5 py-2 rounded-lg bg-emerald-500 hover:bg-emerald-600 text-white text-xs font-semibold shadow-md shadow-emerald-500/20 transition-all flex items-center gap-1.5"
        >
          <span>📥</span> Download Full Dataset (CSV)
        </a>
      </div>
    </div>

    <!-- Fleet Health Overview Table -->
    <div class="rounded-2xl bg-slate-900/60 border border-slate-800/80 overflow-hidden backdrop-blur-sm">
      <div class="p-4 border-b border-slate-800">
        <h3 class="text-sm font-semibold text-white font-mono flex items-center gap-2">
          <span>🩺</span> Controller Health & Reliability Metrics
        </h3>
      </div>

      <div class="overflow-x-auto">
        <table class="w-full text-left text-xs border-collapse">
          <thead>
            <tr class="border-b border-slate-800 bg-slate-900/80 font-mono text-slate-400">
              <th class="p-3.5">Device ID</th>
              <th class="p-3.5">Name</th>
              <th class="p-3.5">Status</th>
              <th class="p-3.5">Total Packets</th>
              <th class="p-3.5">Open Alerts</th>
              <th class="p-3.5">Uptime</th>
              <th class="p-3.5">Firmware / Model</th>
            </tr>
          </thead>
          <tbody class="divide-y divide-slate-800/60 font-mono text-[11px]">
            <tr v-for="h in healthData" :key="h.device_id" class="hover:bg-slate-800/30">
              <td class="p-3.5 text-emerald-400 font-semibold">{{ h.device_id }}</td>
              <td class="p-3.5 text-white">{{ h.name }}</td>
              <td class="p-3.5">
                <span
                  class="px-2 py-0.5 rounded text-[10px] font-bold"
                  :class="h.status === 'online' ? 'bg-emerald-500/10 text-emerald-400' : 'bg-slate-800 text-slate-400'"
                >
                  {{ h.status.toUpperCase() }}
                </span>
              </td>
              <td class="p-3.5 text-slate-200">{{ h.total_telemetry_packets }}</td>
              <td class="p-3.5">
                <span :class="h.active_alerts > 0 ? 'text-rose-400 font-bold' : 'text-slate-400'">
                  {{ h.active_alerts }}
                </span>
              </td>
              <td class="p-3.5 text-cyan-400 font-bold">{{ h.uptime_pct.toFixed(1) }}%</td>
              <td class="p-3.5 text-slate-400">{{ h.firmware_version }} / {{ h.model_version }}</td>
            </tr>
            <tr v-if="healthData.length === 0">
              <td colspan="7" class="p-8 text-center text-slate-500">No device metrics recorded yet.</td>
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

const healthData = ref<any[]>([])

async function fetchHealth() {
  try {
    const res = await apiClient.get('/analytics/device-health')
    healthData.value = res.data
  } catch (err) {
    console.error('Failed to load device health analytics', err)
  }
}

onMounted(() => {
  fetchHealth()
})
</script>
