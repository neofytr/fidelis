<script lang="ts">
  import { activeTab, backendOnline } from '../lib/stores'
  import DevicePicker from './DevicePicker.svelte'
  import Mixer from './Mixer.svelte'

  const tabs = [
    { id: 'nowplaying', label: 'PLAY' },
    { id: 'library', label: 'LIBRARY' },
    { id: 'queue', label: 'QUEUE' },
    { id: 'pipeline', label: 'PIPELINE' },
  ] as const

  let showDevicePicker = $state(false)
  let showMixer = $state(false)
</script>

<nav
  class="flex w-full items-stretch overflow-x-auto whitespace-nowrap border-b border-[#1f1f1f] bg-[#080808] text-xs no-select"
>
  <div
    class="hidden sm:flex items-center border-r border-[#1f1f1f] px-3 font-semibold tracking-[0.2em] text-accent"
  >
    FIDELIS
  </div>

  {#each tabs as tab (tab.id)}
    <button
      class="shrink-0 border-r border-[#1f1f1f] px-3 sm:px-4 py-2 tracking-[0.15em] transition
        {$activeTab === tab.id
        ? 'bg-accent text-black'
        : 'text-white/45 hover:bg-[#141414] hover:text-white/80'}"
      onclick={() => ($activeTab = tab.id)}
    >
      {tab.label}
    </button>
  {/each}

  <button
    class="ml-auto shrink-0 border-l border-[#1f1f1f] px-3 sm:px-4 py-2 tracking-[0.15em] text-white/45 transition hover:bg-[#141414] hover:text-white/80"
    onclick={() => (showMixer = true)}
  >
    MIX
  </button>
  <button
    class="shrink-0 border-l border-[#1f1f1f] px-3 sm:px-4 py-2 tracking-[0.15em] text-white/45 transition hover:bg-[#141414] hover:text-white/80"
    onclick={() => (showDevicePicker = true)}
  >
    DAC
  </button>

  <div
    class="hidden sm:flex shrink-0 items-center gap-2 border-l border-[#1f1f1f] px-3 tracking-[0.15em]"
  >
    <span
      class="inline-block h-1.5 w-1.5 {$backendOnline
        ? 'bg-[#33d17a]'
        : 'animate-pulse bg-accent'}"
    ></span>
    <span class="text-white/35">
      {$backendOnline ? 'LIVE' : 'CONN…'}
    </span>
  </div>
</nav>

{#if showDevicePicker}
  <DevicePicker onClose={() => (showDevicePicker = false)} />
{/if}

{#if showMixer}
  <Mixer onClose={() => (showMixer = false)} />
{/if}
