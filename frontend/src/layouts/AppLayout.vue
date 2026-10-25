<template>
  <div class="min-h-screen bg-slate-950 text-slate-100 flex flex-col md:flex-row antialiased">
    <!-- Sidebar Navigation -->
    <aside class="w-full md:w-64 bg-slate-900/90 border-r border-slate-800/80 flex flex-col shrink-0">
      <!-- Brand Header -->
      <div class="h-16 flex items-center px-6 border-b border-slate-800/80 bg-slate-900/40 gap-3">
        <div class="w-8 h-8 rounded-lg bg-emerald-500/20 border border-emerald-500/40 flex items-center justify-center text-emerald-400 font-bold text-lg shadow-sm">
          🌱
        </div>
        <div>
          <h1 class="text-base font-bold tracking-tight text-white flex items-center gap-1.5">
            SmartGrow <span class="text-xs px-1.5 py-0.5 rounded bg-emerald-500/20 text-emerald-400 font-mono font-semibold border border-emerald-500/30">AI</span>
          </h1>
          <p class="text-xs text-slate-400 font-mono">ESP32-S3 Platform</p>
        </div>
      </div>

      <!-- Navigation Links -->
      <nav class="flex-1 px-3 py-4 space-y-1 overflow-y-auto">
        <div class="px-3 pb-2 text-[10px] font-semibold uppercase tracking-wider text-slate-400 font-mono">
          Core Monitoring
        </div>
        <router-link
          v-for="item in coreNavItems"
          :key="item.to"
          :to="item.to"
          class="flex items-center gap-3 px-3 py-2 rounded-lg text-sm font-medium transition-all"
          :class="$route.path === item.to ? 'bg-emerald-500/15 text-emerald-400 border border-emerald-500/30 font-semibold' : 'text-slate-300 hover:bg-slate-800/60 hover:text-white'"
        >
          <span class="text-base">{{ item.icon }}</span>
          <span>{{ item.label }}</span>
        </router-link>

        <div class="pt-4 px-3 pb-2 text-[10px] font-semibold uppercase tracking-wider text-slate-400 font-mono">
          Intelligence & Control
        </div>
        <router-link
          v-for="item in controlNavItems"
          :key="item.to"
          :to="item.to"
          class="flex items-center gap-3 px-3 py-2 rounded-lg text-sm font-medium transition-all"
          :class="$route.path === item.to ? 'bg-emerald-500/15 text-emerald-400 border border-emerald-500/30 font-semibold' : 'text-slate-300 hover:bg-slate-800/60 hover:text-white'"
        >
          <span class="text-base">{{ item.icon }}</span>
          <span>{{ item.label }}</span>
        </router-link>

        <div class="pt-4 px-3 pb-2 text-[10px] font-semibold uppercase tracking-wider text-slate-400 font-mono">
          Platform & Governance
        </div>
        <router-link
          v-for="item in platformNavItems"
          :key="item.to"
          :to="item.to"
          class="flex items-center gap-3 px-3 py-2 rounded-lg text-sm font-medium transition-all"
          :class="$route.path === item.to ? 'bg-emerald-500/15 text-emerald-400 border border-emerald-500/30 font-semibold' : 'text-slate-300 hover:bg-slate-800/60 hover:text-white'"
        >
          <span class="text-base">{{ item.icon }}</span>
          <span>{{ item.label }}</span>
        </router-link>
      </nav>

      <!-- Connection Status Footer -->
      <div class="p-3 border-t border-slate-800/80 bg-slate-900/40">
        <div class="flex items-center justify-between text-xs font-mono text-slate-400 px-2 py-1">
          <span class="flex items-center gap-1.5">
            <span class="w-2 h-2 rounded-full bg-emerald-400 animate-pulse"></span>
            Backend Active
          </span>
          <span class="text-slate-500 text-[11px]">v1.0.0</span>
        </div>
      </div>
    </aside>

    <!-- Main Content Area -->
    <div class="flex-1 flex flex-col min-w-0">
      <!-- Top Bar -->
      <header class="h-16 bg-slate-900/60 backdrop-blur-md border-b border-slate-800/80 px-6 flex items-center justify-between z-10 shrink-0">
        <div class="flex items-center gap-2">
          <span class="text-xs text-slate-400 font-mono">Platform /</span>
          <span class="text-sm font-semibold text-white">{{ $route.meta.title || 'Management' }}</span>
        </div>

        <div class="flex items-center gap-4">
          <!-- Role Badge -->
          <span class="px-2.5 py-0.5 rounded-full text-xs font-mono font-medium capitalize bg-slate-800 border border-slate-700 text-slate-300">
            {{ authStore.user?.role || 'Guest' }}
          </span>

          <!-- User Details & Logout -->
          <div class="flex items-center gap-3 pl-2 border-l border-slate-800">
            <div class="text-right hidden sm:block">
              <p class="text-xs font-medium text-white">{{ authStore.user?.full_name || 'Administrator' }}</p>
              <p class="text-[11px] font-mono text-slate-400">{{ authStore.user?.email }}</p>
            </div>
            <button
              @click="handleLogout"
              class="px-3 py-1.5 text-xs font-medium text-slate-300 hover:text-white hover:bg-rose-500/20 hover:border-rose-500/40 border border-slate-700 rounded-lg transition-all"
              title="Sign Out"
            >
              Logout
            </button>
          </div>
        </div>
      </header>

      <!-- View Router Container -->
      <main class="flex-1 p-6 overflow-y-auto bg-slate-950/70">
        <router-view />
      </main>
    </div>
  </div>
</template>

<script setup lang="ts">
import { useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/authStore'

const router = useRouter()
const authStore = useAuthStore()

const coreNavItems = [
  { to: '/', label: 'Dashboard', icon: '📊' },
  { to: '/monitoring', label: 'Live Telemetry', icon: '📈' },
  { to: '/greenhouses', label: 'Greenhouses & Zones', icon: '🏡' },
  { to: '/devices', label: 'ESP32-S3 Devices', icon: '📟' },
]

const controlNavItems = [
  { to: '/edge-ai', label: 'Edge AI Center', icon: '🧠' },
  { to: '/irrigation', label: 'Irrigation & Pump', icon: '💧' },
  { to: '/automation', label: 'Automation Rules', icon: '⚡' },
  { to: '/alerts', label: 'Alerts & Incidents', icon: '🔔' },
  { to: '/schedules', label: 'Irrigation Schedule', icon: '📅' },
]

const platformNavItems = [
  { to: '/analytics', label: 'Analytics & Reports', icon: '📑' },
  { to: '/settings', label: 'System Settings', icon: '⚙️' },
  { to: '/users-audit', label: 'Users & Audit Logs', icon: '🛡️' },
]

function handleLogout() {
  authStore.logout()
  router.push({ name: 'login' })
}
</script>
