<script lang="ts">
  import { itemEndModes, type MovieItem } from '../lib/api';
  import { formatTime } from '../lib/time';

  interface Props {
    items: MovieItem[];
    /** Item loaded in C-Play. */
    playing: number;
    /** Item of the prepared cue, null when live. */
    cued: number | null;
    /** Click: load it (live) or cue it. */
    onselect: (index: number) => void;
  }

  let { items, playing, cued, onselect }: Props = $props();

  let list: HTMLElement | undefined = $state();

  // Keep the cued, otherwise the playing item in view.
  $effect(() => {
    const index = cued ?? playing;
    list?.querySelector(`[data-index="${index}"]`)?.scrollIntoView({ block: 'nearest', behavior: 'smooth' });
  });
</script>

<section class="playlist">
  <header>
    <span class="label">Playlist</span>
    <span class="count">{items.length} {items.length === 1 ? 'item' : 'items'}</span>
  </header>
  {#if items.length}
    <ol bind:this={list}>
      {#each items as item (item.index)}
        <li>
          <button
            type="button"
            class="item"
            class:playing={item.index === playing}
            class:cued={item.index === cued}
            data-index={item.index}
            onclick={() => onselect(item.index)}
            title={item.file}
          >
            <span class="number">{item.index + 1}</span>
            <span class="text">
              <span class="title">{item.title}</span>
              <span class="meta">
                {item.duration > 0 ? formatTime(item.duration) : '--:--'}
                {#if item.sections.length}· {item.sections.length} {item.sections.length === 1 ? 'section' : 'sections'}{/if}
                · end: {itemEndModes[item.endMode] ?? 'Pause'}
              </span>
            </span>
            {#if item.index === playing}<span class="badge on-air">On air</span>{/if}
            {#if item.index === cued && item.index !== playing}<span class="badge cue">Cued</span>{/if}
          </button>
        </li>
      {/each}
    </ol>
  {:else}
    <p class="empty">The playlist is empty. Add files to the playlist in C-Play.</p>
  {/if}
</section>

<style>
  .playlist {
    display: flex;
    flex-direction: column;
    gap: 0.5rem;
    min-height: 0;
    min-width: 0;
  }
  header {
    display: flex;
    align-items: baseline;
    gap: 0.6rem;
  }
  .label {
    font-size: 0.75rem;
    font-weight: 700;
    letter-spacing: 0.08em;
    text-transform: uppercase;
    color: var(--text-muted);
  }
  .count {
    color: var(--text-muted);
    font-size: 0.85rem;
  }
  ol {
    list-style: none;
    margin: 0;
    padding: 0.15rem;
    display: flex;
    flex-direction: column;
    gap: 0.35rem;
    overflow-y: auto;
    min-height: 0;
    scrollbar-width: thin;
  }
  .item {
    width: 100%;
    display: flex;
    align-items: center;
    gap: 0.75rem;
    padding: 0.6rem 0.75rem;
    text-align: left;
    background: var(--surface);
    border: 1px solid var(--border);
    border-left: 4px solid var(--border);
    border-radius: var(--radius);
  }
  .item.playing {
    border-left-color: var(--accent);
  }
  .item.cued {
    border-color: var(--next);
    border-left-color: var(--next);
    box-shadow: 0 0 0 1px var(--next);
  }
  .number {
    flex: none;
    min-width: 1.6rem;
    font-variant-numeric: tabular-nums;
    color: var(--text-muted);
    text-align: right;
  }
  .text {
    display: flex;
    flex-direction: column;
    gap: 0.15rem;
    min-width: 0;
    flex: 1;
  }
  .title {
    font-weight: 600;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }
  .meta {
    font-size: 0.8rem;
    color: var(--text-muted);
    font-variant-numeric: tabular-nums;
  }
  .badge {
    flex: none;
    font-size: 0.72rem;
    font-weight: 700;
    letter-spacing: 0.04em;
    text-transform: uppercase;
    padding: 0.15rem 0.5rem;
    border-radius: 999px;
  }
  .on-air {
    background: var(--accent);
    color: var(--accent-text);
  }
  .cue {
    background: var(--next);
    color: #fff;
  }
  .empty {
    color: var(--text-muted);
  }
</style>
