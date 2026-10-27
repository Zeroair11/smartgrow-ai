/**
 * SmartGrow AI - Centralized Date & Timezone Utilities
 * Standard timezone: Asia/Ho_Chi_Minh (UTC+07:00)
 */

export const VIETNAM_TIMEZONE = 'Asia/Ho_Chi_Minh'

/**
 * Safely parses any date input into a valid Date object.
 * If the input string is an ISO format lacking timezone info (e.g. "2026-10-27T03:11:23"),
 * it is normalized to UTC as per SmartGrow system convention.
 */
export function parseDate(val: string | number | Date | null | undefined): Date | null {
  if (val === null || val === undefined || val === '') return null
  if (val instanceof Date) return isNaN(val.getTime()) ? null : val
  if (typeof val === 'number') {
    const d = new Date(val)
    return isNaN(d.getTime()) ? null : d
  }
  if (typeof val === 'string') {
    let s = val.trim()
    if (!s) return null
    // If ISO date string has no offset or Z (e.g. "2026-10-27T03:11:23" or "2026-10-27 03:11:23")
    if (/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(\.\d+)?$/.test(s)) {
      s += 'Z'
    } else if (/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}(\.\d+)?$/.test(s)) {
      s = s.replace(' ', 'T') + 'Z'
    }
    const d = new Date(s)
    return isNaN(d.getTime()) ? null : d
  }
  return null
}

/**
 * Format date and time in Vietnam timezone (Asia/Ho_Chi_Minh) in 24-hour format.
 * Output: DD/MM/YYYY, HH:mm:ss
 */
export function formatDateTime(val: string | number | Date | null | undefined, fallback: string = '—'): string {
  const d = parseDate(val)
  if (!d) return fallback
  return new Intl.DateTimeFormat('vi-VN', {
    timeZone: VIETNAM_TIMEZONE,
    year: 'numeric',
    month: '2-digit',
    day: '2-digit',
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
    hour12: false
  }).format(d)
}

/**
 * Format time only in Vietnam timezone (Asia/Ho_Chi_Minh) in 24-hour format.
 * Output: HH:mm:ss
 */
export function formatTime(val: string | number | Date | null | undefined, fallback: string = '—'): string {
  const d = parseDate(val)
  if (!d) return fallback
  return new Intl.DateTimeFormat('vi-VN', {
    timeZone: VIETNAM_TIMEZONE,
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
    hour12: false
  }).format(d)
}

/**
 * Format date only in Vietnam timezone (Asia/Ho_Chi_Minh).
 * Output: DD/MM/YYYY
 */
export function formatDate(val: string | number | Date | null | undefined, fallback: string = '—'): string {
  const d = parseDate(val)
  if (!d) return fallback
  return new Intl.DateTimeFormat('vi-VN', {
    timeZone: VIETNAM_TIMEZONE,
    year: 'numeric',
    month: '2-digit',
    day: '2-digit'
  }).format(d)
}
