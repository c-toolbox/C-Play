<script lang="ts">
  import { thumbnailUrl, type SlideInfo } from '../lib/api';

  interface Props {
    slides: SlideInfo[];
    current: number;
    next: number;
    viewing: number | null;
    /** Click: show the slide's notes. */
    onview: (index: number) => void;
  }

  let { slides, current, next, viewing, onview }: Props = $props();

  let strip: HTMLElement | undefined = $state();

  // Keep the shown slide in view.
  $effect(() => {
    strip?.querySelector(`[data-index="${current}"]`)?.scrollIntoView({ block: 'nearest', inline: 'center', behavior: 'smooth' });
  });
</script>

<nav class="strip" bind:this={strip} aria-label="Slides">
  {#each slides as slide (slide.index)}
    {@const src = thumbnailUrl(slide)}
    <button
      type="button"
      class="item"
      class:current={slide.index === current}
      class:next={slide.index === next}
      class:viewing={slide.index === viewing}
      data-index={slide.index}
      onclick={() => onview(slide.index)}
      title={slide.name}
    >
      <span class="thumb">
        {#if src}<img {src} alt="" loading="lazy" draggable="false" />{:else}<span class="number">{slide.index + 1}</span>{/if}
        {#if slide.notes.trim()}<span class="has-notes" title="Has notes"></span>{/if}
      </span>
      <span class="caption"><span class="index">{slide.index + 1}</span> {slide.name}</span>
    </button>
  {/each}
</nav>

<style>
  .strip {
    display: flex;
    gap: 0.6rem;
    overflow-x: auto;
    padding: 0.25rem 0.1rem 0.5rem;
    scrollbar-width: thin;
  }
  .item {
    flex: none;
    width: 9.5rem;
    display: flex;
    flex-direction: column;
    gap: 0.3rem;
    padding: 0;
    border: none;
    background: none;
    color: var(--text);
    text-align: left;
    cursor: pointer;
  }
  .thumb {
    position: relative;
    aspect-ratio: 16 / 9;
    border-radius: 6px;
    background: var(--frame);
    border: 2px solid var(--border);
    overflow: hidden;
    display: grid;
    place-items: center;
  }
  .item:hover .thumb {
    border-color: var(--text-muted);
  }
  .next .thumb {
    border-color: var(--next);
  }
  .current .thumb {
    border-color: var(--accent);
  }
  .viewing .thumb {
    outline: 2px dashed var(--text);
    outline-offset: 2px;
  }
  .item:focus-visible {
    outline: none;
  }
  .item:focus-visible .thumb {
    outline: 2px solid var(--text);
    outline-offset: 2px;
  }
  img {
    width: 100%;
    height: 100%;
    object-fit: contain;
  }
  .number {
    font-size: 1.4rem;
    color: var(--text-muted);
  }
  .has-notes {
    position: absolute;
    top: 0.3rem;
    right: 0.3rem;
    width: 0.55rem;
    height: 0.55rem;
    border-radius: 50%;
    background: var(--accent);
  }
  .caption {
    font-size: 0.8rem;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }
  .index {
    color: var(--text-muted);
    font-variant-numeric: tabular-nums;
  }
</style>
