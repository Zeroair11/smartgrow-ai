<template>
  <div class="space-y-6">
    <div class="flex flex-col sm:flex-row sm:items-center justify-between gap-4">
      <div>
        <h2 class="text-xl font-bold text-white tracking-tight">Edge AI & TinyML Center</h2>
        <p class="text-xs text-slate-400 font-mono mt-0.5">ESP32-S3 on-chip inference, deterministic execution, and safety guardrails</p>
      </div>

      <div class="flex items-center gap-2">
        <span class="px-2.5 py-1 rounded bg-emerald-500/10 border border-emerald-500/30 text-emerald-400 font-mono text-xs font-semibold">
          Model: v1.0.0-dt (Active)
        </span>
      </div>
    </div>

    <!-- Edge AI Performance & Architecture Stats Grid -->
    <div class="grid grid-cols-2 md:grid-cols-4 gap-4">
      <div class="p-4 rounded-xl bg-slate-900/60 border border-slate-800/80">
        <p class="text-xs font-mono text-slate-400">Inference Latency</p>
        <p class="text-2xl font-bold font-mono text-emerald-400 mt-2">14 µs</p>
        <p class="text-[11px] text-slate-400 font-mono mt-1">Direct C execution on Xtensa LX7</p>
      </div>

      <div class="p-4 rounded-xl bg-slate-900/60 border border-slate-800/80">
        <p class="text-xs font-mono text-slate-400">RAM Allocation</p>
        <p class="text-2xl font-bold font-mono text-cyan-400 mt-2">128 Bytes</p>
        <p class="text-[11px] text-slate-400 font-mono mt-1">Zero heap / pure stack usage</p>
      </div>

      <div class="p-4 rounded-xl bg-slate-900/60 border border-slate-800/80">
        <p class="text-xs font-mono text-slate-400">Test Accuracy</p>
        <p class="text-2xl font-bold font-mono text-amber-400 mt-2">98.5%</p>
        <p class="text-[11px] text-slate-400 font-mono mt-1">Macro F1: 98.2% vs Baseline</p>
      </div>

      <div class="p-4 rounded-xl bg-slate-900/60 border border-slate-800/80">
        <p class="text-xs font-mono text-slate-400">Safety Interlocks</p>
        <p class="text-2xl font-bold font-mono text-purple-400 mt-2">ARMED</p>
        <p class="text-[11px] text-slate-400 font-mono mt-1">Hardware priority override</p>
      </div>
    </div>

    <!-- Feature Importances & Model Characteristics -->
    <div class="grid grid-cols-1 md:grid-cols-3 gap-6">
      <div class="p-5 rounded-2xl bg-slate-900/60 border border-slate-800/80 md:col-span-1 space-y-3">
        <h3 class="text-sm font-semibold text-white font-mono">Input Feature Weights</h3>
        <p class="text-xs text-slate-400">Gini importance computed during decision tree optimization:</p>
        <div class="space-y-3 pt-2">
          <div>
            <div class="flex justify-between text-xs font-mono mb-1">
              <span class="text-slate-300">Soil Moisture (%)</span>
              <span class="text-emerald-400 font-bold">52%</span>
            </div>
            <div class="w-full bg-slate-800 rounded-full h-1.5 overflow-hidden">
              <div class="bg-emerald-500 h-1.5 rounded-full" style="width: 52%"></div>
            </div>
          </div>
          <div>
            <div class="flex justify-between text-xs font-mono mb-1">
              <span class="text-slate-300">Air Temperature (°C)</span>
              <span class="text-amber-400 font-bold">28%</span>
            </div>
            <div class="w-full bg-slate-800 rounded-full h-1.5 overflow-hidden">
              <div class="bg-amber-500 h-1.5 rounded-full" style="width: 28%"></div>
            </div>
          </div>
          <div>
            <div class="flex justify-between text-xs font-mono mb-1">
              <span class="text-slate-300">Air Humidity (%)</span>
              <span class="text-indigo-400 font-bold">12%</span>
            </div>
            <div class="w-full bg-slate-800 rounded-full h-1.5 overflow-hidden">
              <div class="bg-indigo-500 h-1.5 rounded-full" style="width: 12%"></div>
            </div>
          </div>
          <div>
            <div class="flex justify-between text-xs font-mono mb-1">
              <span class="text-slate-300">Ambient Light (%)</span>
              <span class="text-yellow-400 font-bold">8%</span>
            </div>
            <div class="w-full bg-slate-800 rounded-full h-1.5 overflow-hidden">
              <div class="bg-yellow-500 h-1.5 rounded-full" style="width: 8%"></div>
            </div>
          </div>
        </div>
      </div>

      <!-- Live Inference Log Table -->
      <div class="p-5 rounded-2xl bg-slate-900/60 border border-slate-800/80 md:col-span-2 space-y-4">
        <div class="flex items-center justify-between">
          <h3 class="text-sm font-semibold text-white font-mono flex items-center gap-2">
            <span>🧠</span> On-Chip Inference Events
          </h3>
          <button @click="fetchInferences" class="text-xs font-mono text-emerald-400 hover:underline">Refresh</button>
        </div>

        <div class="overflow-x-auto">
          <table class="w-full text-left text-xs border-collapse">
            <thead>
              <tr class="border-b border-slate-800 font-mono text-slate-400">
                <th class="p-2.5">Time</th>
                <th class="p-2.5">Device</th>
                <th class="p-2.5">Soil / Temp</th>
                <th class="p-2.5">Classification</th>
                <th class="p-2.5">Conf</th>
                <th class="p-2.5">Action Taken</th>
              </tr>
            </thead>
            <tbody class="divide-y divide-slate-800/60 font-mono text-[11px]">
              <tr v-for="ev in inferences" :key="ev.id" class="hover:bg-slate-800/30">
                <td class="p-2.5 text-slate-400">{{ formatDateTime(ev.recorded_at) }}</td>
                <td class="p-2.5 text-slate-300">{{ ev.device_id }}</td>
                <td class="p-2.5">
                  <span :class="ev.soil_moisture_pct != null ? 'text-cyan-400' : 'text-red-400'">{{ ev.soil_moisture_pct != null ? `${ev.soil_moisture_pct.toFixed(1)}%` : 'FAULT' }}</span> /
                  <span :class="ev.temperature_c != null ? 'text-amber-400' : 'text-red-400'">{{ ev.temperature_c != null ? `${ev.temperature_c.toFixed(1)}°C` : 'FAULT' }}</span>
                </td>
                <td class="p-2.5">
                  <span
                    class="px-2 py-0.5 rounded text-[10px] font-bold"
                    :class="{
                      'bg-emerald-500/10 text-emerald-400': ev.predicted_label === 'NORMAL',
                      'bg-cyan-500/10 text-cyan-400': ev.predicted_label === 'NEED_WATER',
                      'bg-amber-500/10 text-amber-400': ev.predicted_label === 'HIGH_TEMPERATURE',
                      'bg-purple-500/10 text-purple-400': ev.predicted_label === 'LOW_LIGHT',
                      'bg-rose-500/10 text-rose-400': ev.predicted_label === 'SENSOR_FAULT'
                    }"
                  >
                    {{ ev.predicted_label }}
                  </span>
                </td>
                <td class="p-2.5 text-slate-300">{{ (ev.confidence * 100).toFixed(0) }}%</td>
                <td class="p-2.5 font-bold" :class="ev.safety_override ? 'text-rose-400' : 'text-emerald-400'">
                  {{ ev.final_action }}
                </td>
              </tr>
              <tr v-if="inferences.length === 0">
                <td colspan="6" class="p-6 text-center text-slate-500 font-mono">
                  No edge inference events recorded yet.
                </td>
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
import type { AIInferenceEvent } from '@/types'
import { formatDateTime } from '@/utils/date'

const inferences = ref<AIInferenceEvent[]>([])

async function fetchInferences() {
  try {
    const res = await apiClient.get<AIInferenceEvent[]>('/edge-ai/inferences?limit=50')
    inferences.value = res.data
  } catch (err) {
    console.error('Failed to load edge inferences', err)
  }
}

onMounted(() => {
  fetchInferences()
})
</script>
