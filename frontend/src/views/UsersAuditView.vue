<template>
  <div class="space-y-6">
    <div class="flex flex-col sm:flex-row sm:items-center justify-between gap-4">
      <div>
        <h2 class="text-xl font-bold text-white tracking-tight">Access Control & Security Audit Trail</h2>
        <p class="text-xs text-slate-400 font-mono mt-0.5">Role-based access management, operator actions and immutable security audit logs</p>
      </div>

      <button
        v-if="authStore.isAdmin"
        @click="showUserModal = true"
        class="px-3.5 py-2 rounded-lg bg-emerald-500 hover:bg-emerald-600 text-white text-xs font-semibold shadow-md shadow-emerald-500/20 transition-all flex items-center gap-1.5"
      >
        <span>+</span> Add User
      </button>
    </div>

    <!-- Users Table -->
    <div class="rounded-2xl bg-slate-900/60 border border-slate-800/80 overflow-hidden backdrop-blur-sm">
      <div class="p-4 border-b border-slate-800 flex items-center justify-between">
        <h3 class="text-sm font-semibold text-white font-mono flex items-center gap-2">
          <span>👥</span> Authorized Platform Users
        </h3>
        <span class="text-xs font-mono text-slate-400">{{ users.length }} Users Enrolled</span>
      </div>

      <div class="overflow-x-auto">
        <table class="w-full text-left text-xs border-collapse">
          <thead>
            <tr class="border-b border-slate-800 bg-slate-900/80 font-mono text-slate-400">
              <th class="p-3.5">User</th>
              <th class="p-3.5">Role</th>
              <th class="p-3.5">Account Status</th>
              <th class="p-3.5">Enrolled At</th>
            </tr>
          </thead>
          <tbody class="divide-y divide-slate-800/60 font-mono text-[11px]">
            <tr v-for="u in users" :key="u.id" class="hover:bg-slate-800/30">
              <td class="p-3.5">
                <div class="font-bold text-white">{{ u.full_name || 'System Operator' }}</div>
                <div class="text-slate-400 text-[10px]">{{ u.email }}</div>
              </td>
              <td class="p-3.5">
                <span
                  class="px-2 py-0.5 rounded text-[10px] font-bold uppercase"
                  :class="u.role === 'admin' ? 'bg-purple-500/10 text-purple-400 border border-purple-500/30' : 'bg-blue-500/10 text-blue-400 border border-blue-500/30'"
                >
                  {{ u.role }}
                </span>
              </td>
              <td class="p-3.5">
                <span class="text-emerald-400 font-bold">ACTIVE</span>
              </td>
              <td class="p-3.5 text-slate-400">
                {{ new Date(u.created_at).toLocaleDateString() }}
              </td>
            </tr>
          </tbody>
        </table>
      </div>
    </div>

    <!-- Audit Logs Table -->
    <div class="rounded-2xl bg-slate-900/60 border border-slate-800/80 overflow-hidden backdrop-blur-sm">
      <div class="p-4 border-b border-slate-800 flex items-center justify-between">
        <h3 class="text-sm font-semibold text-white font-mono flex items-center gap-2">
          <span>🛡️</span> Security & Command Audit Trail
        </h3>
        <button @click="fetchAuditLogs" class="text-xs font-mono text-emerald-400 hover:underline">Refresh Logs</button>
      </div>

      <div class="overflow-x-auto">
        <table class="w-full text-left text-xs border-collapse">
          <thead>
            <tr class="border-b border-slate-800 bg-slate-900/80 font-mono text-slate-400">
              <th class="p-3">Timestamp</th>
              <th class="p-3">Actor / Email</th>
              <th class="p-3">Action</th>
              <th class="p-3">Target Resource</th>
              <th class="p-3">Details</th>
            </tr>
          </thead>
          <tbody class="divide-y divide-slate-800/60 font-mono text-[11px]">
            <tr v-for="log in auditLogs" :key="log.id" class="hover:bg-slate-800/30">
              <td class="p-3 text-slate-400">{{ new Date(log.created_at).toLocaleString() }}</td>
              <td class="p-3 text-slate-200">{{ log.user_email || 'System / Auto' }}</td>
              <td class="p-3">
                <span class="px-2 py-0.5 rounded bg-slate-800 text-emerald-400 font-bold text-[10px]">
                  {{ log.action }}
                </span>
              </td>
              <td class="p-3 text-slate-300">{{ log.resource_type }} ({{ log.resource_id || '-' }})</td>
              <td class="p-3 text-slate-400 truncate max-w-sm">{{ log.details_json || '-' }}</td>
            </tr>
            <tr v-if="auditLogs.length === 0">
              <td colspan="5" class="p-6 text-center text-slate-500 font-mono">
                No audit entries found.
              </td>
            </tr>
          </tbody>
        </table>
      </div>
    </div>

    <!-- Create User Modal -->
    <div v-if="showUserModal" class="fixed inset-0 bg-black/60 backdrop-blur-sm flex items-center justify-center p-4 z-50">
      <div class="w-full max-w-md bg-slate-900 border border-slate-800 rounded-2xl p-6 shadow-2xl space-y-4">
        <h3 class="text-base font-bold text-white">Enroll New User</h3>
        <form @submit.prevent="submitCreateUser" class="space-y-4">
          <div>
            <label class="block text-xs font-mono text-slate-300 mb-1">Full Name</label>
            <input v-model="newUser.full_name" required class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white focus:outline-none focus:border-emerald-500" placeholder="e.g. John Doe" />
          </div>
          <div>
            <label class="block text-xs font-mono text-slate-300 mb-1">Email</label>
            <input v-model="newUser.email" type="email" required class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white font-mono focus:outline-none focus:border-emerald-500" placeholder="operator@smartgrow.local" />
          </div>
          <div>
            <label class="block text-xs font-mono text-slate-300 mb-1">Password</label>
            <input v-model="newUser.password" type="password" minlength="8" required class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white font-mono focus:outline-none focus:border-emerald-500" placeholder="Minimum 8 characters" />
          </div>
          <div>
            <label class="block text-xs font-mono text-slate-300 mb-1">Role</label>
            <select v-model="newUser.role" class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white font-mono">
              <option value="operator">Operator (Can command actuators & configure)</option>
              <option value="admin">Administrator (Full privileges)</option>
              <option value="viewer">Viewer (Read-only)</option>
            </select>
          </div>
          <div class="flex justify-end gap-2 pt-2">
            <button type="button" @click="showUserModal = false" class="px-4 py-2 text-xs text-slate-400 hover:text-white">Cancel</button>
            <button type="submit" class="px-4 py-2 bg-emerald-500 hover:bg-emerald-600 text-white text-xs font-semibold rounded-lg">Enroll User</button>
          </div>
        </form>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'
import apiClient from '@/api/client'
import { useAuthStore } from '@/stores/authStore'
import type { User, AuditLog } from '@/types'

const authStore = useAuthStore()
const users = ref<User[]>([])
const auditLogs = ref<AuditLog[]>([])
const showUserModal = ref(false)

const newUser = ref({
  full_name: '',
  email: '',
  password: '',
  role: 'operator'
})

async function fetchUsers() {
  if (!authStore.isAdmin) return
  try {
    const res = await apiClient.get<User[]>('/auth/users')
    users.value = res.data
  } catch (err) {
    console.error('Failed to load users', err)
  }
}

async function fetchAuditLogs() {
  if (!authStore.isAdmin) return
  try {
    const res = await apiClient.get<AuditLog[]>('/system/audit-logs?limit=50')
    auditLogs.value = res.data
  } catch (err) {
    console.error('Failed to load audit logs', err)
  }
}

async function submitCreateUser() {
  try {
    await apiClient.post('/auth/register', newUser.value)
    showUserModal.value = false
    newUser.value = { full_name: '', email: '', password: '', role: 'operator' }
    await fetchUsers()
    await fetchAuditLogs()
  } catch (err: any) {
    alert(err.response?.data?.detail || 'Failed to enroll user')
  }
}

onMounted(() => {
  fetchUsers()
  fetchAuditLogs()
})
</script>
