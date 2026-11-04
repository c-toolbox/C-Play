<script lang="ts">
  import { thumbnailUrl, type SlideInfo } from '../lib/api';

  interface Props {
    slides: SlideInfo[];
    current: number;
    next: number;
    viewing: number | null;
    /** Click: show the slide's notes. */
    onview: (index: number) => void;
    /** The skip toggle of a slide. */
    onskip: (index: number, skipped: boolean) => void;
  }

  let { slides, current, next, viewing, onview, onskip }: Props = $props();

  let strip: HTMLElement | undefined = $state();

  // Keep the shown slide in view.
  $effect(() => {
    strip?.querySelector(`[data-index="${current}"]`)?.scrollIntoView({ block: 'nearest', inline: 'center', behavior: 'smooth' });
  });
</script>

<nav class="strip" bind:this={strip} aria-label="Slides">
  {#each slides as slide (slide.index)}
    {@const src = thumbnailUrl(slide)}
    <div class="item" class:current={slide.index === current} class:next={slide.index === next} class:viewing={slide.index === viewing} class:skipped={slide.skipped} data-index={slide.index}>
      <button type="button" class="open" onclick={() => onview(slide.index)} title={slide.skipped ? `${slide.name} (skipped)` : slide.name}>
        <span class="thumb">
          {#if src}<img {src} alt="" loading="lazy" draggable="false" />{:else}<span class="number">{slide.index + 1}</span>{/if}
          {#if slide.notes.trim()}<span class="has-notes" title="Has notes"></span>{/if}
          {#if slide.skipped}<span class="skipped-label">Skipped</span>{/if}
        </span>
        <span class="caption"><span class="index">{slide.index + 1}</span> <span class="slide-name">{slide.name}</span></span>
      </button>
      <button
        type="button"
        class="skip"
        aria-pressed={slide.skipped}
        aria-label={slide.skipped ? `Include slide ${slide.index + 1}` : `Skip slide ${slide.index + 1}`}
        title={slide.skipped ? 'Skipped: next and previous pass over it. Click to include it again' : 'Skip this slide: next and previous pass over it'}
        onclick={() => onskip(slide.index, !slide.skipped)}
      >
        {slide.skipped ? 'Include' : 'Skip'}
      </button>
    </div>
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
    position: relative;
    flex: none;
    width: 9.5rem;
  }
  .open {
    width: 100%;
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
  .open:focus-visible {
    outline: none;
  }
  .open:focus-visible .thumb {
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

  /* Shown on hover or focus, always for skipped slides and on touch screens. */
  .skip {
    position: absolute;
    top: 0.3rem;
    left: 0.3rem;
    padding: 0.1rem 0.45rem;
    font-size: 0.7rem;
    border-radius: 999px;
    background: rgb(0 0 0 / 0.65);
    border-color: rgb(255 255 255 / 0.25);
    color: #eef1f5;
    opacity: 0;
    transition: opacity 0.12s;
  }
  .skip:hover:not(:disabled) {
    background: rgb(0 0 0 / 0.85);
  }
  .item:hover .skip,
  .skip:focus-visible,
  .skipped .skip {
    opacity: 1;
  }
  @media (hover: none) {
    .skip {
      opacity: 1;
    }
  }
  .skipped .skip {
    background: var(--warn);
    border-color: var(--warn);
    color: #1a1204;
  }
  .skipped img,
  .skipped .number {
    opacity: 0.3;
    filter: grayscale(1);
  }
  .skipped-label {
    position: absolute;
    inset: auto 0 0.35rem;
    text-align: center;
    font-size: 0.7rem;
    font-weight: 700;
    letter-spacing: 0.08em;
    text-transform: uppercase;
    color: var(--warn);
  }
  .skipped .slide-name {
    color: var(--text-muted);
    text-decoration: line-through;
  }
</style>
