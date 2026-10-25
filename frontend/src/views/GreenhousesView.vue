<template>
  <div class="space-y-6">
    <div class="flex flex-col sm:flex-row sm:items-center justify-between gap-4">
      <div>
        <h2 class="text-xl font-bold text-white tracking-tight">Greenhouses & Zones</h2>
        <p class="text-xs text-slate-400 font-mono mt-0.5">Manage facilities, cultivation zones, crop profiles and environmental thresholds</p>
      </div>
      <div class="flex items-center gap-2">
        <button
          @click="openCreateGreenhouseModal"
          class="px-3.5 py-2 rounded-lg bg-emerald-500 hover:bg-emerald-600 text-white text-xs font-semibold shadow-md shadow-emerald-500/20 transition-all flex items-center gap-1.5"
        >
          <span>+</span> Add Greenhouse
        </button>
        <button
          @click="openCreateZoneModal"
          class="px-3.5 py-2 rounded-lg bg-slate-800 hover:bg-slate-700 text-slate-200 text-xs font-semibold border border-slate-700 transition-all flex items-center gap-1.5"
        >
          <span>+</span> Add Zone
        </button>
      </div>
    </div>

    <!-- Greenhouses Grid -->
    <div v-if="greenhouses.length > 0" class="grid grid-cols-1 md:grid-cols-2 gap-6">
      <div
        v-for="gh in greenhouses"
        :key="gh.id"
        class="p-5 rounded-2xl bg-slate-900/60 border border-slate-800/80 backdrop-blur-sm space-y-4"
      >
        <div class="flex items-start justify-between">
          <div>
            <div class="flex items-center gap-2">
              <span class="text-lg">🏡</span>
              <h3 class="text-base font-bold text-white">{{ gh.name }}</h3>
              <span class="px-2 py-0.5 text-[10px] font-mono rounded bg-slate-800 text-slate-300 border border-slate-700">{{ gh.code }}</span>
            </div>
            <p class="text-xs text-slate-400 mt-1">{{ gh.location || 'Location unassigned' }}</p>
          </div>
          <button
            @click="deleteGreenhouse(gh.id)"
            class="text-xs text-slate-500 hover:text-rose-400 p-1 transition-colors"
            title="Delete Facility"
          >
            ✕
          </button>
        </div>

        <p v-if="gh.description" class="text-xs text-slate-300 bg-slate-950/50 p-2.5 rounded-lg border border-slate-800/80">
          {{ gh.description }}
        </p>

        <!-- Zones in this Greenhouse -->
        <div class="pt-2 border-t border-slate-800/80">
          <p class="text-xs font-mono font-semibold text-slate-400 mb-2">Cultivation Zones:</p>
          <div class="space-y-2">
            <div
              v-for="zone in getZonesForGreenhouse(gh.id)"
              :key="zone.id"
              class="p-3 rounded-xl bg-slate-950/80 border border-slate-800 flex items-center justify-between text-xs"
            >
              <div>
                <p class="font-semibold text-white flex items-center gap-2">
                  <span>🌿 {{ zone.name }}</span>
                  <span class="text-[10px] text-emerald-400 font-mono">({{ zone.crop_type || 'Unspecified Crop' }})</span>
                </p>
                <p class="text-[11px] text-slate-400 font-mono mt-0.5">
                  Moisture: {{ zone.target_soil_moisture_min }}% - {{ zone.target_soil_moisture_max }}% • Temp: {{ zone.target_temp_min }}°C - {{ zone.target_temp_max }}°C
                </p>
              </div>
              <button
                @click="deleteZone(zone.id)"
                class="text-slate-500 hover:text-rose-400 text-xs px-2 py-1"
                title="Delete Zone"
              >
                ✕
              </button>
            </div>
            <div v-if="getZonesForGreenhouse(gh.id).length === 0" class="text-xs text-slate-500 italic py-2">
              No zones registered in this facility yet.
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- Empty State -->
    <div v-else class="text-center py-16 bg-slate-900/40 rounded-2xl border border-dashed border-slate-800">
      <span class="text-4xl">🏡</span>
      <h3 class="text-sm font-semibold text-white mt-3 font-mono">No Greenhouses Configured</h3>
      <p class="text-xs text-slate-400 max-w-sm mx-auto mt-1">Get started by creating your first greenhouse facility and defining cultivation zones.</p>
      <button
        @click="openCreateGreenhouseModal"
        class="mt-4 px-4 py-2 rounded-lg bg-emerald-500 hover:bg-emerald-600 text-white text-xs font-semibold transition-all"
      >
        Create Facility
      </button>
    </div>

    <!-- Create Greenhouse Modal -->
    <div v-if="showGhModal" class="fixed inset-0 bg-black/60 backdrop-blur-sm flex items-center justify-center p-4 z-50">
      <div class="w-full max-w-md bg-slate-900 border border-slate-800 rounded-2xl p-6 shadow-2xl">
        <h3 class="text-base font-bold text-white mb-4">Add Greenhouse Facility</h3>
        <form @submit.prevent="submitCreateGreenhouse" class="space-y-4">
          <div>
            <label class="block text-xs font-mono text-slate-300 mb-1">Facility Name</label>
            <input v-model="newGh.name" required class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white focus:outline-none focus:border-emerald-500" placeholder="e.g. Greenhouse Alpha" />
          </div>
          <div>
            <label class="block text-xs font-mono text-slate-300 mb-1">Code</label>
            <input v-model="newGh.code" required class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white focus:outline-none focus:border-emerald-500 font-mono" placeholder="GH-01" />
          </div>
          <div>
            <label class="block text-xs font-mono text-slate-300 mb-1">Location</label>
            <input v-model="newGh.location" class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white focus:outline-none focus:border-emerald-500" placeholder="e.g. Da Lat, Sector 4" />
          </div>
          <div>
            <label class="block text-xs font-mono text-slate-300 mb-1">Description</label>
            <textarea v-model="newGh.description" rows="2" class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white focus:outline-none focus:border-emerald-500" placeholder="Hydroponic and substrate testbed"></textarea>
          </div>
          <div class="flex justify-end gap-2 pt-2">
            <button type="button" @click="showGhModal = false" class="px-4 py-2 text-xs text-slate-400 hover:text-white">Cancel</button>
            <button type="submit" class="px-4 py-2 bg-emerald-500 hover:bg-emerald-600 text-white text-xs font-semibold rounded-lg">Save Facility</button>
          </div>
        </form>
      </div>
    </div>

    <!-- Create Zone Modal -->
    <div v-if="showZoneModal" class="fixed inset-0 bg-black/60 backdrop-blur-sm flex items-center justify-center p-4 z-50">
      <div class="w-full max-w-md bg-slate-900 border border-slate-800 rounded-2xl p-6 shadow-2xl">
        <h3 class="text-base font-bold text-white mb-4">Add Cultivation Zone</h3>
        <form @submit.prevent="submitCreateZone" class="space-y-4">
          <div>
            <label class="block text-xs font-mono text-slate-300 mb-1">Parent Facility</label>
            <select v-model="newZone.greenhouse_id" required class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white focus:outline-none focus:border-emerald-500">
              <option v-for="gh in greenhouses" :key="gh.id" :value="gh.id">{{ gh.name }} ({{ gh.code }})</option>
            </select>
          </div>
          <div>
            <label class="block text-xs font-mono text-slate-300 mb-1">Zone Name</label>
            <input v-model="newZone.name" required class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white focus:outline-none focus:border-emerald-500" placeholder="e.g. Zone A - Tomatoes" />
          </div>
          <div>
            <label class="block text-xs font-mono text-slate-300 mb-1">Zone Code</label>
            <input v-model="newZone.code" required class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white focus:outline-none focus:border-emerald-500 font-mono" placeholder="Z-01" />
          </div>
          <div>
            <label class="block text-xs font-mono text-slate-300 mb-1">Crop Type</label>
            <input v-model="newZone.crop_type" class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white focus:outline-none focus:border-emerald-500" placeholder="e.g. Cherry Tomato / Lettuce" />
          </div>
          <div class="grid grid-cols-2 gap-3">
            <div>
              <label class="block text-xs font-mono text-slate-300 mb-1">Min Moisture (%)</label>
              <input v-model.number="newZone.target_soil_moisture_min" type="number" class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white font-mono" />
            </div>
            <div>
              <label class="block text-xs font-mono text-slate-300 mb-1">Max Moisture (%)</label>
              <input v-model.number="newZone.target_soil_moisture_max" type="number" class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white font-mono" />
            </div>
          </div>
          <div class="flex justify-end gap-2 pt-2">
            <button type="button" @click="showZoneModal = false" class="px-4 py-2 text-xs text-slate-400 hover:text-white">Cancel</button>
            <button type="submit" class="px-4 py-2 bg-emerald-500 hover:bg-emerald-600 text-white text-xs font-semibold rounded-lg">Save Zone</button>
          </div>
        </form>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'
import apiClient from '@/api/client'
import type { Greenhouse, Zone } from '@/types'

const greenhouses = ref<Greenhouse[]>([])
const zones = ref<Zone[]>([])

const showGhModal = ref(false)
const showZoneModal = ref(false)

const newGh = ref({
  name: '',
  code: '',
  description: '',
  location: ''
})

const newZone = ref({
  greenhouse_id: 1,
  name: '',
  code: '',
  crop_type: '',
  target_soil_moisture_min: 40.0,
  target_soil_moisture_max: 70.0,
  target_temp_min: 18.0,
  target_temp_max: 32.0
})

async function fetchGreenhousesAndZones() {
  try {
    const [ghRes, zoneRes] = await Promise.all([
      apiClient.get<Greenhouse[]>('/greenhouses'),
      apiClient.get<Zone[]>('/zones')
    ])
    greenhouses.value = ghRes.data
    zones.value = zoneRes.data
    if (greenhouses.value.length > 0 && !newZone.value.greenhouse_id) {
      newZone.value.greenhouse_id = greenhouses.value[0].id
    }
  } catch (err) {
    console.error('Failed to load greenhouses/zones', err)
  }
}

function getZonesForGreenhouse(ghId: number): Zone[] {
  return zones.value.filter(z => z.greenhouse_id === ghId)
}

function openCreateGreenhouseModal() {
  newGh.value = { name: '', code: `GH-0${greenhouses.value.length + 1}`, description: '', location: '' }
  showGhModal.value = true
}

function openCreateZoneModal() {
  if (greenhouses.value.length === 0) {
    alert('Please create a greenhouse facility first!')
    return
  }
  newZone.value = {
    greenhouse_id: greenhouses.value[0].id,
    name: '',
    code: `Z-0${zones.value.length + 1}`,
    crop_type: '',
    target_soil_moisture_min: 40.0,
    target_soil_moisture_max: 70.0,
    target_temp_min: 18.0,
    target_temp_max: 32.0
  }
  showZoneModal.value = true
}

async function submitCreateGreenhouse() {
  try {
    await apiClient.post('/greenhouses', newGh.value)
    showGhModal.value = false
    await fetchGreenhousesAndZones()
  } catch (err: any) {
    alert(err.response?.data?.detail || 'Failed to create greenhouse')
  }
}

async function submitCreateZone() {
  try {
    await apiClient.post('/zones', newZone.value)
    showZoneModal.value = false
    await fetchGreenhousesAndZones()
  } catch (err: any) {
    alert(err.response?.data?.detail || 'Failed to create zone')
  }
}

async function deleteGreenhouse(id: number) {
  if (!confirm('Are you sure you want to delete this facility and all its zones?')) return
  try {
    await apiClient.delete(`/greenhouses/${id}`)
    await fetchGreenhousesAndZones()
  } catch (err: any) {
    alert(err.response?.data?.detail || 'Failed to delete greenhouse')
  }
}

async function deleteZone(id: number) {
  if (!confirm('Are you sure you want to delete this zone?')) return
  try {
    await apiClient.delete(`/zones/${id}`)
    await fetchGreenhousesAndZones()
  } catch (err: any) {
    alert(err.response?.data?.detail || 'Failed to delete zone')
  }
}

onMounted(() => {
  fetchGreenhousesAndZones()
})
</script>
