<script lang="ts">
  import type { CircularMapping, LayerInfo, LayerValues } from '../lib/api';
  import { circularActive } from '../lib/geometry';

  interface Props {
    layer: LayerInfo | undefined;
    circular: CircularMapping;
    disabled: boolean;
    onchange: (values: LayerValues) => void;
    onresize: (factor: number) => void;
    /** Moves the layer by steps of the canvas (arrow buttons), as the arrow keys do. */
    onnudge: (dx: number, dy: number) => void;
  }

  let { layer, circular, disabled, onchange, onresize, onnudge }: Props = $props();

  type Field = { key: keyof LayerValues; label: string; unit: string; step: number; min?: number; max?: number; off?: boolean };

  const plane = $derived(layer?.plane);
  const inDegrees = $derived(!!plane && circularActive(plane, circular));
  const sizeBasis = $derived(
    plane?.aspectRatio === 1 ? 'Height and media aspect ratio' : plane?.aspectRatio === 2 ? 'Width and media aspect ratio' : 'Width and height',
  );
  const fields = $derived<Field[]>(
    plane
      ? [
          { key: 'azimuth', label: 'Azimuth', unit: '°', step: 1 },
          { key: 'elevation', label: 'Elevation', unit: '°', step: 1 },
          { key: 'roll', label: 'Roll', unit: '°', step: 1 },
          { key: 'distance', label: 'Distance', unit: 'cm', step: 10, min: 0 },
          inDegrees
            ? { key: 'centreDegrees', label: 'Centre', unit: '°', step: 1 }
            : { key: 'horizontal', label: 'Horizontal', unit: 'cm', step: 10 },
          { key: 'vertical', label: 'Vertical', unit: 'cm', step: 10 },
          inDegrees
            ? { key: 'widthDegrees', label: 'Width', unit: '°', step: 1, min: 0.01, max: 360, off: plane.aspectRatio === 1 }
            : { key: 'width', label: 'Width', unit: 'cm', step: 10, min: 0, off: plane.aspectRatio === 1 },
          { key: 'height', label: 'Height', unit: 'cm', step: 10, min: 0, off: plane.aspectRatio === 2 },
        ]
      : [],
  );

  const shown = (key: keyof LayerValues) => {
    const value = plane?.[key as keyof typeof plane];
    return typeof value === 'number' ? Math.round(value * 100) / 100 : 0;
  };

  function commit(field: Field, input: HTMLInputElement) {
    const value = input.valueAsNumber;
    if (!Number.isFinite(value) || (field.min !== undefined && value < field.min) || (field.max !== undefined && value > field.max)) {
      input.value = String(shown(field.key));
      return;
    }
    onchange({ [field.key]: value });
  }
</script>

<section class="details" aria-label="Selected layer">
  {#if !layer}
    <p class="hint">Select a layer to place it. Flat layers are drawn on the canvas: drag to move, scroll (or the size buttons) to resize.</p>
  {:else if !plane || layer.gridMode !== 1}
    <h2>{layer.title}</h2>
    <p class="hint">Only flat layers (grid mode Plane) have a position and size. This layer has volume and visibility only.</p>
  {:else}
    <h2>{layer.title}</h2>
    <div class="fields">
      {#each fields as field (field.key)}
        <label class:off={field.off}>
          <span>{field.label}</span>
          <input
            type="number"
            step={field.step}
            min={field.min}
            max={field.max}
            value={shown(field.key)}
            disabled={disabled || layer.locked || field.off}
            title={field.off ? `Follows the media aspect ratio (size based on: ${sizeBasis})` : undefined}
            onchange={(event) => commit(field, event.currentTarget)}
            onkeydown={(event) => event.key === 'Enter' && event.currentTarget.blur()}
          />
          <span class="unit">{field.unit}</span>
        </label>
      {/each}
    </div>
    <div class="actions">
      <span class="basis">Size: {sizeBasis}</span>
      <span class="buttons">
        <button type="button" onclick={() => onnudge(-1, 0)} disabled={disabled || layer.locked} aria-label="Move left" title="Move left (←)">←</button>
        <button type="button" onclick={() => onnudge(0, 1)} disabled={disabled || layer.locked} aria-label="Move up" title="Move up (↑)">↑</button>
        <button type="button" onclick={() => onnudge(0, -1)} disabled={disabled || layer.locked} aria-label="Move down" title="Move down (↓)">↓</button>
        <button type="button" onclick={() => onnudge(1, 0)} disabled={disabled || layer.locked} aria-label="Move right" title="Move right (→)">→</button>
        <button type="button" onclick={() => onresize(1 / 1.1)} disabled={disabled || layer.locked} title="Smaller (−)">Smaller</button>
        <button type="button" onclick={() => onresize(1.1)} disabled={disabled || layer.locked} title="Larger (+)">Larger</button>
      </span>
    </div>
  {/if}
</section>

<style>
  .details {
    padding: 0.7rem 0.8rem;
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: var(--radius);
  }
  h2 {
    margin: 0 0 0.6rem;
    font-size: 1rem;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }
  .hint {
    margin: 0;
    color: var(--text-muted);
    font-size: 0.9rem;
  }
  .fields {
    display: grid;
    grid-template-columns: repeat(2, minmax(0, 1fr));
    gap: 0.45rem 0.8rem;
  }
  label {
    display: grid;
    grid-template-columns: 5rem minmax(0, 1fr) 1.6rem;
    align-items: center;
    gap: 0.4rem;
    font-size: 0.85rem;
  }
  label.off span {
    color: var(--text-muted);
  }
  input {
    font: inherit;
    color: var(--text);
    background: var(--bg);
    border: 1px solid var(--border);
    border-radius: 6px;
    padding: 0.3rem 0.4rem;
    min-width: 0;
    font-variant-numeric: tabular-nums;
  }
  input:focus-visible {
    outline: 2px solid var(--accent);
    outline-offset: 1px;
  }
  input:disabled {
    opacity: 0.55;
  }
  .unit {
    color: var(--text-muted);
  }
  .actions {
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    justify-content: space-between;
    gap: 0.5rem;
    margin-top: 0.7rem;
  }
  .basis {
    font-size: 0.8rem;
    color: var(--text-muted);
  }
  .buttons {
    display: flex;
    flex-wrap: wrap;
    gap: 0.3rem;
  }
  .buttons button {
    padding: 0.3rem 0.6rem;
  }
</style>
