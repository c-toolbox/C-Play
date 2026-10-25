<script lang="ts">
  import type { SectionInfo } from '../lib/api';
  import { formatTime } from '../lib/time';

  interface Props {
    /** Seconds, 0 when not known. */
    duration: number;
    sections: SectionInfo[];
    /** Where C-Play plays, null when it plays another item. */
    playhead: number | null;
    /** Where the cue starts, null for none. */
    marker?: number | null;
    /** Section to highlight, -1 for none. */
    active: number;
    /** Colours of the on air item or of the cue. */
    variant: 'live' | 'cue';
    disabled?: boolean;
    /** Released at a position (seconds). */
    onseek?: (seconds: number) => void;
    /** Dragged over a position (seconds), before it is released. */
    onscrub?: (seconds: number) => void;
  }

  let { duration, sections, playhead, marker = null, active, variant, disabled = false, onseek, onscrub }: Props = $props();

  let bar: HTMLElement | undefined = $state();
  // Position under the pointer while dragging.
  let dragging = $state<number | null>(null);

  const length = $derived(Math.max(duration, ...sections.map((section) => section.end), 0));
  const percent = (seconds: number) => `${Math.min(100, Math.max(0, (seconds / length) * 100))}%`;

  function timeAt(event: PointerEvent) {
    const rect = bar!.getBoundingClientRect();
    return Math.min(length, Math.max(0, ((event.clientX - rect.left) / rect.width) * length));
  }

  function onpointerdown(event: PointerEvent) {
    if (disabled || !length || event.button !== 0) return;
    bar!.setPointerCapture(event.pointerId);
    dragging = timeAt(event);
    onscrub?.(dragging);
  }

  function onpointermove(event: PointerEvent) {
    if (dragging === null) return;
    dragging = timeAt(event);
    onscrub?.(dragging);
  }

  function onpointerup() {
    if (dragging === null) return;
    const seconds = dragging;
    dragging = null;
    onseek?.(seconds);
  }

  function onkeydown(event: KeyboardEvent) {
    if (disabled || !length) return;
    const from = marker ?? playhead ?? 0;
    const step = event.shiftKey ? 30 : 5;
    if (event.key === 'ArrowLeft' || event.key === 'ArrowRight') {
      // Leaves the arrow keys of the page (sections) alone.
      event.preventDefault();
      event.stopPropagation();
      onseek?.(Math.min(length, Math.max(0, from + (event.key === 'ArrowLeft' ? -step : step))));
    }
  }
</script>

<div class="timeline {variant}" class:disabled={disabled || !length}>
  <div
    class="bar"
    bind:this={bar}
    role="slider"
    tabindex={disabled || !length ? -1 : 0}
    aria-label={variant === 'live' ? 'On air position' : 'Cue position'}
    aria-valuemin={0}
    aria-valuemax={Math.round(length)}
    aria-valuenow={Math.round(dragging ?? marker ?? playhead ?? 0)}
    aria-valuetext={formatTime(dragging ?? marker ?? playhead ?? 0)}
    {onpointerdown}
    {onpointermove}
    {onpointerup}
    onpointercancel={() => (dragging = null)}
    {onkeydown}
  >
    {#if length}
      {#each sections as section (section.index)}
        <span
          class="section"
          class:active={section.index === active}
          style:left={percent(section.start)}
          style:width="calc({percent(section.end)} - {percent(section.start)})"
          title="{section.title} ({formatTime(section.start)}–{formatTime(section.end)})"
        >
          <span class="section-title">{section.title}</span>
        </span>
      {/each}
      {#if playhead !== null}
        <span class="progress" style:width={percent(playhead)}></span>
        <span class="head playhead" style:left={percent(playhead)}></span>
      {/if}
      {#if marker !== null}<span class="head marker" style:left={percent(marker)}></span>{/if}
      {#if dragging !== null}
        <span class="head drag" style:left={percent(dragging)}><span class="drag-time">{formatTime(dragging)}</span></span>
      {/if}
    {:else}
      <span class="unknown">Length known once C-Play has loaded the file</span>
    {/if}
  </div>
  <div class="scale">
    <span>00:00</span>
    <span>{length ? formatTime(length) : '--:--'}</span>
  </div>
</div>

<style>
  .timeline {
    display: flex;
    flex-direction: column;
    gap: 0.25rem;
    --tone: var(--accent);
  }
  .timeline.cue {
    --tone: var(--next);
  }
  .bar {
    position: relative;
    height: 3.25rem;
    border-radius: 8px;
    background: var(--frame);
    border: 1px solid var(--border);
    overflow: hidden;
    cursor: pointer;
    touch-action: none;
    user-select: none;
  }
  .disabled .bar {
    cursor: default;
  }
  .bar:focus-visible {
    outline: 2px solid var(--tone);
    outline-offset: 2px;
  }
  .section {
    position: absolute;
    top: 0.35rem;
    bottom: 0.35rem;
    border-radius: 5px;
    background: rgb(255 255 255 / 0.1);
    border: 1px solid rgb(255 255 255 / 0.18);
    overflow: hidden;
    display: flex;
    align-items: center;
    pointer-events: none;
  }
  .section.active {
    background: color-mix(in srgb, var(--tone) 35%, transparent);
    border-color: var(--tone);
  }
  .section-title {
    padding: 0 0.4rem;
    font-size: 0.75rem;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
    /* The bar stays dark in both themes. */
    color: #e6e9ef;
  }
  .progress {
    position: absolute;
    left: 0;
    top: 0;
    bottom: 0;
    background: color-mix(in srgb, var(--accent) 18%, transparent);
    pointer-events: none;
  }
  .head {
    position: absolute;
    top: 0;
    bottom: 0;
    width: 3px;
    margin-left: -1px;
    pointer-events: none;
  }
  .playhead {
    background: var(--accent);
  }
  .marker {
    background: var(--next);
    width: 4px;
  }
  .marker::before {
    content: '';
    position: absolute;
    top: 0;
    left: -5px;
    border: 7px solid transparent;
    border-top-color: var(--next);
  }
  .drag {
    background: #fff;
  }
  .drag-time {
    position: absolute;
    bottom: 0.2rem;
    left: 0.4rem;
    font-size: 0.75rem;
    font-variant-numeric: tabular-nums;
    padding: 0.05rem 0.3rem;
    border-radius: 4px;
    background: rgb(0 0 0 / 0.75);
    color: #fff;
    white-space: nowrap;
  }
  .unknown {
    position: absolute;
    inset: 0;
    display: grid;
    place-items: center;
    font-size: 0.85rem;
    color: #9aa1ad;
  }
  .scale {
    display: flex;
    justify-content: space-between;
    font-size: 0.75rem;
    color: var(--text-muted);
    font-variant-numeric: tabular-nums;
  }
</style>
