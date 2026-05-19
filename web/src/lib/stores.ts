import { writable, type Writable } from 'svelte/store'
import type { PlayerState, QueueItem } from './api'
import type { PipelineSnapshot } from './ws'

export const playerState: Writable<PlayerState | null> = writable(null)

export const queueStore: Writable<{
  tracks: QueueItem[]
  current_index: number
} | null> = writable(null)

export const snapshot: Writable<PipelineSnapshot | null> = writable(null)

// Pinned amber. The terminal theme does not tint from cover art; this stays
// constant and only exists so the --accent CSS variable has a single source.
export const accentColor: Writable<string> = writable('#ffb000')

export const activeTab: Writable<
  'nowplaying' | 'library' | 'queue' | 'pipeline'
> = writable('nowplaying')

// Connection health: true once any data has arrived from the backend.
export const backendOnline: Writable<boolean> = writable(false)

function hexToRgbTriplet(hex: string): string | null {
  const m = /^#?([0-9a-f]{6})$/i.exec(hex.trim())
  if (!m) return null
  const n = parseInt(m[1], 16)
  return `${(n >> 16) & 255} ${(n >> 8) & 255} ${n & 255}`
}

accentColor.subscribe((hex) => {
  if (typeof document === 'undefined') return
  const triplet = hexToRgbTriplet(hex)
  if (triplet) {
    document.documentElement.style.setProperty('--accent', triplet)
  }
})
