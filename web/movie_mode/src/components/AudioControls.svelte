<script lang="ts">
  import type { AudioTrack } from '../lib/api';

  let { volume, tracks, selected, connected, syncFades, onvolume, onselect, onfadeout, onfadein }: {
    volume: number | null;
    tracks: AudioTrack[];
    selected: number;
    connected: boolean;
    /** The picture fades with the sound. */
    syncFades: boolean | null;
    onvolume: (level: number) => Promise<unknown>;
    onselect: (index: number) => Promise<unknown>;
    onfadeout: () => void;
    onfadein: () => void;
  } = $props();

  let draftVolume = $state<number | null>(null);
  let changingVolume = $state(false);
  let changingTrack = $state(false);
  let trackIndex = $state(-1);
  const level = $derived(draftVolume ?? volume ?? 0);

  $effect(() => {
    if (!changingTrack) trackIndex = tracks.find((track) => track.id === selected + 1)?.index ?? -1;
  });

  async function changeVolume() {
    changingVolume = true;
    try {
      await onvolume(level);
    } finally {
      draftVolume = null;
      changingVolume = false;
    }
  }

  async function changeTrack(index: number) {
    changingTrack = true;
    try {
      await onselect(index);
    } finally {
      changingTrack = false;
    }
  }
</script>

<div class="audio-controls" role="group" aria-label="On air audio">
  <div class="volume">
    <button type="button" class="fade" onclick={onfadeout} disabled={!connected || !volume} title={`Fade the volume out${syncFades ? ' with the picture' : ''}`}>Fade out</button>
    <label>
      <span>Volume</span>
      <input
        type="range"
        min="0"
        max="100"
        step="1"
        value={level}
        style:--level={`${level}%`}
        aria-label="On air volume"
        aria-valuetext={volume === null ? 'Unknown' : `${level}%`}
        disabled={!connected || volume === null || changingVolume}
        oninput={(event) => (draftVolume = Number(event.currentTarget.value))}
        onchange={changeVolume}
      />
      <output>{volume === null ? '—' : `${level}%`}</output>
    </label>
    <!-- As in C-Play: to the level it faded out from, then once more to 100. -->
    <button type="button" class="fade" onclick={onfadein} disabled={!connected || volume === null || volume >= 100} title={`Fade the volume in${syncFades ? ' with the picture' : ''}, to the level before the fade out, or else to 100`}>Fade in</button>
  </div>
  <label class="audio-file">
    <span>Audio file</span>
    <select
      bind:value={trackIndex}
      disabled={!connected || !tracks.length || changingTrack}
      onchange={(event) => changeTrack(Number(event.currentTarget.value))}
      title="Choose the audio track or file loaded with the on air movie"
    >
      <option value={-1} disabled>{tracks.length ? 'No audio selected' : 'No audio available'}</option>
      {#each tracks as track (track.index)}
        <option value={track.index}>{track.text || `Track ${track.index + 1}`}</option>
      {/each}
    </select>
  </label>
</div>

<style>
  .audio-controls {
    display: flex;
    flex-direction: column;
    gap: 0.6rem;
  }
  label {
    display: flex;
    align-items: center;
    gap: 0.5rem;
    min-width: 0;
    color: var(--text-muted);
    font-size: 0.9rem;
  }
  label > span {
    white-space: nowrap;
  }
  .volume {
    display: flex;
    align-items: center;
    gap: 0.6rem;
  }
  .volume label {
    flex: 1;
  }
  /* Phones: the volume on a row of its own, the fade buttons below it. */
  @media (max-width: 600px) {
    .volume {
      flex-wrap: wrap;
    }
    .volume label {
      order: -1;
      flex-basis: 100%;
    }
    .volume .fade {
      flex: 1;
    }
  }
  .fade {
    flex: none;
    padding: 0.6rem 0.8rem;
    font-weight: 600;
    white-space: nowrap;
  }
  input {
    flex: 1;
    min-width: 3rem;
    height: 2.4rem;
    margin: 0;
    background: transparent;
    appearance: none;
    cursor: pointer;
  }
  input:disabled {
    cursor: default;
    opacity: 0.6;
  }
  input::-webkit-slider-runnable-track {
    height: 1rem;
    border-radius: 999px;
    background: linear-gradient(to right, var(--accent) var(--level), var(--button) var(--level));
  }
  input::-moz-range-track {
    height: 1rem;
    border-radius: 999px;
    background: linear-gradient(to right, var(--accent) var(--level), var(--button) var(--level));
  }
  input::-webkit-slider-thumb {
    width: 1.3rem;
    height: 2.2rem;
    margin-top: -0.6rem;
    border: 2px solid var(--bg);
    border-radius: 6px;
    background: var(--text);
    appearance: none;
  }
  input::-moz-range-thumb {
    box-sizing: border-box;
    width: 1.3rem;
    height: 2.2rem;
    border: 2px solid var(--bg);
    border-radius: 6px;
    background: var(--text);
  }
  output {
    min-width: 3rem;
    text-align: right;
    font-variant-numeric: tabular-nums;
  }
  .audio-file {
    max-width: 100%;
  }
  select {
    flex: 1;
    min-width: 0;
    padding: 0.45rem 0.6rem;
    border: 1px solid var(--border);
    border-radius: 8px;
    background: var(--bg);
    color: var(--text);
    font: inherit;
  }
  select:focus-visible,
  input:focus-visible {
    outline: 2px solid var(--accent);
    outline-offset: 2px;
  }
</style>
