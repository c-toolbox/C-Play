<script lang="ts">
  import type { Snippet } from 'svelte';
  import { thumbnailUrl, type SlideInfo } from '../lib/api';

  interface Props {
    label: string;
    slide: SlideInfo | undefined;
    /** Shown instead of a slide name when there is no slide. */
    empty: string;
    count: number;
    badge?: string;
    accent?: boolean;
    actions?: Snippet;
  }

  let { label, slide, empty, count, badge, accent = false, actions }: Props = $props();
  const src = $derived(thumbnailUrl(slide));
</script>

<section class="preview" class:accent>
  <header>
    <span class="label">{label}</span>
    {#if slide}
      <span class="position">{slide.index + 1} / {count}</span>
      <span class="name" title={slide.name}>{slide.name}</span>
    {:else}
      <span class="name muted">{empty}</span>
    {/if}
    {#if badge}<span class="badge">{badge}</span>{/if}
  </header>
  <div class="frame">
    {#if src}
      <img {src} alt={slide?.name ?? ''} draggable="false" />
    {:else}
      <div class="placeholder">
        {#if slide}
          <span class="placeholder-name">{slide.name}</span>
          <span class="muted">No thumbnail yet</span>
        {:else}
          <span class="muted">{empty}</span>
        {/if}
      </div>
    {/if}
    {#if actions}<div class="actions">{@render actions()}</div>{/if}
  </div>
</section>

<style>
  .preview {
    display: flex;
    flex-direction: column;
    min-height: 0;
    min-width: 0;
    gap: 0.5rem;
  }
  header {
    display: flex;
    align-items: baseline;
    gap: 0.6rem;
    min-width: 0;
  }
  .label {
    font-size: 0.75rem;
    font-weight: 700;
    letter-spacing: 0.08em;
    text-transform: uppercase;
    color: var(--text-muted);
  }
  .accent .label {
    color: var(--accent);
  }
  .position {
    font-variant-numeric: tabular-nums;
    color: var(--text-muted);
  }
  .name {
    font-weight: 600;
    font-size: 1.05rem;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  .badge {
    margin-left: auto;
    flex: none;
    font-size: 0.75rem;
    padding: 0.15rem 0.5rem;
    border-radius: 999px;
    background: var(--warn-bg);
    color: var(--warn);
  }
  .frame {
    position: relative;
    flex: 1;
    min-height: 0;
    border-radius: var(--radius);
    background: var(--frame);
    border: 1px solid var(--border);
    overflow: hidden;
    display: grid;
    place-items: center;
  }
  .accent .frame {
    border-color: var(--accent);
    box-shadow: 0 0 0 1px var(--accent);
  }
  img {
    width: 100%;
    height: 100%;
    object-fit: contain;
    user-select: none;
  }
  .placeholder {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 0.4rem;
    padding: 1rem;
    text-align: center;
    /* The frame stays dark in both themes. */
    color: #e6e9ef;
  }
  .placeholder .muted {
    color: #9aa1ad;
  }
  .placeholder-name {
    font-size: 1.4rem;
    font-weight: 600;
  }
  .muted {
    color: var(--text-muted);
  }
  .actions {
    position: absolute;
    right: 0.6rem;
    bottom: 0.6rem;
    display: flex;
    gap: 0.4rem;
  }
</style>
