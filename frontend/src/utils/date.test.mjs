/**
 * Pure ES Module tests for date formatting logic in Vietnam timezone (Asia/Ho_Chi_Minh)
 */

const VIETNAM_TIMEZONE = 'Asia/Ho_Chi_Minh'

function parseDate(val) {
  if (val === null || val === undefined || val === '') return null
  if (val instanceof Date) return isNaN(val.getTime()) ? null : val
  if (typeof val === 'number') {
    const d = new Date(val)
    return isNaN(d.getTime()) ? null : d
  }
  if (typeof val === 'string') {
    let s = val.trim()
    if (!s) return null
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

function formatDateTime(val, fallback = '—') {
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

function formatTime(val, fallback = '—') {
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

function formatDate(val, fallback = '—') {
  const d = parseDate(val)
  if (!d) return fallback
  return new Intl.DateTimeFormat('vi-VN', {
    timeZone: VIETNAM_TIMEZONE,
    year: 'numeric',
    month: '2-digit',
    day: '2-digit'
  }).format(d)
}

function assert(condition, msg) {
  if (!condition) {
    console.error(`FAIL: ${msg}`)
    process.exit(1)
  }
  console.log(`PASS: ${msg}`)
}

console.log('=== Running Frontend Date & Timezone Tests ===')

// Test 1: UTC string converted to Vietnam time (UTC+7)
const utcIso = '2026-10-27T09:00:00Z'
const formattedUtc = formatDateTime(utcIso)
console.log(`UTC ${utcIso} => ${formattedUtc}`)
assert(formattedUtc.includes('16:00:00'), '2026-10-27T09:00:00Z must display as 16:00:00 in Vietnam')
assert(formattedUtc.includes('27/10/2026'), 'Date must be 27/10/2026')

// Test 2: Timestamp with +07:00 offset is NOT double-added
const offsetIso = '2026-10-27T16:00:00+07:00'
const formattedOffset = formatDateTime(offsetIso)
console.log(`Offset ${offsetIso} => ${formattedOffset}`)
assert(formattedOffset.includes('16:00:00'), '2026-10-27T16:00:00+07:00 must display as 16:00:00 without double addition')

// Test 3: ISO string lacking offset (historical SQLite naive string) is parsed as UTC
const naiveIso = '2026-10-27T09:00:00'
const formattedNaive = formatDateTime(naiveIso)
console.log(`Naive ${naiveIso} => ${formattedNaive}`)
assert(formattedNaive.includes('16:00:00'), 'Naive ISO 2026-10-27T09:00:00 must be interpreted as UTC and display as 16:00:00')

// Test 4: SQL space format lacking offset
const sqlNaive = '2026-10-27 09:00:00'
const formattedSql = formatDateTime(sqlNaive)
console.log(`SQL Naive ${sqlNaive} => ${formattedSql}`)
assert(formattedSql.includes('16:00:00'), 'SQL string 2026-10-27 09:00:00 must be interpreted as UTC and display as 16:00:00')

// Test 5: formatTime 24-hour test
const timeResult = formatTime('2026-10-27T09:00:00Z')
assert(timeResult === '16:00:00', `formatTime must return 16:00:00, got ${timeResult}`)

// Test 6: formatDate test
const dateResult = formatDate('2026-10-27T09:00:00Z')
assert(dateResult === '27/10/2026', `formatDate must return 27/10/2026, got ${dateResult}`)

// Test 7: Null, undefined, empty, invalid dates handle safely without crashing
assert(formatDateTime(null) === '—', 'null returns fallback —')
assert(formatDateTime(undefined) === '—', 'undefined returns fallback —')
assert(formatDateTime('') === '—', 'empty string returns fallback —')
assert(formatDateTime('invalid-date-string') === '—', 'invalid string returns fallback —')

console.log('=== All 7 Frontend Date & Timezone Tests PASSED ===')
