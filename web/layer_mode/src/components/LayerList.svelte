<script lang="ts">
  import type { LayerInfo } from '../lib/api';

  interface Props {
    layers: LayerInfo[];
    selected: number | null;
    disabled: boolean;
    /** By layer key (see layerKey). */
    onselect: (key: number) => void;
    onvolume: (key: number, volume: number) => void;
    onvisible: (key: number, on: boolean) => void;
  }

  let { layers, selected, disabled, onselect, onvolume, onvisible }: Props = $props();

  const gridNames = ['2D', 'Flat', 'Dome', 'Sphere', 'Sphere EAC'];
</script>

<ul class="layers" aria-label="Layers, top first">
  {#each layers as layer (layer.key)}
    {@const on = layer.visibility > 0}
    <li class="layer" class:selected={layer.key === selected}>
      <button type="button" class="pick" onclick={() => onselect(layer.key)} aria-pressed={layer.key === selected}>
        <span class="title">{layer.title}</span>
        <span class="tags">
          <span class="tag" class:flat={layer.gridMode === 1}>{gridNames[layer.gridMode] ?? 'Grid'}</span>
          <span class="tag">{layer.type}</span>
          {#if layer.locked}<span class="tag warn" title="Locked layer">Locked</span>{/if}
          {#if layer.source}<span class="tag" title="Kept visible from an earlier slide">From {layer.source}</span>{/if}
        </span>
      </button>
      <div class="controls">
        <button
          type="button"
          class="toggle"
          class:on
          onclick={() => onvisible(layer.key, !on)}
          {disabled}
          aria-pressed={on}
          title={on ? 'Fade the layer out' : 'Fade the layer in'}
          style:--level={`${layer.visibility}%`}
        >
          {on ? (layer.visibility < 100 ? `${layer.visibility}%` : 'On') : 'Off'}
        </button>
        {#if layer.hasAudio}
          <label class="volume" title="Layer volume">
            <span class="icon" aria-hidden="true">🔊</span>
            <input
              type="range"
              min="0"
              max="100"
              step="1"
              value={layer.volume}
              {disabled}
              aria-label={`Volume of ${layer.title}`}
              oninput={(event) => onvolume(layer.key, Number(event.currentTarget.value))}
            />
            <span class="value">{layer.volume}</span>
          </label>
        {/if}
      </div>
    </li>
  {:else}
    <li class="empty">No layers in this slide</li>
  {/each}
</ul>

<style>
  .layers {
    list-style: none;
    margin: 0;
    padding: 0.35rem;
    display: flex;
    flex-direction: column;
    gap: 0.3rem;
    overflow-y: auto;
    min-height: 0;
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: var(--radius);
  }
  .layer {
    display: flex;
    flex-direction: column;
    gap: 0.35rem;
    padding: 0.4rem 0.5rem;
    border: 1px solid transparent;
    border-radius: 8px;
  }
  .layer.selected {
    border-color: var(--accent);
    background: var(--button);
  }
  .pick {
    display: flex;
    flex-direction: column;
    align-items: flex-start;
    gap: 0.2rem;
    padding: 0;
    background: transparent;
    border: none;
    text-align: left;
    min-width: 0;
  }
  .pick:hover:not(:disabled) {
    background: transparent;
  }
  .title {
    font-weight: 600;
    max-width: 100%;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }
  .tags {
    display: flex;
    flex-wrap: wrap;
    gap: 0.25rem;
    font-size: 0.7rem;
  }
  .tag {
    padding: 0.05rem 0.4rem;
    border-radius: 999px;
    background: var(--button-hover);
    color: var(--text-muted);
  }
  .tag.flat {
    color: var(--accent);
  }
  .tag.warn {
    background: var(--warn-bg);
    color: var(--warn);
  }
  .controls {
    display: flex;
    align-items: center;
    gap: 0.6rem;
  }
  .toggle {
    flex: none;
    min-width: 4rem;
    padding: 0.3rem 0.6rem;
    font-size: 0.85rem;
    font-weight: 600;
    color: var(--text-muted);
    background: linear-gradient(to right, var(--accent-fill) var(--level), var(--button) var(--level));
  }
  .toggle.on {
    color: var(--text);
    border-color: var(--accent);
  }
  .volume {
    display: flex;
    align-items: center;
    gap: 0.4rem;
    flex: 1;
    min-width: 0;
    font-size: 0.85rem;
  }
  .volume input {
    flex: 1;
    min-width: 0;
    accent-color: var(--accent);
  }
  .value {
    width: 2rem;
    text-align: right;
    font-variant-numeric: tabular-nums;
    color: var(--text-muted);
  }
  .empty {
    padding: 1rem;
    color: var(--text-muted);
    text-align: center;
  }
</style>
