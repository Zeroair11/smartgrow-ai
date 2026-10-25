<template>
  <div class="min-h-screen bg-slate-950 flex items-center justify-center p-6 relative overflow-hidden">
    <!-- Ambient Backdrop Glow -->
    <div class="absolute -top-40 -left-40 w-96 h-96 bg-emerald-500/10 rounded-full blur-3xl pointer-events-none"></div>
    <div class="absolute -bottom-40 -right-40 w-96 h-96 bg-blue-500/10 rounded-full blur-3xl pointer-events-none"></div>

    <div class="w-full max-w-md bg-slate-900/90 border border-slate-800 rounded-2xl shadow-2xl p-8 backdrop-blur-xl relative z-10">
      <!-- Header -->
      <div class="text-center mb-8">
        <div class="w-12 h-12 rounded-xl bg-emerald-500/20 border border-emerald-500/30 flex items-center justify-center text-2xl mx-auto mb-3 shadow-inner">
          🌱
        </div>
        <h2 class="text-2xl font-bold tracking-tight text-white">SmartGrow AI</h2>
        <p class="text-xs text-slate-400 font-mono mt-1">Intelligent Greenhouse Management Platform</p>
      </div>

      <!-- Error Alert -->
      <div v-if="errorMessage" class="mb-6 p-3 rounded-lg bg-rose-500/10 border border-rose-500/30 text-rose-400 text-xs flex items-center gap-2">
        <span>⚠️</span>
        <span>{{ errorMessage }}</span>
      </div>

      <!-- Login Form -->
      <form @submit.prevent="handleLogin" class="space-y-4">
        <div>
          <label class="block text-xs font-medium text-slate-300 uppercase tracking-wider mb-1 font-mono">Email Address</label>
          <input
            v-model="email"
            type="email"
            required
            placeholder="admin@smartgrow.local"
            class="w-full px-3.5 py-2.5 rounded-lg bg-slate-950/80 border border-slate-800 text-slate-100 text-sm focus:outline-none focus:border-emerald-500 focus:ring-1 focus:ring-emerald-500 transition-all font-mono"
          />
        </div>

        <div>
          <label class="block text-xs font-medium text-slate-300 uppercase tracking-wider mb-1 font-mono">Password</label>
          <input
            v-model="password"
            type="password"
            required
            placeholder="••••••••••••"
            class="w-full px-3.5 py-2.5 rounded-lg bg-slate-950/80 border border-slate-800 text-slate-100 text-sm focus:outline-none focus:border-emerald-500 focus:ring-1 focus:ring-emerald-500 transition-all font-mono"
          />
        </div>

        <button
          type="submit"
          :disabled="isLoading"
          class="w-full mt-2 py-2.5 px-4 rounded-lg bg-emerald-500 hover:bg-emerald-600 active:scale-[0.99] text-white text-sm font-semibold shadow-lg shadow-emerald-500/20 flex items-center justify-center gap-2 transition-all disabled:opacity-50 disabled:cursor-not-allowed"
        >
          <span v-if="isLoading" class="w-4 h-4 border-2 border-white/20 border-t-white rounded-full animate-spin"></span>
          <span>{{ isLoading ? 'Authenticating...' : 'Sign In to Console' }}</span>
        </button>
      </form>

      <!-- Quick credentials hint for demo/evaluation -->
      <div class="mt-8 pt-4 border-t border-slate-800/80 text-center">
        <p class="text-[11px] text-slate-400 font-mono">Default Credentials: <code class="text-slate-300 font-bold">admin@smartgrow.local</code> / <code class="text-slate-300 font-bold">Admin@SmartGrow2026!</code></p>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref } from 'vue'
import { useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/authStore'

const router = useRouter()
const authStore = useAuthStore()

const email = ref('admin@smartgrow.local')
const password = ref('Admin@SmartGrow2026!')
const isLoading = ref(false)
const errorMessage = ref('')

async function handleLogin() {
  errorMessage.value = ''
  isLoading.value = true
  try {
    await authStore.login(email.value, password.value)
    router.push({ name: 'dashboard' })
  } catch (err: any) {
    errorMessage.value = err.response?.data?.detail || 'Authentication failed. Please check credentials.'
  } finally {
    isLoading.value = false
  }
}
</script>
