<script lang="ts">
  import type { SlideInfo } from '../lib/api';

  interface Props {
    slides: SlideInfo[];
    /** The slide the view edits. */
    current: number;
    /** Selected in the C-Play slide list. */
    selected: number;
    /** Shown on the output. */
    triggered: number;
    onpick: (index: number) => void;
  }

  let { slides, current, selected, triggered, onpick }: Props = $props();

  let list = $state<HTMLElement>();

  // Keeps the edited slide in view, also when it changes with the keyboard.
  $effect(() => {
    list?.querySelector<HTMLElement>(`[data-index="${current}"]`)?.scrollIntoView({ block: 'nearest' });
  });
</script>

<nav class="slides" bind:this={list} aria-label="Slides">
  {#each slides as slide (slide.index)}
    <button
      type="button"
      class="slide"
      class:current={slide.index === current}
      class:skipped={slide.skipped}
      data-index={slide.index}
      aria-current={slide.index === current ? 'true' : undefined}
      onclick={() => onpick(slide.index)}
    >
      <span class="number">{slide.index < 0 ? 'M' : slide.index + 1}</span>
      <span class="name">{slide.index < 0 ? 'Master' : slide.name || `Slide ${slide.index + 1}`}</span>
      <span class="marks">
        {#if slide.index === triggered}<span class="mark live" title="Shown on the output">On air</span>{/if}
        {#if slide.index === selected}<span class="mark" title="Selected in C-Play">C-Play</span>{/if}
        {#if slide.skipped}<span class="mark warn" title="Next and previous pass over it">Skipped</span>{/if}
        <span class="count">{slide.layers.length} {slide.layers.length === 1 ? 'layer' : 'layers'}</span>
      </span>
    </button>
  {/each}
</nav>

<style>
  .slides {
    display: flex;
    flex-direction: column;
    gap: 0.25rem;
    overflow-y: auto;
    min-height: 0;
    padding: 0.35rem;
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: var(--radius);
  }
  .slide {
    display: grid;
    grid-template-columns: 2rem minmax(0, 1fr);
    grid-template-rows: auto auto;
    column-gap: 0.4rem;
    align-items: center;
    text-align: left;
    padding: 0.45rem 0.55rem;
    background: transparent;
    border-color: transparent;
  }
  .slide.current {
    background: var(--button);
    border-color: var(--accent);
  }
  .slide.skipped .name {
    color: var(--text-muted);
  }
  .number {
    grid-row: span 2;
    font-variant-numeric: tabular-nums;
    color: var(--text-muted);
    font-weight: 600;
    text-align: center;
  }
  .name {
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
    font-weight: 500;
  }
  .marks {
    display: flex;
    gap: 0.3rem;
    align-items: center;
    flex-wrap: wrap;
    font-size: 0.7rem;
  }
  .mark {
    padding: 0.05rem 0.4rem;
    border-radius: 999px;
    background: var(--button-hover);
    color: var(--text-muted);
  }
  .mark.live {
    background: var(--accent);
    color: var(--accent-text);
    font-weight: 600;
  }
  .mark.warn {
    background: var(--warn-bg);
    color: var(--warn);
  }
  .count {
    margin-left: auto;
    color: var(--text-muted);
  }
</style>
