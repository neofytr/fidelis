<script lang="ts">
  import { onMount } from 'svelte'
  import { api, type FsListing, type FsEntry } from '../lib/api'
  import { X, Folder, FileAudio, ChevronUp, Plus, ListPlus } from 'lucide-svelte'

  let {
    onClose,
    onMutated,
  }: {
    onClose: () => void
    onMutated: () => void
  } = $props()

  let listing = $state<FsListing | null>(null)
  let loading = $state(true)
  let error = $state<string | null>(null)
  let busy = $state(false)

  async function open(path?: string) {
    loading = true
    error = null
    try {
      listing = await api.listFs(path)
    } catch (e: unknown) {
      error = e instanceof Error ? e.message : 'failed to list'
      listing = null
    } finally {
      loading = false
    }
  }

  onMount(() => open())

  async function clickEntry(e: FsEntry) {
    if (e.is_dir) {
      await open(e.path)
    } else {
      busy = true
      try {
        await api.appendToQueue(e.path)
        onMutated()
      } finally {
        busy = false
      }
    }
  }

  async function appendThisFolder() {
    if (!listing) return
    busy = true
    try {
      await api.appendFolder(listing.path)
      onMutated()
    } finally {
      busy = false
    }
  }

  function up() {
    if (listing?.parent) open(listing.parent)
  }
</script>

<div
  class="fixed inset-0 z-50 flex items-start justify-center bg-black/70"
  role="button"
  tabindex="-1"
  onclick={(e) => { if (e.target === e.currentTarget) onClose() }}
  onkeydown={(e) => e.key === 'Escape' && onClose()}
>
  <div class="mt-12 w-full max-w-2xl border border-[#2b2b2b] bg-[#0a0a0a]">
    <div
      class="flex items-center justify-between border-b border-[#1f1f1f] px-4 py-3"
    >
      <span class="text-xs uppercase tracking-[0.18em] text-white/70">
        Browse · append from any folder
      </span>
      <button
        class="p-1 text-white/40 transition hover:text-white"
        onclick={onClose}
        aria-label="Close"
      >
        <X size={15} />
      </button>
    </div>

    <div class="border-b border-[#1f1f1f] px-4 py-2 flex items-center gap-2">
      <button
        class="p-1 text-white/50 transition hover:text-white disabled:opacity-30"
        onclick={up}
        disabled={!listing?.parent}
        aria-label="Up"
        title="Up one level"
      >
        <ChevronUp size={14} />
      </button>
      <span class="flex-1 truncate font-mono text-[11px] text-white/60">
        {listing?.path ?? '…'}
      </span>
      {#if listing && listing.entries.some((e) => !e.is_dir)}
        <button
          class="flex items-center gap-1.5 border border-[#2b2b2b] px-2.5 py-1 text-[10px] uppercase tracking-[0.12em] text-accent transition hover:bg-accent hover:text-black disabled:opacity-40"
          onclick={appendThisFolder}
          disabled={busy}
          title="Recursively append every audio file in this folder"
        >
          <ListPlus size={11} /> Append all
        </button>
      {/if}
    </div>

    <div class="max-h-[64vh] overflow-y-auto">
      {#if loading}
        <div class="px-4 py-10 text-center text-xs text-white/40">Loading…</div>
      {:else if error}
        <div class="px-4 py-10 text-center text-xs text-amber-300/80">
          {error}
        </div>
      {:else if !listing || listing.entries.length === 0}
        <div class="px-4 py-10 text-center text-xs text-white/40">
          Empty (no audio files or subfolders here).
        </div>
      {:else}
        <ul>
          {#each listing.entries as e (e.path)}
            <li>
              <button
                class="flex w-full items-center gap-3 border-b border-[#161616] px-4 py-2 text-left transition hover:bg-[#141414] disabled:opacity-40"
                onclick={() => clickEntry(e)}
                disabled={busy}
              >
                <span class="text-white/40">
                  {#if e.is_dir}
                    <Folder size={14} />
                  {:else}
                    <FileAudio size={14} />
                  {/if}
                </span>
                <span
                  class="flex-1 truncate text-[12px] {e.is_dir
                    ? 'text-white/90'
                    : 'text-white/75'}"
                >
                  {e.name}
                </span>
                {#if !e.is_dir}
                  <Plus size={12} class="text-accent opacity-70" />
                {/if}
              </button>
            </li>
          {/each}
        </ul>
      {/if}
    </div>
  </div>
</div>
