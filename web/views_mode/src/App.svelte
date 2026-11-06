<script lang="ts">
  import { onMount } from 'svelte';
  import ViewSwitcher, { viewHref, webViews, type ViewPath } from './components/ViewSwitcher.svelte';
  import { LiveStatus } from './lib/live.svelte';
  import { load, store } from './lib/storage';

  const live = new LiveStatus();
  // Opened by C-Play in its web view window, which closes when the page title says so.
  const inCPlay = new URLSearchParams(location.search).get('host') === 'cplay';

  let theme = $state<'dark' | 'light'>(load('theme', 'dark'));
  let fullscreen = $state(false);

  const slideCount = $derived(live.slides.filter((slide) => slide.index >= 0).length);
  const slideCountText = $derived(`${slideCount} ${slideCount === 1 ? 'slide' : 'slides'}`);

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

  function slideName(index: number) {
    const slide = live.slides.find((s) => s.index === index);
    if (index < 0) return 'Master';
    return slide?.name || `Slide ${index + 1}`;
  }

  /** What C-Play shows now, for the card of a view. */
  function status(path: ViewPath) {
    if (!live.connected || !live.details) return '';
    if (path === 'presenter') return slideCount ? `${live.triggered >= 0 ? `On air: ${slideName(live.triggered)}` : 'No slide on air'} · ${slideCountText}` : 'No presentation loaded';
    if (path === 'movie') return live.mediaTitle ? `${live.paused ? 'Paused' : 'Playing'}: ${live.mediaTitle}` : 'Nothing loaded';
    if (path === 'layerview') return slideCount ? `Selected in C-Play: ${slideName(live.selected)} · ${slideCountText}` : 'No presentation loaded';
    return '';
  }

  // Qt WebView in C-Play has no window.close() signal, it watches the title instead.
  function closeInCPlay() {
    document.title = 'cplay:close';
    setTimeout(() => (document.title = 'C-Play'), 500);
  }

  function toggleFullscreen() {
    if (document.fullscreenElement) void document.exitFullscreen();
    else void document.documentElement.requestFullscreen().catch(() => {});
  }

  function toggleTheme() {
    theme = theme === 'dark' ? 'light' : 'dark';
    store('theme', theme);
  }

  function onkeydown(event: KeyboardEvent) {
    if (event.defaultPrevented || event.altKey || event.ctrlKey || event.metaKey) return;
    const view = webViews[Number(event.key) - 1];
    if (view) location.href = viewHref(view.path);
    else if ((event.key === 'f' || event.key === 'F') && !inCPlay) toggleFullscreen();
  }
</script>

<svelte:window {onkeydown} />

<div class="app">
  <header class="top">
    <div class="title">
      <ViewSwitcher current="views" />
      <span class="name">C-Play</span>
    </div>

    <div class="tools">
      <span class="connection" class:online={live.connected} title={live.connected ? 'Connected to C-Play' : 'Not connected to C-Play'}>
        {live.connected ? 'Live' : 'Offline'}
      </span>
      <button type="button" class="ghost" onclick={toggleTheme} aria-label="Switch light or dark" title="Light or dark">◐</button>
      {#if inCPlay}
        <button type="button" class="ghost" onclick={closeInCPlay} aria-label="Close" title="Close (Ctrl+W)">✕</button>
      {:else if document.fullscreenEnabled}
        <button type="button" class="ghost" onclick={toggleFullscreen} aria-label="Fullscreen" title="Fullscreen (F)">{fullscreen ? '⤡' : '⤢'}</button>
      {/if}
    </div>
  </header>

  <main class="views">
    <p class="intro">Choose a view. Each view has this menu under the logo, top left, to go to the others or back here.</p>
    <nav class="cards" aria-label="C-Play views">
      {#each webViews as view, i (view.path)}
        <a class="card" href={viewHref(view.path)}>
          <span class="key" aria-hidden="true">{i + 1}</span>
          <span class="name">{view.name}</span>
          <span class="description">{view.description}</span>
          {#if status(view.path)}<span class="status">{status(view.path)}</span>{/if}
        </a>
      {/each}
    </nav>
  </main>
</div>

<style>
  .app {
    min-height: 100%;
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
  .title .name {
    font-weight: 600;
    font-size: 1.1rem;
  }
  .tools {
    display: flex;
    align-items: center;
    gap: 0.4rem;
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

  .views {
    width: min(64rem, 100%);
    margin: 0 auto;
    align-self: center;
  }
  .intro {
    margin: 0 0 var(--gap);
    color: var(--text-muted);
    text-align: center;
  }
  .cards {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(15rem, 1fr));
    gap: var(--gap);
  }
  .card {
    position: relative;
    display: flex;
    flex-direction: column;
    gap: 0.4rem;
    min-height: 10rem;
    padding: 1.2rem 1.3rem;
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: var(--radius);
    color: var(--text);
    text-decoration: none;
  }
  .card:hover {
    border-color: var(--accent);
    background: var(--button);
  }
  .card:focus-visible {
    outline: 2px solid var(--accent);
    outline-offset: 2px;
  }
  .card .name {
    font-size: 1.4rem;
    font-weight: 600;
  }
  .description {
    color: var(--text-muted);
  }
  .status {
    margin-top: auto;
    font-size: 0.85rem;
    color: var(--accent);
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  .key {
    position: absolute;
    top: 0.8rem;
    right: 0.9rem;
    font-size: 0.75rem;
    color: var(--text-muted);
    border: 1px solid var(--border);
    border-radius: 6px;
    padding: 0.05rem 0.4rem;
  }
</style>
