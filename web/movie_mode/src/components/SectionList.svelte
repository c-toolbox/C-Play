<script lang="ts">
  import { sectionEndModes, type SectionInfo } from '../lib/api';
  import { formatTime } from '../lib/time';

  interface Props {
    sections: SectionInfo[];
    /** Section playing in C-Play, -1 for none or another item. */
    playing: number;
    /** Section of the prepared cue, null when live or none is cued. */
    cued: number | null;
    onselect: (index: number) => void;
  }

  let { sections, playing, cued, onselect }: Props = $props();
</script>

{#if sections.length}
  <ol class="sections" aria-label="Sections">
    {#each sections as section (section.index)}
      <li>
        <button
          type="button"
          class="section"
          class:playing={section.index === playing}
          class:cued={section.index === cued}
          onclick={() => onselect(section.index)}
        >
          <span class="number">{section.index + 1}</span>
          <span class="title">{section.title || `Section ${section.index + 1}`}</span>
          <span class="times">{formatTime(section.start)} – {formatTime(section.end)}</span>
          <span class="mode" class:loop={section.endMode === 4}>{sectionEndModes[section.endMode] ?? 'Pause'}</span>
        </button>
      </li>
    {/each}
  </ol>
{:else}
  <p class="empty">No sections in this file. Add sections in C-Play, or pick a time on the timeline.</p>
{/if}

<style>
  .sections {
    list-style: none;
    margin: 0;
    padding: 0.15rem;
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(15rem, 1fr));
    gap: 0.4rem;
    overflow-y: auto;
    min-height: 0;
    scrollbar-width: thin;
  }
  .section {
    width: 100%;
    display: grid;
    grid-template-columns: auto minmax(0, 1fr) auto;
    grid-template-areas:
      'number title title'
      'number times mode';
    column-gap: 0.6rem;
    row-gap: 0.1rem;
    align-items: baseline;
    text-align: left;
    padding: 0.55rem 0.7rem;
    background: var(--surface);
    border-radius: var(--radius);
  }
  .section.playing {
    border-color: var(--accent);
    box-shadow: inset 4px 0 0 var(--accent);
  }
  .section.cued {
    border-color: var(--next);
    box-shadow: 0 0 0 1px var(--next);
  }
  .section.playing.cued {
    box-shadow:
      inset 4px 0 0 var(--accent),
      0 0 0 1px var(--next);
  }
  .number {
    grid-area: number;
    font-variant-numeric: tabular-nums;
    color: var(--text-muted);
  }
  .title {
    grid-area: title;
    font-weight: 600;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }
  .times {
    grid-area: times;
    font-size: 0.8rem;
    color: var(--text-muted);
    font-variant-numeric: tabular-nums;
  }
  .mode {
    grid-area: mode;
    font-size: 0.75rem;
    color: var(--text-muted);
  }
  .mode.loop {
    color: var(--warn);
  }
  .empty {
    margin: 0;
    color: var(--text-muted);
  }
</style>
