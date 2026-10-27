<template>
  <div class="space-y-6">
    <div class="flex flex-col sm:flex-row sm:items-center justify-between gap-4">
      <div>
        <h2 class="text-xl font-bold text-white tracking-tight">Access Control & Security Audit Trail</h2>
        <p class="text-xs text-slate-400 font-mono mt-0.5">Role-based access management, operator actions and immutable security audit logs</p>
      </div>

      <button
        v-if="authStore.isAdmin"
        @click="openCreateModal"
        class="px-3.5 py-2 rounded-lg bg-emerald-500 hover:bg-emerald-600 text-white text-xs font-semibold shadow-md shadow-emerald-500/20 transition-all flex items-center gap-1.5"
      >
        <span>+</span> Add User
      </button>
    </div>

    <!-- Alert / Notification -->
    <div v-if="notification" class="p-3 rounded-xl border text-xs font-mono flex items-center justify-between"
         :class="notification.type === 'success' ? 'bg-emerald-500/10 border-emerald-500/30 text-emerald-300' : 'bg-rose-500/10 border-rose-500/30 text-rose-300'">
      <span>{{ notification.message }}</span>
      <button @click="notification = null" class="text-slate-400 hover:text-white ml-2">✕</button>
    </div>

    <!-- Users Table & Filters -->
    <div class="rounded-2xl bg-slate-900/60 border border-slate-800/80 overflow-hidden backdrop-blur-sm">
      <div class="p-4 border-b border-slate-800 flex flex-col md:flex-row md:items-center justify-between gap-3">
        <div class="flex items-center gap-2">
          <h3 class="text-sm font-semibold text-white font-mono flex items-center gap-2">
            <span>👥</span> Authorized Platform Users
          </h3>
          <span class="text-xs font-mono text-slate-400">({{ filteredUsers.length }} / {{ users.length }})</span>
        </div>

        <div class="flex flex-wrap items-center gap-2">
          <!-- Search input -->
          <input
            v-model="searchQuery"
            type="text"
            placeholder="Search name or email..."
            class="px-2.5 py-1.5 rounded-lg bg-slate-950 border border-slate-800 text-xs text-white placeholder-slate-500 focus:outline-none focus:border-emerald-500 font-mono w-44 sm:w-56"
          />

          <!-- Role Filter -->
          <select
            v-model="roleFilter"
            class="px-2.5 py-1.5 rounded-lg bg-slate-950 border border-slate-800 text-xs text-white font-mono focus:outline-none focus:border-emerald-500"
          >
            <option value="all">All Roles</option>
            <option value="admin">Admin</option>
            <option value="operator">Operator</option>
            <option value="viewer">Viewer</option>
          </select>

          <!-- Status Filter -->
          <select
            v-model="statusFilter"
            class="px-2.5 py-1.5 rounded-lg bg-slate-950 border border-slate-800 text-xs text-white font-mono focus:outline-none focus:border-emerald-500"
          >
            <option value="all">All Status</option>
            <option value="active">Active Only</option>
            <option value="inactive">Disabled Only</option>
          </select>

          <button
            @click="fetchUsers"
            class="px-2.5 py-1.5 rounded-lg bg-slate-800 hover:bg-slate-700 text-slate-300 text-xs font-mono border border-slate-700"
            title="Refresh Users"
          >
            🔄
          </button>
        </div>
      </div>

      <div class="overflow-x-auto">
        <table class="w-full text-left text-xs border-collapse">
          <thead>
            <tr class="border-b border-slate-800 bg-slate-900/80 font-mono text-slate-400">
              <th class="p-3.5">User</th>
              <th class="p-3.5">Role</th>
              <th class="p-3.5">Account Status</th>
              <th class="p-3.5">Enrolled At</th>
              <th v-if="authStore.isAdmin" class="p-3.5 text-right">Actions</th>
            </tr>
          </thead>
          <tbody class="divide-y divide-slate-800/60 font-mono text-[11px]">
            <tr v-for="u in filteredUsers" :key="u.id" class="hover:bg-slate-800/30">
              <td class="p-3.5">
                <div class="font-bold text-white flex items-center gap-1.5">
                  <span>{{ u.full_name || 'System Operator' }}</span>
                  <span v-if="u.id === authStore.user?.id" class="px-1.5 py-0.2 rounded bg-emerald-500/20 text-emerald-400 text-[9px]">YOU</span>
                </div>
                <div class="text-slate-400 text-[10px]">{{ u.email }}</div>
              </td>
              <td class="p-3.5">
                <span
                  class="px-2 py-0.5 rounded text-[10px] font-bold uppercase"
                  :class="{
                    'bg-purple-500/10 text-purple-400 border border-purple-500/30': u.role === 'admin',
                    'bg-blue-500/10 text-blue-400 border border-blue-500/30': u.role === 'operator',
                    'bg-slate-700/30 text-slate-400 border border-slate-600/30': u.role === 'viewer'
                  }"
                >
                  {{ u.role }}
                </span>
              </td>
              <td class="p-3.5">
                <span
                  class="inline-flex items-center gap-1 px-2 py-0.5 rounded text-[10px] font-bold"
                  :class="u.is_active ? 'bg-emerald-500/10 text-emerald-400 border border-emerald-500/30' : 'bg-rose-500/10 text-rose-400 border border-rose-500/30'"
                >
                  <span class="w-1.5 h-1.5 rounded-full" :class="u.is_active ? 'bg-emerald-400' : 'bg-rose-400'"></span>
                  {{ u.is_active ? 'ACTIVE' : 'DISABLED' }}
                </span>
              </td>
              <td class="p-3.5 text-slate-400">
                {{ formatDate(u.created_at) }}
              </td>
              <td v-if="authStore.isAdmin" class="p-3.5 text-right space-x-1.5">
                <button
                  @click="openEditModal(u)"
                  class="px-2 py-1 rounded bg-slate-800 hover:bg-slate-700 text-slate-200 text-[10px] font-mono border border-slate-700 transition-all"
                >
                  Edit
                </button>
                <button
                  @click="toggleUserActive(u)"
                  :disabled="u.id === authStore.user?.id"
                  class="px-2 py-1 rounded text-[10px] font-mono border transition-all disabled:opacity-30 disabled:cursor-not-allowed"
                  :class="u.is_active ? 'bg-amber-500/10 hover:bg-amber-500/20 text-amber-400 border-amber-500/30' : 'bg-emerald-500/10 hover:bg-emerald-500/20 text-emerald-400 border-emerald-500/30'"
                >
                  {{ u.is_active ? 'Disable' : 'Enable' }}
                </button>
                <button
                  @click="confirmDeleteUser(u)"
                  :disabled="u.id === authStore.user?.id"
                  class="px-2 py-1 rounded bg-rose-500/10 hover:bg-rose-500/20 text-rose-400 text-[10px] font-mono border border-rose-500/30 transition-all disabled:opacity-30 disabled:cursor-not-allowed"
                >
                  Delete
                </button>
              </td>
            </tr>
            <tr v-if="filteredUsers.length === 0">
              <td colspan="5" class="p-6 text-center text-slate-500 font-mono">
                No matching users found.
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
              <td class="p-3 text-slate-400">{{ formatDateTime(log.created_at) }}</td>
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

    <!-- Edit User Modal -->
    <div v-if="editUserData" class="fixed inset-0 bg-black/60 backdrop-blur-sm flex items-center justify-center p-4 z-50">
      <div class="w-full max-w-md bg-slate-900 border border-slate-800 rounded-2xl p-6 shadow-2xl space-y-4">
        <h3 class="text-base font-bold text-white">Edit User: {{ editUserData.email }}</h3>
        <form @submit.prevent="submitEditUser" class="space-y-4">
          <div>
            <label class="block text-xs font-mono text-slate-300 mb-1">Full Name</label>
            <input v-model="editForm.full_name" required class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white focus:outline-none focus:border-emerald-500" />
          </div>
          <div>
            <label class="block text-xs font-mono text-slate-300 mb-1">Email</label>
            <input v-model="editForm.email" type="email" required class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white font-mono focus:outline-none focus:border-emerald-500" />
          </div>
          <div>
            <label class="block text-xs font-mono text-slate-300 mb-1">Role</label>
            <select v-model="editForm.role" class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white font-mono">
              <option value="operator">Operator</option>
              <option value="admin">Administrator</option>
              <option value="viewer">Viewer</option>
            </select>
          </div>
          <div>
            <label class="block text-xs font-mono text-slate-300 mb-1">Reset Password (leave empty to keep current)</label>
            <input v-model="editForm.password" type="password" minlength="8" class="w-full px-3 py-2 rounded-lg bg-slate-950 border border-slate-800 text-sm text-white font-mono focus:outline-none focus:border-emerald-500" placeholder="Optional new password" />
          </div>
          <div class="flex items-center gap-2 pt-1">
            <input id="edit_active" type="checkbox" v-model="editForm.is_active" class="rounded bg-slate-950 border-slate-800 text-emerald-500 focus:ring-0" />
            <label for="edit_active" class="text-xs font-mono text-slate-300">Account Active</label>
          </div>
          <div class="flex justify-end gap-2 pt-2">
            <button type="button" @click="editUserData = null" class="px-4 py-2 text-xs text-slate-400 hover:text-white">Cancel</button>
            <button type="submit" class="px-4 py-2 bg-emerald-500 hover:bg-emerald-600 text-white text-xs font-semibold rounded-lg">Save Changes</button>
          </div>
        </form>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, onMounted } from 'vue'
import apiClient from '@/api/client'
import { useAuthStore } from '@/stores/authStore'
import type { User, AuditLog } from '@/types'
import { formatDate, formatDateTime } from '@/utils/date'

const authStore = useAuthStore()
const users = ref<User[]>([])
const auditLogs = ref<AuditLog[]>([])
const showUserModal = ref(false)
const editUserData = ref<User | null>(null)
const notification = ref<{ type: 'success' | 'error'; message: string } | null>(null)

const searchQuery = ref('')
const roleFilter = ref('all')
const statusFilter = ref('all')

const newUser = ref({
  full_name: '',
  email: '',
  password: '',
  role: 'operator'
})

const editForm = ref({
  full_name: '',
  email: '',
  role: 'operator',
  password: '',
  is_active: true
})

const filteredUsers = computed(() => {
  return users.value.filter(u => {
    // Search query
    if (searchQuery.value) {
      const q = searchQuery.value.toLowerCase()
      const matchName = (u.full_name || '').toLowerCase().includes(q)
      const matchEmail = u.email.toLowerCase().includes(q)
      if (!matchName && !matchEmail) return false
    }
    // Role filter
    if (roleFilter.value !== 'all' && u.role !== roleFilter.value) {
      return false
    }
    // Status filter
    if (statusFilter.value === 'active' && !u.is_active) return false
    if (statusFilter.value === 'inactive' && u.is_active) return false
    return true
  })
})

function notify(type: 'success' | 'error', message: string) {
  notification.value = { type, message }
  setTimeout(() => {
    if (notification.value?.message === message) {
      notification.value = null
    }
  }, 5000)
}

async function fetchUsers() {
  if (!authStore.isAdmin) return
  try {
    const res = await apiClient.get<User[]>('/auth/users')
    users.value = res.data
  } catch (err: any) {
    console.error('Failed to load users', err)
    notify('error', err.response?.data?.detail || 'Failed to load users')
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

function openCreateModal() {
  newUser.value = { full_name: '', email: '', password: '', role: 'operator' }
  showUserModal.value = true
}

async function submitCreateUser() {
  try {
    await apiClient.post('/auth/register', newUser.value)
    showUserModal.value = false
    notify('success', `User ${newUser.value.email} created successfully`)
    newUser.value = { full_name: '', email: '', password: '', role: 'operator' }
    await fetchUsers()
    await fetchAuditLogs()
  } catch (err: any) {
    notify('error', err.response?.data?.detail || 'Failed to enroll user')
  }
}

function openEditModal(u: User) {
  editUserData.value = u
  editForm.value = {
    full_name: u.full_name || '',
    email: u.email,
    role: u.role,
    password: '',
    is_active: u.is_active
  }
}

async function submitEditUser() {
  if (!editUserData.value) return
  try {
    const payload: any = {
      full_name: editForm.value.full_name,
      email: editForm.value.email,
      role: editForm.value.role,
      is_active: editForm.value.is_active
    }
    if (editForm.value.password.trim().length >= 8) {
      payload.password = editForm.value.password.trim()
    }
    await apiClient.patch(`/auth/users/${editUserData.value.id}`, payload)
    notify('success', `User ${editUserData.value.email} updated successfully`)
    editUserData.value = null
    await fetchUsers()
    await fetchAuditLogs()
  } catch (err: any) {
    notify('error', err.response?.data?.detail || 'Failed to update user')
  }
}

async function toggleUserActive(u: User) {
  const actionText = u.is_active ? 'deactivate' : 'activate'
  if (!confirm(`Are you sure you want to ${actionText} account for "${u.email}"?`)) return
  try {
    await apiClient.patch(`/auth/users/${u.id}`, { is_active: !u.is_active })
    notify('success', `User ${u.email} is now ${!u.is_active ? 'active' : 'disabled'}`)
    await fetchUsers()
    await fetchAuditLogs()
  } catch (err: any) {
    notify('error', err.response?.data?.detail || `Failed to ${actionText} user`)
  }
}

async function confirmDeleteUser(u: User) {
  if (!confirm(`CAUTION: Are you sure you want to delete/deactivate user "${u.email}"? This action is audited.`)) return
  try {
    await apiClient.delete(`/auth/users/${u.id}`)
    notify('success', `User ${u.email} deleted/deactivated successfully`)
    await fetchUsers()
    await fetchAuditLogs()
  } catch (err: any) {
    notify('error', err.response?.data?.detail || 'Failed to delete user')
  }
}

onMounted(() => {
  fetchUsers()
  fetchAuditLogs()
})
</script>
