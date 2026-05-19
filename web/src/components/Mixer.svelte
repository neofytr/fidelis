<script lang="ts">
  import { onMount } from 'svelte'
  import {
    api,
    type MixerControl,
    type ReplayGainState,
    type ReplayGainMode,
  } from '../lib/api'
  import { X, RefreshCw } from 'lucide-svelte'

  let { onClose }: { onClose: () => void } = $props()

  let controls = $state<MixerControl[]>([])
  let loading = $state(true)
  let error = $state(false)
  let rg = $state<ReplayGainState | null>(null)

  async function load() {
    try {
      const [c, r] = await Promise.all([api.getMixer(), api.getReplayGain()])
      controls = c
      rg = r
      error = false
    } catch {
      error = true
    } finally {
      loading = false
    }
  }

  async function setRgMode(mode: ReplayGainMode) {
    try {
      await api.setReplayGain({ mode })
      rg = await api.getReplayGain()
    } catch {
      error = true
    }
  }

  async function setRgClip(prevent_clipping: boolean) {
    try {
      await api.setReplayGain({ prevent_clipping })
      rg = await api.getReplayGain()
    } catch {
      error = true
    }
  }

  onMount(load)

  // One inflight write per control id; coalesce slider spam.
  const timers = new Map<string, ReturnType<typeof setTimeout>>()

  function key(c: MixerControl): string {
    return `${c.name}/${c.index}`
  }

  function pushVolume(c: MixerControl, pct: number) {
    // Optimistic: reflect immediately, debounce the network write.
    c.channel_pct = c.channel_pct.map(() => pct)
    const k = key(c)
    const t = timers.get(k)
    if (t) clearTimeout(t)
    timers.set(
      k,
      setTimeout(() => {
        timers.delete(k)
        api
          .setMixerControl(c.name, c.index, 'volume', pct)
          .then(load)
          .catch(() => { error = true })
      }, 90),
    )
  }

  function toggleSwitch(c: MixerControl) {
    const on = !(c.channel_switch[0] === 1)
    api
      .setMixerControl(c.name, c.index, 'switch', on)
      .then(load)
      .catch(() => { error = true })
  }

  function setEnum(c: MixerControl, item: number) {
    api
      .setMixerControl(c.name, c.index, 'enum', item)
      .then(load)
      .catch(() => { error = true })
  }

  function db(c: MixerControl): string {
    if (!c.has_db) return ''
    const lo = c.db_min_x100 / 100
    const hi = c.db_max_x100 / 100
    return `${lo.toFixed(0)}…${hi > 0 ? '+' : ''}${hi.toFixed(0)} dB`
  }
</script>

<div
  class="fixed inset-0 z-50 flex items-start justify-center bg-black/70"
  role="button"
  tabindex="-1"
  onclick={(e) => { if (e.target === e.currentTarget) onClose() }}
  onkeydown={(e) => e.key === 'Escape' && onClose()}
>
  <div class="mt-16 w-full max-w-xl border border-[#2b2b2b] bg-[#0a0a0a]">
    <div
      class="flex items-center justify-between border-b border-[#1f1f1f] px-4 py-3"
    >
      <span class="text-xs uppercase tracking-[0.18em] text-white/70">
        Mixer · alsamixer controls
      </span>
      <button
        class="p-1 text-white/40 transition hover:text-white"
        onclick={onClose}
        aria-label="Close"
      >
        <X size={15} />
      </button>
    </div>

    {#if rg}
      <div class="border-b border-[#1f1f1f] px-4 py-3">
        <div class="mb-2 flex items-baseline justify-between gap-3">
          <span class="text-[10px] uppercase tracking-[0.18em] text-white/55">
            ReplayGain
          </span>
          <span class="text-[10px] tabular-nums text-white/35">
            {rg.linear === 1
              ? 'no scaling · bit-perfect path'
              : `linear ${rg.linear.toFixed(3)} · ${(
                  20 * Math.log10(rg.linear)
                ).toFixed(2)} dB applied · QUALIFIED`}
          </span>
        </div>
        <div class="flex flex-wrap items-center gap-1.5">
          {#each ['off', 'album', 'track'] as m (m)}
            <button
              class="border px-2.5 py-1 text-[10px] uppercase tracking-[0.12em] transition
                {rg.mode === m
                  ? 'border-accent bg-accent text-black'
                  : 'border-[#2b2b2b] text-white/55 hover:border-white/30 hover:text-white/85'}"
              onclick={() => setRgMode(m as ReplayGainMode)}
            >
              {m}
            </button>
          {/each}
          <label class="ml-3 flex items-center gap-2 text-[10px] text-white/45">
            <input
              type="checkbox"
              checked={rg.prevent_clipping}
              onchange={(e) =>
                setRgClip((e.target as HTMLInputElement).checked)}
            />
            prevent clipping (peak-aware)
          </label>
        </div>
        {#if rg.mode !== 'off'}
          <div class="mt-1.5 text-[10px] text-amber-400/70">
            digital scaling on the decoder thread — Pipeline verdict will
            stay QUALIFIED while engaged.
          </div>
        {/if}
      </div>
    {/if}

    <div class="max-h-[64vh] overflow-y-auto">
      {#if loading}
        <div class="px-4 py-10 text-center text-xs text-white/40">
          Loading controls…
        </div>
      {:else if error}
        <div class="px-4 py-10 text-center text-xs text-white/40">
          Mixer unavailable for the active device.
        </div>
      {:else if controls.length === 0}
        <div class="px-4 py-10 text-center text-xs text-white/40">
          This device exposes no mixer controls.
        </div>
      {:else}
        <ul>
          {#each controls as c (key(c))}
            <li class="border-b border-[#161616] px-4 py-3">
              <div class="flex items-baseline justify-between gap-3">
                <span class="text-sm text-white/85">
                  {c.name}{c.index > 0 ? `,${c.index}` : ''}
                </span>
                <span class="text-[10px] tabular-nums text-white/35">
                  {db(c)}
                </span>
              </div>

              {#if c.has_volume && c.channel_pct.length > 0}
                {@const pct = c.channel_pct[0]}
                <div class="mt-2 flex items-center gap-3">
                  <input
                    type="range"
                    min="0"
                    max="100"
                    value={pct}
                    oninput={(e) =>
                      pushVolume(
                        c,
                        parseInt((e.target as HTMLInputElement).value, 10),
                      )}
                    class="mix-range h-1 flex-1 appearance-none bg-[#2a2a2a]"
                  />
                  <span
                    class="w-10 text-right text-xs tabular-nums text-accent"
                  >
                    {pct}%
                  </span>
                </div>
                {#if pct < 100}
                  <div class="mt-1 text-[10px] text-amber-400/70">
                    digital attenuation in DAC — not bit-perfect below 100%
                  </div>
                {/if}
              {/if}

              {#if c.is_enum && c.enum_items.length > 0}
                <select
                  class="mt-2 w-full border border-[#2b2b2b] bg-black px-2 py-1 text-xs text-white/80 focus:border-accent focus:outline-none"
                  value={c.enum_current}
                  onchange={(e) =>
                    setEnum(
                      c,
                      parseInt((e.target as HTMLSelectElement).value, 10),
                    )}
                >
                  {#each c.enum_items as it, i (i)}
                    <option value={i}>{it}</option>
                  {/each}
                </select>
              {/if}

              {#if c.has_switch && !c.has_volume && !c.is_enum}
                <button
                  class="mt-2 border px-3 py-1 text-xs transition
                    {c.channel_switch[0] === 1
                    ? 'border-accent bg-accent text-black'
                    : 'border-[#2b2b2b] text-white/55 hover:border-white/30'}"
                  onclick={() => toggleSwitch(c)}
                >
                  {c.channel_switch[0] === 1 ? 'ON' : 'OFF'}
                </button>
              {:else if c.has_switch}
                <label
                  class="mt-2 flex items-center gap-2 text-[11px] text-white/45"
                >
                  <input
                    type="checkbox"
                    checked={c.channel_switch[0] === 1}
                    onchange={() => toggleSwitch(c)}
                  />
                  {c.channel_switch[0] === 1 ? 'enabled' : 'muted'}
                </label>
              {/if}
            </li>
          {/each}
        </ul>
      {/if}
    </div>

    <div
      class="flex items-center gap-2 border-t border-[#1f1f1f] px-4 py-2 text-[10px] text-white/30"
    >
      <RefreshCw size={11} />
      <span>Hardware mixer · written straight to the DAC via ALSA</span>
    </div>
  </div>
</div>

<style>
  .mix-range::-webkit-slider-thumb {
    -webkit-appearance: none;
    width: 10px;
    height: 14px;
    background: rgb(var(--accent));
    cursor: pointer;
  }
  .mix-range::-moz-range-thumb {
    width: 10px;
    height: 14px;
    border: 0;
    background: rgb(var(--accent));
    cursor: pointer;
  }
</style>
