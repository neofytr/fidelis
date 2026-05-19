<script lang="ts">
  import { playerState, snapshot } from '../lib/stores'
  import { parseFormat } from '../lib/ws'
  import { api } from '../lib/api'
  import CoverArt from '../components/CoverArt.svelte'
  import Seekbar from '../components/Seekbar.svelte'
  import TransportControls from '../components/TransportControls.svelte'
  import { onMount } from 'svelte'

  let ps = $derived($playerState)
  let snap = $derived($snapshot)

  let track = $derived(ps?.current_track ?? null)

  // Sample rate drives time formatting; prefer the live matched rate, fall
  // back to source rate, then 44.1k.
  let matchedFmt = $derived(parseFormat(snap?.format_match.matched))
  let sampleRate = $derived(
    matchedFmt?.rate || snap?.source.sample_rate_hz || 44100,
  )

  // /api/art/current resolves art from the engine's live source path. The URL
  // is constant, so a path-keyed query busts the browser cache (and retriggers
  // CoverArt's crossfade) on every track change.
  let artSrc = $derived<string | null>(
    track ? `${api.currentArtUrl()}?t=${encodeURIComponent(track.path)}` : null,
  )

  let format = $derived(
    matchedFmt
      ? `${matchedFmt.fmt} · ${(matchedFmt.rate / 1000).toFixed(1)} kHz · ${
          matchedFmt.channels
        }ch`
      : (track?.format ?? ''),
  )

  let position = $derived(ps?.position_frames ?? 0)
  let total = $derived(
    track?.duration_frames || snap?.source.total_frames || 0,
  )

  // Space play/pause is handled globally in App.svelte; here only the
  // page-local ±10s seek on the arrow keys.
  async function onKey(e: KeyboardEvent) {
    if (e.target instanceof HTMLInputElement) return
    if (e.code === 'ArrowLeft') {
      e.preventDefault()
      try {
        await api.seek(Math.max(0, position - 10 * sampleRate))
      } catch {
        /* offline */
      }
    } else if (e.code === 'ArrowRight') {
      e.preventDefault()
      try {
        await api.seek(Math.min(total || position, position + 10 * sampleRate))
      } catch {
        /* offline */
      }
    }
  }

  onMount(() => {
    window.addEventListener('keydown', onKey)
    return () => window.removeEventListener('keydown', onKey)
  })
</script>

<div class="flex h-full w-full items-center justify-center bg-black p-8">
  <div
    class="flex w-full max-w-3xl flex-col gap-6 border border-[#2b2b2b] bg-[#0c0c0c] p-6 md:flex-row md:items-center"
  >
    <div class="mx-auto w-48 shrink-0 md:mx-0">
      <CoverArt srcOverride={artSrc} size="large" />
    </div>

    <div class="flex min-w-0 flex-1 flex-col gap-6">
      <div class="min-w-0">
        <div
          class="mb-2 text-[10px] uppercase tracking-[0.2em] text-white/35"
        >
          {track ? 'Now Playing' : 'Idle'}
        </div>
        {#if track}
          <h1 class="truncate text-lg font-semibold leading-tight text-white">
            {track.title || track.path.split('/').pop()}
          </h1>
          <p class="mt-1 truncate text-sm text-white/65">
            {track.artist || 'Unknown artist'}
          </p>
          <p class="mt-0.5 truncate text-xs text-white/40">
            {track.album || ''}
          </p>
        {:else}
          <h1 class="text-base font-semibold text-white/60">
            Nothing playing
          </h1>
          <p class="mt-1 text-xs text-white/35">
            Queue a track from the Library.
          </p>
        {/if}

        {#if format}
          <span
            class="mt-4 inline-block border border-[#2b2b2b] bg-black px-2.5 py-1 text-xs text-accent"
          >
            {format}
          </span>
        {/if}
      </div>

      <Seekbar
        position_frames={position}
        total_frames={total}
        sample_rate={sampleRate}
      />

      <TransportControls />

      <div
        class="flex justify-center gap-6 text-[10px] uppercase tracking-[0.15em] text-white/30"
      >
        <span>[space] play/pause</span>
        <span>[&larr;] -10s</span>
        <span>[&rarr;] +10s</span>
      </div>
    </div>
  </div>
</div>
