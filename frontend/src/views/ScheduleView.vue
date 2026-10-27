<template>
  <div class="space-y-6">
    <div class="flex flex-col sm:flex-row sm:items-center justify-between gap-4">
      <div>
        <h2 class="text-xl font-bold text-white tracking-tight">Irrigation & Maintenance Schedules</h2>
        <p class="text-xs text-slate-400 font-mono mt-0.5">Automated cron-based watering programs and periodic maintenance routines</p>
      </div>

      <button
        @click="showModal = true"
        class="px-3.5 py-2 rounded-lg bg-emerald-500 hover:bg-emerald-600 text-white text-xs font-semibold shadow-md shadow-emerald-500/20 transition-all flex items-center gap-1.5"
      >
        <span>+</span> Add Schedule
      </button>
    </div>

    <!-- Schedules Grid -->
    <div v-if="schedules.length > 0" class="grid grid-cols-1 md:grid-cols-3 gap-4">
      <div
        v-for="sch in schedules"
        :key="sch.id"
        class="p-5 rounded-2xl bg-slate-900/60 border border-slate-800/80 backdrop-blur-sm space-y-3"
      >
        <div class="flex items-start justify-between">
          <div>
            <h3 class="text-sm font-bold text-white flex items-center gap-2">
              <span>📅</span>
              <span>{{ sch.name }}</span>
            </h3>
            <p class="text-xs font-mono text-emerald-400 mt-1">Cron: {{ sch.cron_expression }}</p>
          </div>
          <button
            @click="deleteSchedule(sch.id)"
            class="text-xs text-slate-500 hover:text-rose-400"
          >
            ✕
          </button>
        </div>

        <div class="p-3 bg-slate-950/80 rounded-xl border border-slate-800 text-xs font-mono space-y-1">
          <div class="flex justify-between text-slate-400">
            <span>Duration:</span>
            <span class="text-white font-bold">{{ sch.duration_seconds }}s</span>
          </div>
          <div class="flex justify-between text-slate-400">
            <span>Status:</span>
            <span :class="sch.is_active ? 'text-emerald-400' : 'text-slate-500'">{{ sch.is_active ? 'Active' : 'Paused' }}</span>
          </div>
        </div>

        <div class="text-[11px] font-mono text-slate-500">
          Created: {{ formatDate(sch.created_at) }}
        </div>
      </div>
    </div>

    <!-- Empty State -->
    <div v-else class="text-center py-16 bg-slate-900/40 rounded-2xl border border-dashed border-slate-800">
      <span class="text-4xl">📅</span>
      <h3 class="text-sm font-semibold text-white mt-3 font-mono">No Schedules Configured</h3>
      <p class="text-xs text-slate-400 max-w-sm mx-auto mt-1">Create scheduled routines to automate irrigation at recurring times of day.</p>
      <button
        @click="showModal = true"
        class="mt-4 px-4 py-2 rounded-lg bg-emerald-500 hover:bg-emerald-600 text-white text-xs font-semibold transition-all"
      >
        Create Routine
      </button>
    </div>

    <!-- Create Schedule Modal -->
    <div v-if="showModal" class="fixed inset-0 bg-black/60 backdrop-blur-sm flex items-center justify-center p-4 z-50">
      <div class="w-full max-w-md bg-slate-900 border border-slate-800 rounded-2xl p-6 shadow-2xl space-y-4">
        <h3 class="text-base font-bold text-white">Create Irrigation Schedule</h3>
        <form @submit.prevent="submitCreateSchedule" class="space-y-4">
          <div>
            <label class="block text-xs font-mono text-slate-300 mb-1">Schedule Name</label>
            <input v-model="newSchedule.name" required class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white focus:outline-none focus:border-emerald-500" placeholder="e.g. Morning 8AM Refresh" />
          </div>

          <div>
            <label class="block text-xs font-mono text-slate-300 mb-1">Cron Expression</label>
            <input v-model="newSchedule.cron_expression" required class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white font-mono focus:outline-none focus:border-emerald-500" placeholder="0 8 * * *" />
            <p class="text-[10px] text-slate-500 font-mono mt-1">Default: 0 8 * * * (Every morning at 8:00 AM)</p>
          </div>

          <div>
            <label class="block text-xs font-mono text-slate-300 mb-1">Duration (Seconds, max 120)</label>
            <input v-model.number="newSchedule.duration_seconds" type="number" min="5" max="120" required class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white font-mono focus:outline-none focus:border-emerald-500" />
          </div>

          <div class="flex justify-end gap-2 pt-2">
            <button type="button" @click="showModal = false" class="px-4 py-2 text-xs text-slate-400 hover:text-white">Cancel</button>
            <button type="submit" class="px-4 py-2 bg-emerald-500 hover:bg-emerald-600 text-white text-xs font-semibold rounded-lg">Save Schedule</button>
          </div>
        </form>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'
import apiClient from '@/api/client'
import type { IrrigationSchedule } from '@/types'
import { formatDate } from '@/utils/date'

const schedules = ref<IrrigationSchedule[]>([])
const showModal = ref(false)

const newSchedule = ref({
  name: '',
  cron_expression: '0 8 * * *',
  duration_seconds: 45
})

async function fetchSchedules() {
  try {
    const res = await apiClient.get<IrrigationSchedule[]>('/irrigation/schedules')
    schedules.value = res.data
  } catch (err) {
    console.error('Failed to load schedules', err)
  }
}

async function submitCreateSchedule() {
  try {
    await apiClient.post('/irrigation/schedules', newSchedule.value)
    showModal.value = false
    await fetchSchedules()
  } catch (err: any) {
    alert(err.response?.data?.detail || 'Failed to create schedule')
  }
}

async function deleteSchedule(id: number) {
  if (!confirm('Delete this irrigation schedule?')) return
  try {
    await apiClient.delete(`/irrigation/schedules/${id}`)
    await fetchSchedules()
  } catch (err: any) {
    alert(err.response?.data?.detail || 'Failed to delete schedule')
  }
}

onMounted(() => {
  fetchSchedules()
})
</script>
