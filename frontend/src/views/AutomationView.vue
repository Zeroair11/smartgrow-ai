<template>
  <div class="space-y-6">
    <div class="flex flex-col sm:flex-row sm:items-center justify-between gap-4">
      <div>
        <h2 class="text-xl font-bold text-white tracking-tight">Automation Rules Engine</h2>
        <p class="text-xs text-slate-400 font-mono mt-0.5">Threshold triggers, cooldown constraints, and automated environmental actuation</p>
      </div>

      <button
        @click="openCreateModal"
        class="px-3.5 py-2 rounded-lg bg-emerald-500 hover:bg-emerald-600 text-white text-xs font-semibold shadow-md shadow-emerald-500/20 transition-all flex items-center gap-1.5"
      >
        <span>+</span> Add Automation Rule
      </button>
    </div>

    <!-- Active Rules Cards -->
    <div v-if="rules.length > 0" class="grid grid-cols-1 md:grid-cols-2 gap-4">
      <div
        v-for="rule in rules"
        :key="rule.id"
        class="p-5 rounded-2xl bg-slate-900/60 border border-slate-800/80 backdrop-blur-sm space-y-3"
      >
        <div class="flex items-start justify-between">
          <div>
            <h3 class="text-sm font-bold text-white flex items-center gap-2">
              <span>⚡</span>
              <span>{{ rule.name }}</span>
            </h3>
            <p class="text-[11px] text-slate-400 font-mono mt-0.5">
              Triggered: {{ rule.execution_count }} times • Cooldown: {{ rule.cooldown_s }}s
            </p>
          </div>
          <button
            @click="toggleRule(rule.id)"
            class="px-2.5 py-1 rounded-full text-[10px] font-mono font-bold border transition-all"
            :class="rule.is_active ? 'bg-emerald-500/10 text-emerald-400 border-emerald-500/30' : 'bg-slate-800 text-slate-500 border-slate-700'"
          >
            {{ rule.is_active ? 'ENABLED' : 'PAUSED' }}
          </button>
        </div>

        <!-- Logic Statement Pill -->
        <div class="p-3 bg-slate-950/80 rounded-xl border border-slate-800 font-mono text-xs flex items-center justify-between">
          <span class="text-cyan-400">
            IF {{ rule.sensor_metric }} {{ rule.operator }} {{ rule.threshold_val }}
          </span>
          <span class="text-slate-400">&rarr;</span>
          <span class="text-emerald-400 font-bold">
            {{ rule.action_type }} ({{ rule.action_param_val }})
          </span>
        </div>

        <div class="flex items-center justify-between text-xs font-mono text-slate-400 pt-1">
          <span>Last trigger: {{ rule.last_triggered_at ? new Date(rule.last_triggered_at).toLocaleTimeString() : 'Never' }}</span>
          <button @click="deleteRule(rule.id)" class="text-slate-500 hover:text-rose-400">Delete</button>
        </div>
      </div>
    </div>

    <!-- Empty State -->
    <div v-else class="text-center py-16 bg-slate-900/40 rounded-2xl border border-dashed border-slate-800">
      <span class="text-4xl">⚡</span>
      <h3 class="text-sm font-semibold text-white mt-3 font-mono">No Automation Rules Configured</h3>
      <p class="text-xs text-slate-400 max-w-sm mx-auto mt-1">Define autonomous logic to automatically trigger irrigation, open ventilation, or raise alerts.</p>
      <button
        @click="openCreateModal"
        class="mt-4 px-4 py-2 rounded-lg bg-emerald-500 hover:bg-emerald-600 text-white text-xs font-semibold transition-all"
      >
        Create First Rule
      </button>
    </div>

    <!-- Rule Trigger Events Log -->
    <div class="rounded-2xl bg-slate-900/60 border border-slate-800/80 overflow-hidden backdrop-blur-sm">
      <div class="p-4 border-b border-slate-800 flex items-center justify-between">
        <h3 class="text-sm font-semibold text-white font-mono flex items-center gap-2">
          <span>📜</span> Automation Trigger Activity Log
        </h3>
        <button @click="fetchRuleEvents" class="text-xs font-mono text-emerald-400 hover:underline">Refresh Logs</button>
      </div>

      <div class="overflow-x-auto">
        <table class="w-full text-left text-xs border-collapse">
          <thead>
            <tr class="border-b border-slate-800 bg-slate-900/80 font-mono text-slate-400">
              <th class="p-3">Time</th>
              <th class="p-3">Rule ID</th>
              <th class="p-3">Device</th>
              <th class="p-3">Sensor Value</th>
              <th class="p-3">Result</th>
              <th class="p-3">Details</th>
            </tr>
          </thead>
          <tbody class="divide-y divide-slate-800/60 font-mono text-[11px]">
            <tr v-for="ev in ruleEvents" :key="ev.id" class="hover:bg-slate-800/30">
              <td class="p-3 text-slate-400">{{ new Date(ev.triggered_at).toLocaleTimeString() }}</td>
              <td class="p-3 text-slate-300">#RULE-{{ ev.rule_id }}</td>
              <td class="p-3 text-emerald-400">{{ ev.device_id }}</td>
              <td class="p-3 text-cyan-400 font-bold">{{ ev.metric_val.toFixed(1) }}</td>
              <td class="p-3">
                <span class="px-2 py-0.5 rounded text-[10px] font-bold" :class="ev.result_status === 'executed' ? 'bg-emerald-500/10 text-emerald-400' : 'bg-rose-500/10 text-rose-400'">
                  {{ ev.result_status }}
                </span>
              </td>
              <td class="p-3 text-slate-300 truncate max-w-xs">{{ ev.message }}</td>
            </tr>
            <tr v-if="ruleEvents.length === 0">
              <td colspan="6" class="p-6 text-center text-slate-500 font-mono">
                No automation rules triggered yet.
              </td>
            </tr>
          </tbody>
        </table>
      </div>
    </div>

    <!-- Create Rule Modal -->
    <div v-if="showModal" class="fixed inset-0 bg-black/60 backdrop-blur-sm flex items-center justify-center p-4 z-50">
      <div class="w-full max-w-lg bg-slate-900 border border-slate-800 rounded-2xl p-6 shadow-2xl space-y-4">
        <h3 class="text-base font-bold text-white">Create Automation Rule</h3>
        <form @submit.prevent="submitCreateRule" class="space-y-4">
          <div>
            <label class="block text-xs font-mono text-slate-300 mb-1">Rule Name</label>
            <input v-model="newRule.name" required class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white focus:outline-none focus:border-emerald-500" placeholder="e.g. Dry Soil Auto-Water" />
          </div>

          <div class="grid grid-cols-3 gap-3">
            <div>
              <label class="block text-xs font-mono text-slate-300 mb-1">Sensor Metric</label>
              <select v-model="newRule.sensor_metric" class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-xs text-white font-mono">
                <option value="soil_moisture_pct">Soil Moisture (%)</option>
                <option value="temperature_c">Temperature (°C)</option>
                <option value="humidity_pct">Air Humidity (%)</option>
                <option value="light_level_pct">Light Level (%)</option>
              </select>
            </div>
            <div>
              <label class="block text-xs font-mono text-slate-300 mb-1">Condition</label>
              <select v-model="newRule.operator" class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-xs text-white font-mono">
                <option value="<">&lt; (Less Than)</option>
                <option value="<=">&lt;= (Less or Eq)</option>
                <option value=">">&gt; (Greater Than)</option>
                <option value=">=">&gt;= (Greater or Eq)</option>
              </select>
            </div>
            <div>
              <label class="block text-xs font-mono text-slate-300 mb-1">Threshold</label>
              <input v-model.number="newRule.threshold_val" type="number" step="0.5" required class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-xs text-white font-mono" />
            </div>
          </div>

          <div class="grid grid-cols-2 gap-3">
            <div>
              <label class="block text-xs font-mono text-slate-300 mb-1">Action</label>
              <select v-model="newRule.action_type" class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-xs text-white font-mono">
                <option value="PUMP_START">Start Pump</option>
                <option value="VENT_SET">Set Ventilation Servo</option>
                <option value="ALERT">Raise Incident Alert</option>
              </select>
            </div>
            <div>
              <label class="block text-xs font-mono text-slate-300 mb-1">Parameter (Duration / Angle)</label>
              <input v-model.number="newRule.action_param_val" type="number" min="5" max="120" required class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-xs text-white font-mono" />
            </div>
          </div>

          <div>
            <label class="block text-xs font-mono text-slate-300 mb-1">Cooldown Timer (seconds)</label>
            <input v-model.number="newRule.cooldown_s" type="number" min="30" max="3600" required class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-xs text-white font-mono" />
          </div>

          <div class="flex justify-end gap-2 pt-2">
            <button type="button" @click="showModal = false" class="px-4 py-2 text-xs text-slate-400 hover:text-white">Cancel</button>
            <button type="submit" class="px-4 py-2 bg-emerald-500 hover:bg-emerald-600 text-white text-xs font-semibold rounded-lg">Save Rule</button>
          </div>
        </form>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'
import apiClient from '@/api/client'
import type { AutomationRule, AutomationRuleEvent } from '@/types'

const rules = ref<AutomationRule[]>([])
const ruleEvents = ref<AutomationRuleEvent[]>([])
const showModal = ref(false)

const newRule = ref({
  name: '',
  sensor_metric: 'soil_moisture_pct',
  operator: '<',
  threshold_val: 30.0,
  action_type: 'PUMP_START',
  action_param_val: 30.0,
  cooldown_s: 300
})

async function fetchRules() {
  try {
    const res = await apiClient.get<AutomationRule[]>('/automation/rules')
    rules.value = res.data
  } catch (err) {
    console.error('Failed to load rules', err)
  }
}

async function fetchRuleEvents() {
  try {
    const res = await apiClient.get<AutomationRuleEvent[]>('/automation/events')
    ruleEvents.value = res.data
  } catch (err) {
    console.error('Failed to load rule events', err)
  }
}

function openCreateModal() {
  newRule.value = {
    name: '',
    sensor_metric: 'soil_moisture_pct',
    operator: '<',
    threshold_val: 30.0,
    action_type: 'PUMP_START',
    action_param_val: 30.0,
    cooldown_s: 300
  }
  showModal.value = true
}

async function submitCreateRule() {
  try {
    await apiClient.post('/automation/rules', newRule.value)
    showModal.value = false
    await fetchRules()
  } catch (err: any) {
    alert(err.response?.data?.detail || 'Failed to create automation rule')
  }
}

async function toggleRule(id: number) {
  try {
    await apiClient.post(`/automation/rules/${id}/toggle`)
    await fetchRules()
  } catch (err: any) {
    alert(err.response?.data?.detail || 'Failed to toggle rule')
  }
}

async function deleteRule(id: number) {
  if (!confirm('Are you sure you want to delete this automation rule?')) return
  try {
    await apiClient.delete(`/automation/rules/${id}`)
    await fetchRules()
  } catch (err: any) {
    alert(err.response?.data?.detail || 'Failed to delete rule')
  }
}

onMounted(() => {
  fetchRules()
  fetchRuleEvents()
})
</script>
