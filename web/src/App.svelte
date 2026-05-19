<script lang="ts">
  import { onMount } from 'svelte'
  import {
    activeTab,
    playerState,
    queueStore,
    snapshot,
    backendOnline,
  } from './lib/stores'
  import { api, setToken, ApiError } from './lib/api'
  import { connectSnapshot } from './lib/ws'
  import Nav from './components/Nav.svelte'
  import NowPlaying from './pages/NowPlaying.svelte'
  import Library from './pages/Library.svelte'
  import Queue from './pages/Queue.svelte'
  import Pipeline from './pages/Pipeline.svelte'
  import { KeyRound } from 'lucide-svelte'

  let needToken = $state(false)
  let tokenInput = $state('')

  function saveToken() {
    setToken(tokenInput)
    needToken = false
    poll()
  }

  async function poll() {
    try {
      const [st, q] = await Promise.all([api.getState(), api.getQueue()])
      $playerState = st
      $queueStore = q
      $backendOnline = true
      needToken = false
    } catch (e: unknown) {
      // Only surface the token prompt on an explicit 401; network errors just
      // mean the daemon is down, not that auth is required.
      if (e instanceof ApiError && e.status === 401) {
        needToken = true
      }
      $backendOnline = false
    }
  }

  // Hardware-volume target: the first control that has both a volume and a
  // mute switch (the iFi's stereo PCM,0), falling back to any volume control.
  // PCM,1 is a second series gain stage — left pinned at 100%, not touched.
  async function adjustVolume(delta: number) {
    try {
      const ctrls = await api.getMixer()
      const c =
        ctrls.find((x) => x.has_volume && x.has_switch) ??
        ctrls.find((x) => x.has_volume)
      if (!c) return
      const cur = c.channel_pct[0] ?? 0
      const next = Math.min(100, Math.max(0, cur + delta))
      await api.setMixerControl(c.name, c.index, 'volume', next)
    } catch {
      /* offline */
    }
  }

  async function toggleMute() {
    try {
      const ctrls = await api.getMixer()
      const c =
        ctrls.find((x) => x.has_volume && x.has_switch) ??
        ctrls.find((x) => x.has_switch)
      if (!c) return
      const on = c.channel_switch[0] === 1
      await api.setMixerControl(c.name, c.index, 'switch', !on)
    } catch {
      /* offline */
    }
  }

  async function togglePlay() {
    try {
      if ($playerState?.state === 'Playing') await api.pause()
      else await api.play()
    } catch {
      /* offline */
    }
  }

  function isTextTarget(t: EventTarget | null): boolean {
    if (!(t instanceof HTMLElement)) return false
    const tag = t.tagName
    return (
      tag === 'INPUT' ||
      tag === 'TEXTAREA' ||
      tag === 'SELECT' ||
      t.isContentEditable
    )
  }

  function onGlobalKey(e: KeyboardEvent) {
    if (e.ctrlKey || e.altKey || e.metaKey) return
    if (isTextTarget(e.target)) return
    switch (e.key) {
      case 'm':
      case 'M':
        e.preventDefault()
        toggleMute()
        break
      case '-':
      case '_':
        e.preventDefault()
        adjustVolume(-3)
        break
      case '=':
      case '+':
        e.preventDefault()
        adjustVolume(3)
        break
      case ' ':
        e.preventDefault()
        togglePlay()
        break
    }
  }

  onMount(() => {
    poll()
    const pollTimer = setInterval(poll, 1000)

    const disconnect = connectSnapshot((snap) => {
      $snapshot = snap
      $backendOnline = true
    })

    window.addEventListener('keydown', onGlobalKey)

    return () => {
      clearInterval(pollTimer)
      disconnect()
      window.removeEventListener('keydown', onGlobalKey)
    }
  })
</script>

<div class="app-bg"></div>

<Nav />

<main class="min-h-0 flex-1">
  {#if $activeTab === 'nowplaying'}
    <NowPlaying />
  {:else if $activeTab === 'library'}
    <Library />
  {:else if $activeTab === 'queue'}
    <Queue />
  {:else if $activeTab === 'pipeline'}
    <Pipeline />
  {/if}
</main>

{#if needToken}
  <div
    class="fixed inset-0 z-[100] grid place-items-center bg-black/70 backdrop-blur-md"
  >
    <div class="glass-strong w-full max-w-md p-7">
      <div class="mb-4 flex items-center gap-3">
        <div
          class="grid h-11 w-11 place-items-center rounded-full bg-accent/20 text-accent"
        >
          <KeyRound size={20} />
        </div>
        <div>
          <h2 class="text-lg font-bold text-white">Access token</h2>
          <p class="text-xs text-white/50">
            Set in <span class="font-mono">[web] token</span> in the daemon
            config.
          </p>
        </div>
      </div>

      <input
        type="password"
        bind:value={tokenInput}
        placeholder="Bearer token"
        class="w-full rounded-xl border border-white/15 bg-white/[0.06] px-4 py-3 text-sm text-white placeholder:text-white/35 focus:border-accent/50 focus:outline-none"
        onkeydown={(e) => e.key === 'Enter' && saveToken()}
      />

      <div class="mt-5 flex gap-3">
        <button
          class="flex-1 rounded-xl bg-accent py-2.5 text-sm font-semibold text-black transition hover:brightness-110"
          onclick={saveToken}
        >
          Save
        </button>
        <button
          class="rounded-xl border border-white/15 px-5 py-2.5 text-sm text-white/70 transition hover:bg-white/10"
          onclick={() => { needToken = false }}
        >
          Skip
        </button>
      </div>
    </div>
  </div>
{/if}
