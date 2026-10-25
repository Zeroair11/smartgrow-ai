import { createRouter, createWebHistory } from 'vue-router'
import { useAuthStore } from '@/stores/authStore'

const router = createRouter({
  history: createWebHistory(),
  routes: [
    {
      path: '/login',
      name: 'login',
      component: () => import('@/views/LoginView.vue'),
      meta: { public: true }
    },
    {
      path: '/',
      component: () => import('@/layouts/AppLayout.vue'),
      children: [
        {
          path: '',
          name: 'dashboard',
          component: () => import('@/views/DashboardView.vue'),
          meta: { title: 'Dashboard & Quick Telemetry' }
        },
        {
          path: 'greenhouses',
          name: 'greenhouses',
          component: () => import('@/views/GreenhousesView.vue'),
          meta: { title: 'Greenhouses & Zones' }
        },
        {
          path: 'devices',
          name: 'devices',
          component: () => import('@/views/DevicesView.vue'),
          meta: { title: 'ESP32-S3 Device Management' }
        },
        {
          path: 'monitoring',
          name: 'monitoring',
          component: () => import('@/views/MonitoringView.vue'),
          meta: { title: 'Live Sensor Telemetry' }
        },
        {
          path: 'edge-ai',
          name: 'edge-ai',
          component: () => import('@/views/EdgeAiView.vue'),
          meta: { title: 'Edge AI Center & TinyML' }
        },
        {
          path: 'automation',
          name: 'automation',
          component: () => import('@/views/AutomationView.vue'),
          meta: { title: 'Automation Rules & Engine' }
        },
        {
          path: 'irrigation',
          name: 'irrigation',
          component: () => import('@/views/IrrigationView.vue'),
          meta: { title: 'Irrigation & Pump Safety Control' }
        },
        {
          path: 'alerts',
          name: 'alerts',
          component: () => import('@/views/AlertsView.vue'),
          meta: { title: 'Alerts & Incidents' }
        },
        {
          path: 'schedules',
          name: 'schedules',
          component: () => import('@/views/ScheduleView.vue'),
          meta: { title: 'Schedules & Maintenance' }
        },
        {
          path: 'analytics',
          name: 'analytics',
          component: () => import('@/views/AnalyticsView.vue'),
          meta: { title: 'Analytics & Reports' }
        },
        {
          path: 'settings',
          name: 'settings',
          component: () => import('@/views/SettingsView.vue'),
          meta: { title: 'System Settings & Calibration' }
        },
        {
          path: 'users-audit',
          name: 'users-audit',
          component: () => import('@/views/UsersAuditView.vue'),
          meta: { title: 'Users & Audit Logs' }
        }
      ]
    }
  ]
})

router.beforeEach((to, from, next) => {
  const authStore = useAuthStore()
  if (!to.meta.public && !authStore.isAuthenticated) {
    next({ name: 'login' })
  } else if (to.name === 'login' && authStore.isAuthenticated) {
    next({ name: 'dashboard' })
  } else {
    next()
  }
})

export default router
