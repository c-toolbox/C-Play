<script lang="ts">
  import { onMount } from 'svelte';
  import NotesEditor from './components/NotesEditor.svelte';
  import SlidePreview from './components/SlidePreview.svelte';
  import SlideStrip from './components/SlideStrip.svelte';
  import { ApiError, api } from './lib/api';
  import { Clock, formatDuration } from './lib/clock.svelte';
  import { LivePresentation } from './lib/live.svelte';
  import { load, store } from './lib/storage';

  const live = new LivePresentation();
  const clock = new Clock();
  // Opened by C-Play in its presenter window, which closes when the page title says so.
  const inCPlay = new URLSearchParams(location.search).get('host') === 'cplay';

  let theme = $state<'dark' | 'light'>(load('theme', 'dark'));
  let message = $state<{ text: string; error: boolean } | null>(null);
  let messageTimer: ReturnType<typeof setTimeout> | undefined;
  let capturing = $state(false);
  // Slide picked in the strip to read or edit its notes; null follows the shown slide.
  let browsing = $state<number | null>(null);
  let fullscreen = $state(false);

  const s = $derived(live.state);
  const slides = $derived(s?.slides ?? []);
  const current = $derived(s && s.triggered >= 0 ? slides[s.triggered] : undefined);
  const next = $derived(s && s.next >= 0 ? slides[s.next] : undefined);
  const notesIndex = $derived(browsing ?? (current ? current.index : (next?.index ?? null)));
  const notesSlide = $derived(notesIndex === null ? undefined : slides[notesIndex]);
  const notesRole = $derived(
    browsing !== null && browsing !== current?.index ? 'browsing' : notesSlide === current ? 'current slide' : 'next slide',
  );

  $effect(() => {
    document.documentElement.dataset.theme = theme;
  });

  // A slide removed in C-Play.
  $effect(() => {
    if (browsing !== null && browsing >= slides.length) browsing = null;
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

  async function run(action: () => Promise<unknown>, failure: string) {
    try {
      await action();
      return true;
    } catch (error) {
      notify(error instanceof ApiError && error.status !== 409 ? `${failure}: ${error.message}` : failure, true);
      return false;
    } finally {
      void live.refresh();
    }
  }

  function goNext() {
    clock.startIfFresh();
    browsing = null;
    void run(api.next, 'Could not go to the next slide');
  }

  function goPrevious() {
    browsing = null;
    void run(api.previous, 'Could not go to the previous slide');
  }

  function goTo(index: number) {
    clock.startIfFresh();
    browsing = null;
    void run(() => api.goTo(index), 'Could not go to the slide');
  }

  async function saveNotes(index: number, notes: string) {
    await api.setNotes(index, notes);
    // Show the saved text right away instead of the older notes until the next event.
    const slide = live.state?.slides[index];
    if (slide) slide.notes = notes;
  }

  async function updateThumbnail() {
    capturing = true;
    const ok = await run(
      api.updateThumbnail,
      'Could not capture the C-Play view.',
    );
    capturing = false;
    if (ok) notify('Thumbnail updated');
  }

  async function save() {
    if (await run(api.save, 'Could not save the presentation')) notify('Presentation saved');
  }

  // Qt WebView in C-Play has no window.close() signal, it watches the title instead.
  function closeInCPlay() {
    document.title = 'cplay:close';
    setTimeout(() => (document.title = 'C-Play Presenter'), 500);
  }

  function toggleFullscreen() {
    if (document.fullscreenElement) void document.exitFullscreen();
    else void document.documentElement.requestFullscreen().catch(() => notify('Fullscreen is not available', true));
  }

  function toggleTheme() {
    theme = theme === 'dark' ? 'light' : 'dark';
    store('theme', theme);
  }

  // Presentation remotes send Page Up/Down or arrow keys.
  function onkeydown(event: KeyboardEvent) {
    const target = event.target as HTMLElement | null;
    if (event.defaultPrevented || event.altKey || event.ctrlKey || event.metaKey) return;
    if (target?.closest('textarea, input, select, [contenteditable="true"]')) return;
    if (['ArrowRight', 'ArrowDown', 'PageDown', ' ', 'Enter', 'n', 'N'].includes(event.key)) {
      if (event.key === 'Enter' && target?.closest('button')) return;
      event.preventDefault();
      goNext();
    } else if (['ArrowLeft', 'ArrowUp', 'PageUp', 'Backspace', 'p', 'P'].includes(event.key)) {
      event.preventDefault();
      goPrevious();
    } else if (event.key === 'Escape') {
      browsing = null;
    } else if (event.key === 'f' || event.key === 'F') {
      if (!inCPlay) toggleFullscreen();
    }
  }
</script>

<svelte:window {onkeydown} />

<div class="app">
  <header class="top">
    <div class="title">
      <img src="./favicon.png" alt="" class="logo" />
      <span class="name" title={s?.name}>{s?.name || 'C-Play presentation'}</span>
      {#if s?.needsSave}<span class="unsaved" title="The presentation has unsaved changes in C-Play">Unsaved</span>{/if}
    </div>

    <div class="clock">
      <span class="time" title="Time">{clock.time}</span>
      <span class="timer" class:paused={!clock.running} title="Presentation timer">{formatDuration(clock.elapsed)}</span>
      <button type="button" class="ghost" onclick={() => clock.toggle()} aria-label={clock.running ? 'Pause timer' : 'Start timer'}>
        {clock.running ? '❚❚' : '▶'}
      </button>
      <button type="button" class="ghost" onclick={() => clock.reset()} aria-label="Reset timer" title="Reset timer">↺</button>
    </div>

    <div class="tools">
      <span class="connection" class:online={live.connected} title={live.connected ? 'Connected to C-Play' : 'Not connected to C-Play'}>
        {live.connected ? 'Live' : 'Offline'}
      </span>
      <button type="button" onclick={save} disabled={!s?.canSave || !live.connected} title={s?.canSave ? 'Save the presentation file in C-Play' : 'Save the presentation in C-Play first'}>
        Save
      </button>
      <button type="button" class="ghost" onclick={toggleTheme} aria-label="Switch light or dark" title="Light or dark">◐</button>
      {#if inCPlay}
        <button type="button" class="ghost" onclick={closeInCPlay} aria-label="Close presenter view" title="Close (Ctrl+W)">✕</button>
      {:else if document.fullscreenEnabled}
        <button type="button" class="ghost" onclick={toggleFullscreen} aria-label="Fullscreen" title="Fullscreen (F)">{fullscreen ? '⤡' : '⤢'}</button>
      {/if}
    </div>
  </header>

  {#if !s}
    <div class="connecting">
      <p>{live.connected ? 'Waiting for C-Play…' : 'Connecting to C-Play…'}</p>
    </div>
  {:else}
    <main class="stage">
      <div class="main-col">
        <SlidePreview label="Current" slide={current} count={slides.length} empty={slides.length ? 'Not started, Next shows the first slide' : 'No slides in the presentation'} accent>
          {#snippet actions()}
            {#if current}
              <button type="button" onclick={updateThumbnail} disabled={capturing || !s.canCapture} title="Use what C-Play shows now as this slide's thumbnail">
                {capturing ? 'Capturing…' : current.thumbnail ? 'Update thumbnail' : 'Capture thumbnail'}
              </button>
            {/if}
          {/snippet}
        </SlidePreview>
        <div class="navigation">
          <button type="button" class="nav" onclick={goPrevious} disabled={!live.connected} aria-label="Previous slide">
            <span class="arrow">←</span> Previous
          </button>
          <button type="button" class="nav primary" onclick={goNext} disabled={!live.connected} aria-label="Next slide">
            {s.pendingStep ? 'Next step' : 'Next'} <span class="arrow">→</span>
          </button>
        </div>
      </div>

      <div class="side-col">
        <div class="next-preview">
          <SlidePreview
            label="Next"
            slide={next}
            count={slides.length}
            empty="End of presentation"
            badge={s.pendingStep ? 'Animation step first' : undefined}
          />
        </div>
        <NotesEditor slide={notesSlide} role={notesRole} onsave={saveNotes}>
          {#snippet heading()}
            {#if notesSlide && notesRole === 'browsing'}
              <span class="browsing-name">{notesSlide.index + 1}. {notesSlide.name}</span>
              <button type="button" onclick={() => goTo(notesSlide.index)}>Go to slide</button>
              <button type="button" class="ghost" onclick={() => (browsing = null)}>Back to current</button>
            {/if}
          {/snippet}
        </NotesEditor>
      </div>
    </main>

    <footer class="bottom">
      <SlideStrip {slides} current={s.triggered} next={s.next} viewing={browsing} onview={(index) => (browsing = index === current?.index ? null : index)} />
    </footer>
  {/if}

  {#if message}
    <div class="toast" class:error={message.error} role="status">{message.text}</div>
  {/if}
</div>

<style>
  .app {
    height: 100%;
    display: grid;
    grid-template-rows: auto minmax(0, 1fr) auto;
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
  .logo {
    width: 1.6rem;
    height: 1.6rem;
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
  .clock {
    display: flex;
    align-items: center;
    gap: 0.4rem;
    font-variant-numeric: tabular-nums;
  }
  .time {
    font-size: 1.6rem;
    font-weight: 600;
    margin-right: 0.6rem;
  }
  .timer {
    font-size: 1.6rem;
    font-weight: 600;
    color: var(--accent);
  }
  .timer.paused {
    color: var(--text-muted);
  }
  .tools {
    display: flex;
    align-items: center;
    gap: 0.4rem;
    flex: 1;
    justify-content: flex-end;
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
    grid-template-columns: minmax(0, 1.55fr) minmax(0, 1fr);
    gap: var(--gap);
    min-height: 0;
  }
  .main-col,
  .side-col {
    display: flex;
    flex-direction: column;
    gap: var(--gap);
    min-height: 0;
    min-width: 0;
  }
  .main-col > :global(.preview) {
    flex: 1;
  }
  .next-preview {
    display: flex;
    flex-direction: column;
    flex: 0 0 auto;
    height: clamp(9rem, 32vh, 22rem);
  }
  .next-preview > :global(.preview) {
    flex: 1;
  }
  .side-col > :global(.notes) {
    flex: 1;
  }
  .navigation {
    display: grid;
    grid-template-columns: 1fr 2fr;
    gap: var(--gap);
  }
  .nav {
    white-space: nowrap;
    padding: 1rem;
    font-size: 1.25rem;
    font-weight: 600;
    border-radius: var(--radius);
  }
  .arrow {
    font-size: 1.4rem;
  }
  .browsing-name {
    font-weight: 600;
  }

  .bottom {
    min-width: 0;
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

  /* Phones and portrait tablets: one column that scrolls. */
  @media (max-width: 820px), (orientation: portrait) and (max-width: 1100px) {
    .app {
      height: auto;
      min-height: 100%;
      grid-template-rows: auto auto auto;
    }
    .top {
      flex-wrap: wrap;
    }
    .title {
      flex-basis: 100%;
    }
    .tools {
      flex: 1;
    }
    .time,
    .timer {
      font-size: 1.25rem;
    }
    .stage {
      grid-template-columns: minmax(0, 1fr);
    }
    .main-col > :global(.preview) {
      height: min(56vw, 50vh);
      flex: none;
    }
    .next-preview {
      height: min(40vw, 30vh);
    }
    .side-col > :global(.notes) {
      min-height: 40vh;
    }
  }
</style>
