import { defineStore } from 'pinia'
import { ref, computed } from 'vue'
import apiClient from '@/api/client'
import type { User } from '@/types'

export const useAuthStore = defineStore('auth', () => {
  const token = ref<string | null>(localStorage.getItem('smartgrow_token'))
  const user = ref<User | null>(
    localStorage.getItem('smartgrow_user')
      ? JSON.parse(localStorage.getItem('smartgrow_user')!)
      : null
  )

  const isAuthenticated = computed(() => !!token.value)
  const isAdmin = computed(() => user.value?.role === 'admin' || user.value?.is_superuser)
  const isOperator = computed(() => ['admin', 'operator'].includes(user.value?.role || ''))

  async function login(username: string, password: string):Promise<boolean> {
    const formData = new URLSearchParams()
    formData.append('username', username)
    formData.append('password', password)

    const response = await apiClient.post('/auth/login', formData, {
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' }
    })

    token.value = response.data.access_token
    localStorage.setItem('smartgrow_token', token.value!)

    // Fetch user profile
    const meRes = await apiClient.get<User>('/auth/me')
    user.value = meRes.data
    localStorage.setItem('smartgrow_user', JSON.stringify(user.value))

    return true
  }

  function logout() {
    token.value = null
    user.value = null
    localStorage.removeItem('smartgrow_token')
    localStorage.removeItem('smartgrow_user')
  }

  async function fetchCurrentUser() {
    if (!token.value) return null
    try {
      const res = await apiClient.get<User>('/auth/me')
      user.value = res.data
      localStorage.setItem('smartgrow_user', JSON.stringify(user.value))
      return user.value
    } catch {
      logout()
      return null
    }
  }

  return {
    token,
    user,
    isAuthenticated,
    isAdmin,
    isOperator,
    login,
    logout,
    fetchCurrentUser
  }
})
