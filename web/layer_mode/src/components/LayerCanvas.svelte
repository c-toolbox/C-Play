<script lang="ts">
  import type { LayerInfo, LayerValues, LayerViewState } from '../lib/api';
  import {
    circularPoint,
    circularRadius,
    dragValues,
    layerShape,
    positionValues,
    ringInner,
    ringOuter,
    valuesAt,
    viewBox,
    wrap180,
    wrapsAround,
    type Point,
  } from '../lib/geometry';

  interface Props {
    view: LayerViewState;
    layers: LayerInfo[];
    selected: number | null;
    onselect: (row: number) => void;
    onmove: (row: number, values: LayerValues) => void;
    /** Resizes the selected layer by factor (wheel). */
    onresize: (factor: number) => void;
  }

  let { view, layers, selected, onselect, onmove, onresize }: Props = $props();

  let svg = $state<SVGSVGElement>();

  const kind = $derived(view.canvas);
  const box = $derived(viewBox(kind, view));
  // Text and marks are sized in the units of the canvas (degrees, cm or the unit disc).
  const unit = $derived(box[2] / 100);
  // Flat layers, bottom of the render order first so the top one is drawn last.
  const planes = $derived(
    layers
      .filter((layer) => layer.gridMode === 1)
      .reverse()
      .map((layer) => ({ layer, ...layerShape(kind, layer.plane, view) })),
  );

  // ---- Background ----------------------------------------------------------------------------------

  const halfFov = $derived(view.dome.fov / 2);
  const fisheyeRings = $derived([60, 30, 0].map((elevation) => ({ elevation, r: (90 - elevation) / halfFov })).filter((ring) => ring.r <= 1.001));
  const spokes = Array.from({ length: 8 }, (_, i) => i * 45);
  const sphereMeridians = Array.from({ length: 11 }, (_, i) => -150 + i * 30);
  const sphereParallels = [-60, -30, 0, 30, 60];
  const flatStep = $derived(niceStep(view.flat.horizontalRange / 4));
  const flatLines = $derived({
    x: steps(view.flat.horizontalRange, flatStep),
    y: steps(view.flat.verticalRange, flatStep),
  });
  const ticks = Array.from({ length: 12 }, (_, i) => i * 30);

  function niceStep(value: number) {
    const power = 10 ** Math.floor(Math.log10(Math.max(value, 1)));
    return [1, 2, 5, 10].map((m) => m * power).find((step) => step >= value) ?? power * 10;
  }

  // Names are left out for layers centred off the canvas, which would only show clipped.
  function onCanvas([x, y]: Point) {
    if (!Number.isFinite(x) || !Number.isFinite(y)) return false;
    if (kind === 'fisheye') return Math.hypot(x, y) <= 1;
    return x >= box[0] && x <= box[0] + box[2] && y >= box[1] && y <= box[1] + box[3];
  }

  // Names near the left or right edge run inwards, so they are not cut off.
  function anchorAt(x: number) {
    const at = (x - box[0]) / box[2];
    return at < 0.15 ? 'start' : at > 0.85 ? 'end' : 'middle';
  }

  function steps(range: number, step: number) {
    const values: number[] = [];
    for (let v = -Math.floor(range / step) * step; v <= range; v += step) values.push(v);
    return values;
  }

  // ---- Dragging ------------------------------------------------------------------------------------

  let drag: { row: number; pointer: number; start: Point; from: Point; last: Point; u: number } | null = null;

  function canvasPoint(event: PointerEvent | WheelEvent): Point | null {
    const matrix = svg?.getScreenCTM();
    if (!svg || !matrix) return null;
    const point = new DOMPoint(event.clientX, event.clientY).matrixTransform(matrix.inverse());
    return [point.x, point.y];
  }

  // Touch: two fingers, the first on a layer, resize it by pinching.
  const touches = new Map<number, [number, number]>();
  let pinchDistance = 0;

  function spread() {
    const [a, b] = [...touches.values()];
    return Math.hypot(a[0] - b[0], a[1] - b[1]);
  }

  function addTouch(event: PointerEvent) {
    if (event.pointerType !== 'touch' || !drag) return;
    touches.set(event.pointerId, [event.clientX, event.clientY]);
    svg?.setPointerCapture(event.pointerId);
    if (touches.size === 2) {
      // The second finger: a pinch instead of the drag.
      drag = null;
      pinchDistance = spread();
    }
  }

  function startDrag(event: PointerEvent, layer: LayerInfo) {
    // A second finger, also on a layer, pinches the one being dragged (addTouch).
    if (event.button !== 0 || (event.pointerType === 'touch' && drag)) return;
    event.preventDefault();
    onselect(layer.row);
    const point = canvasPoint(event);
    if (event.pointerType === 'touch') {
      touches.clear();
      touches.set(event.pointerId, [event.clientX, event.clientY]);
    }
    if (!point || layer.locked) return;
    const from = valuesAt(kind, point, view);
    drag = { row: layer.row, pointer: event.pointerId, start: dragValues(kind, layer.plane, view.circular), from, last: from, u: from[0] };
    svg?.setPointerCapture(event.pointerId);
  }

  function moveDrag(event: PointerEvent) {
    if (touches.has(event.pointerId)) {
      touches.set(event.pointerId, [event.clientX, event.clientY]);
      if (touches.size === 2 && pinchDistance > 0) {
        const distance = spread();
        if (distance > 0) onresize(distance / pinchDistance);
        pinchDistance = distance;
        return;
      }
    }
    if (!drag || event.pointerId !== drag.pointer) return;
    const point = canvasPoint(event);
    const layer = layers.find((l) => l.row === drag?.row);
    if (!point || !layer) return;
    const at = valuesAt(kind, point, view);
    // Angles that go around follow the pointer past +-180 instead of jumping back.
    drag.u = wrapsAround(kind) ? drag.u + wrap180(at[0] - drag.last[0]) : at[0];
    drag.last = at;
    const target: Point = [drag.start[0] + drag.u - drag.from[0], drag.start[1] + at[1] - drag.from[1]];
    onmove(drag.row, positionValues(kind, target, layer.plane, view));
  }

  function endDrag(event: PointerEvent) {
    if (drag && event.pointerId === drag.pointer) drag = null;
    touches.delete(event.pointerId);
    if (touches.size < 2) pinchDistance = 0;
  }

  // Not a passive listener, so the page does not scroll while resizing.
  $effect(() => {
    const element = svg;
    if (!element) return;
    const onwheel = (event: WheelEvent) => {
      if (selected === null || !layers.some((l) => l.row === selected && l.gridMode === 1 && !l.locked)) return;
      event.preventDefault();
      onresize(Math.exp(-event.deltaY * (event.deltaMode === 1 ? 0.05 : 0.0015)));
    };
    element.addEventListener('wheel', onwheel, { passive: false });
    return () => element.removeEventListener('wheel', onwheel);
  });
</script>

<div class="canvas" class:wide={kind === 'sphere'}>
  <svg
    bind:this={svg}
    viewBox={box.join(' ')}
    preserveAspectRatio="xMidYMid meet"
    role="application"
    aria-label="Layer positions"
    onpointerdown={addTouch}
    onpointermove={moveDrag}
    onpointerup={endDrag}
    onpointercancel={endDrag}
  >
    <defs>
      <clipPath id="dome-clip"><circle r="1" /></clipPath>
      <clipPath id="map-clip"><rect x={box[0]} y={box[1]} width={box[2]} height={box[3]} /></clipPath>
    </defs>

    <g class="grid">
      {#if kind === 'fisheye'}
        <circle class="surface" r="1" />
        {#each fisheyeRings as ring (ring.elevation)}
          <circle r={ring.r} class:horizon={ring.elevation === 0} />
          <text class="label" x={0.02} y={ring.r - 0.02} font-size={unit * 2.2}>{ring.elevation}°</text>
        {/each}
        {#each spokes as angle (angle)}
          <line x1="0" y1="0" x2={Math.sin((angle * Math.PI) / 180)} y2={Math.cos((angle * Math.PI) / 180)} />
        {/each}
        <text class="direction" x="0" y="1.055" font-size={unit * 2.6}>Front</text>
        <text class="direction" x="0" y="-1.025" font-size={unit * 2.6}>Back</text>
        <text class="direction" x="-1.04" y="0" font-size={unit * 2.6} transform="rotate(-90 -1.04 0)">Left</text>
        <text class="direction" x="1.04" y="0" font-size={unit * 2.6} transform="rotate(90 1.04 0)">Right</text>
      {:else if kind === 'sphere'}
        <rect class="surface" x="-180" y="-90" width="360" height="180" />
        {#each sphereMeridians as azimuth (azimuth)}
          <line x1={azimuth} y1="-90" x2={azimuth} y2="90" class:horizon={azimuth === 0} />
        {/each}
        {#each sphereParallels as elevation (elevation)}
          <line x1="-180" y1={-elevation} x2="180" y2={-elevation} class:horizon={elevation === 0} />
        {/each}
        {#each [[-180, 'Back'], [-90, 'Left'], [0, 'Front'], [90, 'Right']] as [azimuth, name] (azimuth)}
          <text class="label" x={Number(azimuth) + 2} y="86" font-size={unit * 2.4}>{name}</text>
        {/each}
        <text class="label" x="-178" y="-2" font-size={unit * 2.4}>Horizon</text>
      {:else if kind === 'flat'}
        <rect class="surface" x={box[0]} y={box[1]} width={box[2]} height={box[3]} />
        {#each flatLines.x as x (x)}
          <line x1={x} y1={box[1]} x2={x} y2={box[1] + box[3]} class:horizon={x === 0} />
        {/each}
        {#each flatLines.y as y (y)}
          <line x1={box[0]} y1={y} x2={box[0] + box[2]} y2={y} class:horizon={y === 0} />
        {/each}
        <text class="label" x={box[0] + unit} y={box[1] + unit * 3} font-size={unit * 2.4}>Grid {flatStep} cm</text>
      {:else}
        <path
          class="surface"
          fill-rule="evenodd"
          d={`M 0 -${ringOuter} A ${ringOuter} ${ringOuter} 0 1 1 0 ${ringOuter} A ${ringOuter} ${ringOuter} 0 1 1 0 -${ringOuter} Z M 0 -${ringInner} A ${ringInner} ${ringInner} 0 1 1 0 ${ringInner} A ${ringInner} ${ringInner} 0 1 1 0 -${ringInner} Z`}
        />
        <circle r={circularRadius(0, view.circular)} class="horizon" />
        {#each ticks as degrees (degrees)}
          {@const inner = circularPoint(degrees, -view.circular.heightCm / 2, view.circular)}
          {@const outer = circularPoint(degrees, view.circular.heightCm / 2, view.circular)}
          {@const label = circularPoint(degrees, -view.circular.heightCm * 0.62, view.circular)}
          <line x1={inner[0]} y1={inner[1]} x2={outer[0]} y2={outer[1]} />
          <text class="label centred" x={label[0]} y={label[1]} font-size={unit * 2.2}>{degrees}°</text>
        {/each}
        {@const seamInner = circularPoint(view.circular.seamDegrees, -view.circular.heightCm / 2, view.circular)}
        {@const seamOuter = circularPoint(view.circular.seamDegrees, view.circular.heightCm / 2, view.circular)}
        <line class="seam" x1={seamInner[0]} y1={seamInner[1]} x2={seamOuter[0]} y2={seamOuter[1]} />
        <text class="direction" x="0" y="0" font-size={unit * 2.6}>Circular screen</text>
        <text class="label centred" x="0" y={unit * 4} font-size={unit * 2}>{Math.round(view.circular.widthCm)} × {Math.round(view.circular.heightCm)} cm</text>
      {/if}
    </g>

    <g clip-path={kind === 'fisheye' ? 'url(#dome-clip)' : 'url(#map-clip)'}>
      {#each planes as { layer, path } (layer.row)}
        <path
          class="layer"
          class:selected={layer.row === selected}
          class:hidden={layer.visibility <= 0}
          class:locked={layer.locked}
          fill-rule="evenodd"
          d={path}
          role="button"
          tabindex="-1"
          aria-label={layer.title}
          onpointerdown={(event) => startDrag(event, layer)}
        />
      {/each}
      {#each planes as { layer, centre } (layer.row)}
        {#if onCanvas(centre)}
          <text class="name" class:selected={layer.row === selected} x={centre[0]} y={centre[1]} font-size={unit * 2.6} stroke-width={unit * 0.6} style:text-anchor={anchorAt(centre[0])}>{layer.title}</text>
        {/if}
      {/each}
    </g>
  </svg>
</div>

<style>
  .canvas {
    position: relative;
    min-height: 0;
    min-width: 0;
    display: grid;
    background: var(--frame);
    border: 1px solid var(--border);
    border-radius: var(--radius);
    overflow: hidden;
  }
  svg {
    width: 100%;
    height: 100%;
    display: block;
    /* A finger off the layers scrolls the page; on a layer it drags or pinches it. */
    touch-action: pan-y;
    user-select: none;
  }
  .grid :is(circle, line, path, rect) {
    fill: none;
    stroke: var(--grid);
    stroke-width: 1;
    vector-effect: non-scaling-stroke;
  }
  .grid .surface {
    fill: var(--surface-canvas);
    stroke: var(--grid-strong);
  }
  .grid .horizon {
    stroke: var(--grid-strong);
    stroke-width: 1.5;
  }
  .grid .seam {
    stroke: var(--danger);
    stroke-width: 2;
  }
  text {
    fill: var(--text-muted);
    pointer-events: none;
    dominant-baseline: middle;
  }
  .direction,
  .centred {
    text-anchor: middle;
  }
  .direction {
    font-weight: 600;
    letter-spacing: 0.04em;
  }
  .layer {
    touch-action: none;
    fill: var(--layer-fill);
    stroke: var(--layer-stroke);
    stroke-width: 1.5;
    vector-effect: non-scaling-stroke;
    cursor: grab;
  }
  .layer:hover {
    fill: var(--layer-hover);
  }
  .layer.hidden {
    stroke-dasharray: 5 4;
    fill: transparent;
  }
  .layer.selected {
    fill: var(--accent-fill);
    stroke: var(--accent);
    stroke-width: 2.5;
  }
  .layer.locked {
    cursor: not-allowed;
  }
  .layer:active {
    cursor: grabbing;
  }
  .name {
    fill: var(--text);
    font-weight: 600;
    paint-order: stroke;
    stroke: var(--frame);
    stroke-linejoin: round;
  }
  .name.selected {
    fill: var(--accent);
  }
</style>
