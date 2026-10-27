/**
 * SmartGrow AI - Authorized CSV Export & Download Utility
 */
import apiClient from '@/api/client'

export interface CsvExportOptions {
  params?: Record<string, any>
  defaultFilename?: string
}

/**
 * Downloads a CSV file from an authenticated API endpoint.
 * Automatically handles Bearer token authorization, filename extraction,
 * Blob creation, and DOM URL cleanup.
 */
export async function downloadCsvFromApi(
  endpoint: string,
  options: CsvExportOptions = {}
): Promise<string> {
  const { params = {}, defaultFilename = `smartgrow_export_${Date.now()}.csv` } = options

  const response = await apiClient.get(endpoint, {
    params,
    responseType: 'blob'
  })

  // Extract filename from Content-Disposition header if available
  let filename = defaultFilename
  const disposition = response.headers['content-disposition'] || response.headers['Content-Disposition']
  if (disposition && typeof disposition === 'string') {
    const filenameMatch = disposition.match(/filename\*?=(?:UTF-8'')?["']?([^"';]+)["']?/i)
    if (filenameMatch && filenameMatch[1]) {
      filename = decodeURIComponent(filenameMatch[1].trim())
    }
  }

  // Create Blob and initiate download
  const blob = new Blob([response.data], { type: 'text/csv;charset=utf-8;' })
  const url = window.URL.createObjectURL(blob)
  const link = document.createElement('a')
  link.href = url
  link.setAttribute('download', filename)
  link.style.display = 'none'
  document.body.appendChild(link)
  link.click()

  // Clean up
  document.body.removeChild(link)
  setTimeout(() => {
    window.URL.revokeObjectURL(url)
  }, 1000)

  return filename
}
