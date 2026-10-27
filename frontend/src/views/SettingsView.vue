<template>
  <div class="space-y-6">
    <div class="flex flex-col sm:flex-row sm:items-center justify-between gap-4">
      <div>
        <h2 class="text-xl font-bold text-white tracking-tight">System Settings & Calibration</h2>
        <p class="text-xs text-slate-400 font-mono mt-0.5">Platform parameters, safety guardrails and OTA firmware distributions</p>
      </div>
    </div>

    <!-- System Configuration Cards -->
    <div class="grid grid-cols-1 md:grid-cols-2 gap-6">
      <!-- Card 1: Safety & Units -->
      <div class="p-5 rounded-2xl bg-slate-900/60 border border-slate-800/80 backdrop-blur-sm space-y-4">
        <h3 class="text-sm font-bold text-white font-mono flex items-center gap-2">
          <span>⚙️</span> Environmental & Safety Defaults
        </h3>

        <div class="space-y-3 text-xs font-mono">
          <div class="flex justify-between p-2.5 rounded-lg bg-slate-950/80 border border-slate-800">
            <span class="text-slate-400">Timezone</span>
            <span class="text-white">UTC+07:00 (Asia/Ho_Chi_Minh)</span>
          </div>
          <div class="flex justify-between p-2.5 rounded-lg bg-slate-950/80 border border-slate-800">
            <span class="text-slate-400">Max Pump Duration</span>
            <span class="text-emerald-400 font-bold">120 seconds (Hard Interlock)</span>
          </div>
          <div class="flex justify-between p-2.5 rounded-lg bg-slate-950/80 border border-slate-800">
            <span class="text-slate-400">Min Pump Cooldown</span>
            <span class="text-amber-400 font-bold">180 seconds (Mandatory Rest)</span>
          </div>
          <div class="flex justify-between p-2.5 rounded-lg bg-slate-950/80 border border-slate-800">
            <span class="text-slate-400">Offline Fail-Safe</span>
            <span class="text-cyan-400 font-bold">300 seconds</span>
          </div>
        </div>
      </div>

      <!-- Card 2: Soil Calibration Guide -->
      <div class="p-5 rounded-2xl bg-slate-900/60 border border-slate-800/80 backdrop-blur-sm space-y-3">
        <h3 class="text-sm font-bold text-white font-mono flex items-center gap-2">
          <span>🧪</span> Soil Moisture Probe Calibration
        </h3>
        <p class="text-xs text-slate-400">
          Analog capacitive soil moisture probes measure dielectric permittivity. High voltage corresponds to dry air; low voltage corresponds to submerged water.
        </p>

        <div class="p-3 rounded-xl bg-slate-950/80 border border-slate-800 text-xs font-mono space-y-1">
          <div class="flex justify-between text-slate-300">
            <span>Air Baseline (0% moisture):</span>
            <span class="text-amber-400 font-bold">~3200 ADC (12-bit)</span>
          </div>
          <div class="flex justify-between text-slate-300">
            <span>Water Baseline (100% moisture):</span>
            <span class="text-cyan-400 font-bold">~1200 ADC (12-bit)</span>
          </div>
        </div>
        <p class="text-[11px] text-slate-500 font-mono">
          Per-controller dry/wet points can be tuned in the <router-link to="/devices" class="text-emerald-400 underline">Devices tab</router-link>.
        </p>
      </div>
    </div>

    <!-- OTA Firmware Distribution Center -->
    <div class="p-6 rounded-2xl bg-slate-900/60 border border-slate-800/80 backdrop-blur-sm space-y-4">
      <div class="flex items-center justify-between">
        <div>
          <h3 class="text-base font-bold text-white flex items-center gap-2">
            <span>🚀</span> OTA Firmware Releases
          </h3>
          <p class="text-xs text-slate-400 font-mono mt-0.5">Manage certified binary releases and distribute updates over Wi-Fi</p>
        </div>
      </div>

      <div class="overflow-x-auto">
        <table class="w-full text-left text-xs border-collapse">
          <thead>
            <tr class="border-b border-slate-800 bg-slate-900 font-mono text-slate-400">
              <th class="p-3">Version</th>
              <th class="p-3">SHA256 Checksum</th>
              <th class="p-3">Binary Size</th>
              <th class="p-3">Release Notes</th>
              <th class="p-3">Date</th>
            </tr>
          </thead>
          <tbody class="divide-y divide-slate-800/60 font-mono text-[11px]">
            <tr v-for="rel in releases" :key="rel.id" class="hover:bg-slate-800/30">
              <td class="p-3 text-emerald-400 font-bold">v{{ rel.version }}</td>
              <td class="p-3 text-slate-400 font-mono text-[10px] truncate max-w-xs">{{ rel.checksum_sha256 }}</td>
              <td class="p-3 text-slate-300">{{ (rel.file_size_bytes / 1024).toFixed(1) }} KB</td>
              <td class="p-3 text-slate-300">{{ rel.release_notes || 'Stable build' }}</td>
              <td class="p-3 text-slate-400">{{ formatDate(rel.created_at) }}</td>
            </tr>
            <tr v-if="releases.length === 0">
              <td colspan="5" class="p-6 text-center text-slate-500 font-mono">
                No firmware releases registered. Builds can be pushed via CI/CD.
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
import type { FirmwareRelease } from '@/types'
import { formatDate } from '@/utils/date'

const releases = ref<FirmwareRelease[]>([])

async function fetchReleases() {
  try {
    const res = await apiClient.get<FirmwareRelease[]>('/ota/releases')
    releases.value = res.data
  } catch (err) {
    console.error('Failed to load firmware releases', err)
  }
}

onMounted(() => {
  fetchReleases()
})
</script>
