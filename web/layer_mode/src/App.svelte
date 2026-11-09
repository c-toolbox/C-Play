<script lang="ts">
  import { onMount } from 'svelte';
  import LayerCanvas from './components/LayerCanvas.svelte';
  import LayerDetails from './components/LayerDetails.svelte';
  import LayerList from './components/LayerList.svelte';
  import SlideList from './components/SlideList.svelte';
  import ViewSwitcher from './components/ViewSwitcher.svelte';
  import { ApiError, api, type CanvasKind, type LayerValues } from './lib/api';
  import { LayerEdits } from './lib/edits.svelte';
  import { circularActive, clamp, dragValues, positionValues } from './lib/geometry';
  import { LiveLayers } from './lib/live.svelte';
  import { load, store } from './lib/storage';

  const live = new LiveLayers();
  const edits = new LayerEdits((error) => notify(failure('Could not change the layer', error), true));
  // Opened by C-Play in its layer view window, which closes when the page title says so.
  const inCPlay = new URLSearchParams(location.search).get('host') === 'cplay';
  const canvasNames: Record<CanvasKind, string> = {
    fisheye: 'Fisheye (dome)',
    flat: 'Flat',
    sphere: 'Sphere',
    circular: 'Circular screen',
  };

  let theme = $state<'dark' | 'light'>(load('theme', 'dark'));
  let message = $state<{ text: string; error: boolean } | null>(null);
  let messageTimer: ReturnType<typeof setTimeout> | undefined;
  let fullscreen = $state(false);
  // Slide picked in the list; null follows the slide selected in C-Play.
  let picked = $state<number | null>(null);
  // The selected layer, by id so it stays selected when the layers are reordered.
  let selectedId = $state<string | null>(null);

  const s = $derived(live.state);
  const slides = $derived(s?.slides ?? []);
  const slide = $derived(slides.find((sl) => sl.index === (picked ?? s?.selected)) ?? slides[0]);
  const layers = $derived(slide ? slide.layers.map((layer) => edits.apply(slide.index, layer)) : []);
  const selectedLayer = $derived(layers.find((layer) => layer.id === selectedId));
  const editable = $derived(!!selectedLayer && selectedLayer.gridMode === 1 && !selectedLayer.locked && live.connected);

  $effect(() => {
    document.documentElement.dataset.theme = theme;
  });

  onMount(() => {
    live.start();
    const onFullscreen = () => (fullscreen = !!document.fullscreenElement);
    document.addEventListener('fullscreenchange', onFullscreen);
    return () => {
      live.stop();
      document.removeEventListener('fullscreenchange', onFullscreen);
    };
  });

  function notify(text: string, error = false) {
    message = { text, error };
    clearTimeout(messageTimer);
    messageTimer = setTimeout(() => (message = null), error ? 6000 : 2500);
  }

  const failure = (text: string, error: unknown) => (error instanceof ApiError ? `${text}: ${error.message}` : text);

  async function run(action: () => Promise<unknown>, text: string) {
    try {
      await action();
      return true;
    } catch (error) {
      notify(failure(text, error), true);
      return false;
    } finally {
      void live.refresh();
    }
  }

  function pickSlide(index: number) {
    picked = index;
    if (!slides.find((sl) => sl.index === index)?.layers.some((layer) => layer.id === selectedId)) selectedId = null;
  }

  function stepSlide(delta: number) {
    const at = slides.findIndex((sl) => sl === slide);
    const next = slides[clamp(at + delta, 0, slides.length - 1)];
    if (next) pickSlide(next.index);
  }

  // Layers and duplicates by their key (see layerKey), which the components use.
  const layerAt = (key: number) => layers.find((layer) => layer.key === key);

  function selectLayer(key: number) {
    selectedId = layerAt(key)?.id ?? null;
  }

  function change(key: number, values: LayerValues) {
    const layer = layerAt(key);
    if (slide && layer) edits.set(slide.index, layer.row, layer.sub, values);
  }

  /** Resizes the selected layer, keeping its aspect ratio; sizes that follow the media are left. */
  function resize(factor: number) {
    const layer = selectedLayer;
    if (!layer || !editable || !s) return;
    const p = layer.plane;
    const values: LayerValues = {};
    if (p.aspectRatio !== 1) {
      if (circularActive(p, s.circular)) values.widthDegrees = clamp(p.widthDegrees * factor, 0.01, 360);
      else values.width = Math.max(1, Math.round(p.width * factor));
    }
    if (p.aspectRatio !== 2) values.height = Math.max(1, Math.round(p.height * factor));
    change(layer.key, values);
  }

  /** Moves the selected layer by steps: degrees on the dome and sphere, cm on flat and circular heights. */
  function nudge(dx: number, dy: number, large = false) {
    const layer = selectedLayer;
    if (!layer || !editable || !s) return;
    const kind = s.canvas;
    const scale = large ? 10 : 1;
    const [u, v] = dragValues(kind, layer.plane, s.circular);
    const stepU = kind === 'flat' ? 10 : 1;
    const stepV = kind === 'flat' || kind === 'circular' ? 10 : 1;
    change(layer.key, positionValues(kind, [u + dx * stepU * scale, v + dy * stepV * scale], layer.plane, s));
  }

  function setVisible(key: number, on: boolean) {
    const layer = layerAt(key);
    if (slide && layer) void run(() => api.fade(slide.index, layer.row, layer.sub, on), on ? `Could not fade ${layer.title} in` : `Could not fade ${layer.title} out`);
  }

  async function save() {
    if (await run(api.save, 'Could not save the presentation')) notify('Presentation saved');
  }

  // Qt WebView in C-Play has no window.close() signal, it watches the title instead.
  function closeInCPlay() {
    document.title = 'cplay:close';
    setTimeout(() => (document.title = 'C-Play Layers'), 500);
  }

  function toggleFullscreen() {
    if (document.fullscreenElement) void document.exitFullscreen();
    else void document.documentElement.requestFullscreen().catch(() => notify('Fullscreen is not available', true));
  }

  function toggleTheme() {
    theme = theme === 'dark' ? 'light' : 'dark';
    store('theme', theme);
  }

  function onkeydown(event: KeyboardEvent) {
    const target = event.target as HTMLElement | null;
    if (event.defaultPrevented || event.altKey || event.ctrlKey || event.metaKey) return;
    if (target?.closest('textarea, input, select, [contenteditable="true"]')) return;
    const arrows: Record<string, [number, number]> = { ArrowLeft: [-1, 0], ArrowRight: [1, 0], ArrowUp: [0, 1], ArrowDown: [0, -1] };
    if (event.key in arrows && editable) {
      event.preventDefault();
      nudge(...arrows[event.key], event.shiftKey);
    } else if (event.key === '+' || event.key === '=') {
      resize(1.05);
    } else if (event.key === '-' || event.key === '_') {
      resize(1 / 1.05);
    } else if (event.key === 'PageDown' || event.key === 'PageUp') {
      event.preventDefault();
      stepSlide(event.key === 'PageDown' ? 1 : -1);
    } else if (event.key === 'Escape') {
      selectedId = null;
    } else if (event.key === 'f' || event.key === 'F') {
      if (!inCPlay) toggleFullscreen();
    }
  }
</script>

<svelte:window {onkeydown} />

<div class="app">
  <header class="top">
    <div class="title">
      <ViewSwitcher current="layerview" />
      <span class="name" title={s?.name}>{s?.name || 'C-Play layers'}</span>
      {#if s?.needsSave}<span class="unsaved" title="The presentation has unsaved changes in C-Play">Unsaved</span>{/if}
    </div>

    <div class="tools">
      {#if s}
        <span class="canvas-name" title="Chosen in C-Play: Views > Layer view > Move layers on"><span class="prefix">Moving on: </span>{canvasNames[s.canvas]}</span>
      {/if}
      <span class="connection" class:online={live.connected} title={live.connected ? 'Connected to C-Play' : 'Not connected to C-Play'}>
        {live.connected ? 'Live' : 'Offline'}
      </span>
      <button type="button" onclick={save} disabled={!s?.canSave || !live.connected} title={s?.canSave ? 'Save the presentation file in C-Play' : 'Save the presentation in C-Play first'}>
        Save
      </button>
      <button type="button" class="ghost" onclick={toggleTheme} aria-label="Switch light or dark" title="Light or dark">◐</button>
      {#if inCPlay}
        <button type="button" class="ghost" onclick={closeInCPlay} aria-label="Close layer view" title="Close (Ctrl+W)">✕</button>
      {:else if document.fullscreenEnabled}
        <button type="button" class="ghost" onclick={toggleFullscreen} aria-label="Fullscreen" title="Fullscreen (F)">{fullscreen ? '⤡' : '⤢'}</button>
      {/if}
    </div>
  </header>

  {#if !s || !slide}
    <div class="connecting">
      <p>{!live.connected ? 'Connecting to C-Play…' : s ? 'No presentation in C-Play' : 'Waiting for C-Play…'}</p>
    </div>
  {:else}
    <main class="stage">
      <SlideList {slides} current={slide.index} selected={s.selected} triggered={s.triggered} onpick={pickSlide} />

      <div class="canvas-col">
        {#if s.canvas === 'circular' && !s.circular.enabled}
          <p class="notice">Circular surface mapping is off in the C-Play Grid settings: layers are placed by their horizontal position, and C-Play renders them flat.</p>
        {/if}
        <LayerCanvas view={s} {layers} selected={selectedLayer?.key ?? null} onselect={selectLayer} onmove={change} onresize={resize} />
      </div>

      <div class="side-col">
        <LayerDetails
          layer={selectedLayer}
          circular={s.circular}
          disabled={!live.connected}
          onchange={(values) => selectedLayer && change(selectedLayer.key, values)}
          onresize={resize}
          onnudge={(dx, dy) => nudge(dx, dy)}
        />
        <LayerList
          {layers}
          selected={selectedLayer?.key ?? null}
          disabled={!live.connected}
          onselect={selectLayer}
          onvolume={(key, volume) => change(key, { volume })}
          onvisible={setVisible}
        />
      </div>
    </main>
  {/if}

  {#if message}
    <div class="toast" class:error={message.error} role="status">{message.text}</div>
  {/if}
</div>

<style>
  .app {
    height: 100%;
    display: grid;
    grid-template-rows: auto minmax(0, 1fr);
    gap: var(--gap);
    padding: var(--gap);
    padding-left: max(var(--gap), env(safe-area-inset-left));
    padding-right: max(var(--gap), env(safe-area-inset-right));
  }

  .top {
    display: flex;
    align-items: center;
    gap: var(--gap);
    min-width: 0;
  }
  .title {
    display: flex;
    align-items: center;
    gap: 0.6rem;
    min-width: 0;
    flex: 1;
  }
  .name {
    font-weight: 600;
    font-size: 1.1rem;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }
  .unsaved {
    flex: none;
    font-size: 0.75rem;
    padding: 0.15rem 0.5rem;
    border-radius: 999px;
    background: var(--warn-bg);
    color: var(--warn);
  }
  .tools {
    display: flex;
    align-items: center;
    gap: 0.4rem;
    justify-content: flex-end;
    flex-wrap: wrap;
  }
  .canvas-name {
    font-size: 0.8rem;
    color: var(--text-muted);
    padding: 0.2rem 0.6rem;
    border: 1px solid var(--border);
    border-radius: 999px;
    margin-right: 0.4rem;
  }
  .connection {
    font-size: 0.8rem;
    color: var(--danger);
    margin-right: 0.4rem;
  }
  .connection::before {
    content: '';
    display: inline-block;
    width: 0.5rem;
    height: 0.5rem;
    border-radius: 50%;
    background: currentColor;
    margin-right: 0.35rem;
    vertical-align: 0.05rem;
  }
  .connection.online {
    color: var(--accent);
  }

  .stage {
    display: grid;
    grid-template-columns: minmax(12rem, 16rem) minmax(0, 1fr) minmax(19rem, 24rem);
    gap: var(--gap);
    min-height: 0;
  }
  .canvas-col,
  .side-col {
    display: flex;
    flex-direction: column;
    gap: var(--gap);
    min-height: 0;
    min-width: 0;
  }
  .canvas-col > :global(.canvas) {
    flex: 1;
  }
  .side-col > :global(.layers) {
    flex: 1;
  }
  .notice {
    margin: 0;
    padding: 0.5rem 0.8rem;
    border-radius: var(--radius);
    background: var(--warn-bg);
    color: var(--warn);
    font-size: 0.85rem;
  }

  .connecting {
    display: grid;
    place-items: center;
    color: var(--text-muted);
    font-size: 1.3rem;
  }

  .toast {
    position: fixed;
    left: 50%;
    bottom: 1.5rem;
    transform: translateX(-50%);
    max-width: min(40rem, calc(100% - 2rem));
    padding: 0.7rem 1.1rem;
    border-radius: var(--radius);
    background: var(--surface);
    border: 1px solid var(--accent);
    box-shadow: 0 8px 30px rgb(0 0 0 / 0.4);
  }
  .toast.error {
    border-color: var(--danger);
  }

  /* Tablets: slides above, canvas and layers below. */
  @media (max-width: 1100px) {
    .stage {
      grid-template-columns: minmax(0, 1fr) minmax(17rem, 22rem);
      grid-template-rows: minmax(0, 9rem) minmax(0, 1fr);
    }
    .stage > :global(.slides) {
      grid-column: 1 / -1;
    }
  }

  /* Phones (also in landscape) and portrait tablets: one column that scrolls. */
  @media (max-width: 760px), (orientation: portrait) and (max-width: 1100px), (max-height: 520px) {
    .app {
      height: auto;
      min-height: 100%;
      grid-template-rows: auto auto;
    }
    .top {
      flex-wrap: wrap;
    }
    .title {
      flex-basis: 100%;
    }
    .stage {
      grid-template-columns: minmax(0, 1fr);
      grid-template-rows: none;
    }
    .stage > :global(.slides) {
      max-height: 12rem;
    }
    .canvas-col > :global(.canvas) {
      flex: none;
      height: min(92vw, 70vh);
    }
    .side-col > :global(.layers) {
      max-height: none;
    }
  }

  /* Phones: a smaller header. */
  @media (max-width: 600px), (max-height: 520px) {
    .tools {
      flex-wrap: nowrap;
      gap: 0.15rem;
    }
    .canvas-name {
      margin-right: 0.2rem;
    }
    .canvas-name .prefix {
      display: none;
    }
    /* The dot alone; its title still says it. */
    .connection {
      font-size: 0;
      margin-right: 0.2rem;
    }
  }
</style>
