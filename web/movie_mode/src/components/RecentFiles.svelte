<script module lang="ts">
  import type { RecentFile } from '../lib/api';

  export interface RecentGroup {
    label: string;
    files: RecentFile[];
  }
</script>

<script lang="ts">
  import { ApiError } from '../lib/api';

  interface Props {
    /** The recent files of C-Play, fetched each time the list opens. */
    load: () => Promise<RecentGroup[]>;
    onopen: (file: RecentFile) => void;
    notify: (text: string, error?: boolean) => void;
    disabled?: boolean;
    title?: string;
  }

  let { load, onopen, notify, disabled = false, title = 'Open a recently loaded file in C-Play' }: Props = $props();

  let groups = $state<RecentGroup[] | null>(null);
  let element: HTMLElement | undefined = $state();

  async function toggle() {
    if (groups) {
      groups = null;
      return;
    }
    try {
      groups = await load();
    } catch (error) {
      notify(error instanceof ApiError ? `Could not list the recent files: ${error.message}` : 'Could not list the recent files', true);
    }
  }

  function open(file: RecentFile) {
    groups = null;
    onopen(file);
  }

  // Closes on a click outside the list.
  $effect(() => {
    if (!groups) return;
    const onpointerdown = (event: PointerEvent) => {
      if (!element?.contains(event.target as Node)) groups = null;
    };
    document.addEventListener('pointerdown', onpointerdown);
    return () => document.removeEventListener('pointerdown', onpointerdown);
  });

  function onkeydown(event: KeyboardEvent) {
    if (event.key === 'Escape' && groups) {
      event.preventDefault();
      groups = null;
    }
  }
</script>

<!-- svelte-ignore a11y_no_static_element_interactions -->
<div class="recent" bind:this={element} {onkeydown}>
  <button type="button" class="ghost" class:selected={!!groups} onclick={toggle} {disabled} {title} aria-expanded={!!groups}>Recent ▾</button>

  {#if groups}
    <div class="menu" role="menu">
      {#each groups as group (group.label)}
        <div class="group">
          {#if groups.length > 1}<span class="label">{group.label}</span>{/if}
          {#each group.files as file (file.path)}
            <button type="button" role="menuitem" onclick={() => open(file)} title={file.path}>
              <span class="file-name">{file.name}</span>
              <span class="path">{file.path}</span>
            </button>
          {:else}
            <span class="empty">No recent {group.label.toLowerCase()}</span>
          {/each}
        </div>
      {/each}
    </div>
  {/if}
</div>

<style>
  .recent {
    position: relative;
  }
  .selected {
    border-color: var(--next);
    box-shadow: inset 0 0 0 1px var(--next);
  }
  .menu {
    position: absolute;
    top: calc(100% + 0.35rem);
    right: 0;
    z-index: 10;
    width: min(28rem, calc(100vw - 2rem));
    max-height: min(32rem, 70vh);
    overflow-y: auto;
    display: flex;
    flex-direction: column;
    gap: 0.6rem;
    padding: 0.4rem;
    border: 1px solid var(--border);
    border-radius: var(--radius);
    background: var(--surface);
    box-shadow: 0 8px 30px rgb(0 0 0 / 0.4);
  }
  .group {
    display: flex;
    flex-direction: column;
    gap: 0.2rem;
  }
  .label {
    padding: 0.2rem 0.5rem;
    font-size: 0.75rem;
    font-weight: 600;
    text-transform: uppercase;
    letter-spacing: 0.05em;
    color: var(--text-muted);
  }
  .group button {
    display: flex;
    flex-direction: column;
    align-items: flex-start;
    gap: 0.1rem;
    width: 100%;
    min-width: 0;
    text-align: left;
    background: transparent;
    border-color: transparent;
  }
  .group button:hover {
    background: var(--button);
  }
  .file-name,
  .path {
    max-width: 100%;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  .file-name {
    font-weight: 600;
  }
  .path {
    font-size: 0.8rem;
    color: var(--text-muted);
  }
  .empty {
    padding: 0.3rem 0.5rem;
    font-size: 0.9rem;
    color: var(--text-muted);
  }
</style>
