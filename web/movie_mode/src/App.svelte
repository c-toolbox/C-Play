<script lang="ts">
  import { onMount, untrack } from 'svelte';
  import CueListPanel from './components/CueListPanel.svelte';
  import Picture from './components/Picture.svelte';
  import Playlist from './components/Playlist.svelte';
  import SectionList from './components/SectionList.svelte';
  import Timeline from './components/Timeline.svelte';
  import { ApiError, api, sectionAt, sectionEndModes, type CueRequest, type SectionInfo } from './lib/api';
  import { PreparedCue, type TakeMode } from './lib/cue.svelte';
  import { CueList, cueItem, cueRequest, type CueEntry } from './lib/cuelist.svelte';
  import { LiveMovie } from './lib/live.svelte';
  import { PreviewStream, webRtcSupported } from './lib/preview.svelte';
  import { load, store } from './lib/storage';
  import { formatTime, parseTime } from './lib/time';

  const live = new LiveMovie();
  const cue = new PreparedCue();
  const cueList = new CueList();
  // Pictures over WebRTC: what plays, and the cue in prepare mode.
  const livePicture = new PreviewStream('live');
  const cuePicture = new PreviewStream('prepare');
  // Opened by C-Play in its web view window, which closes when the page title says so.
  const inCPlay = new URLSearchParams(location.search).get('host') === 'cplay';
  const takeModes: { mode: TakeMode; label: string; title: string }[] = [
    { mode: 'auto', label: 'As C-Play', title: 'Auto play setting of C-Play for a new item, otherwise play or pause as now' },
    { mode: 'play', label: 'Play', title: 'Play after the cue is taken' },
    { mode: 'pause', label: 'Pause', title: 'Pause at the cue' },
  ];

  let theme = $state<'dark' | 'light'>(load('theme', 'dark'));
  // Live: every action goes to C-Play at once. Otherwise actions prepare a cue, sent when going live.
  let isLive = $state<boolean>(load('live', true));
  let busy = $state(false);
  let message = $state<{ text: string; error: boolean } | null>(null);
  let messageTimer: ReturnType<typeof setTimeout> | undefined;
  let fullscreen = $state(false);
  let timeText = $state('');
  // C-Play sends pictures (built with WebRTC); the device shows them unless switched off.
  let picturesAvailable = $state(false);
  let showPictures = $state<boolean>(load('pictures', true));
  let livePictureProblem = $state('');
  let cuePictureProblem = $state('');
  // The cue picture plays on from the cue (no sound) instead of showing one frame.
  let cuePictureRuns = $state(false);
  // Where the cue timeline is dragged over, shown before it is released.
  let scrubTime = $state<number | null>(null);
  // Seconds a fade takes in C-Play.
  let fadeSeconds = $state(2);
  // The step the automatic transition is at, null when it does not run.
  let transitionStep = $state<'out' | 'load' | 'in' | null>(null);
  let transitionCancelled = false;
  // A cue of the cue list is being sent (cut).
  let firing = $state(false);

  const s = $derived(live.state);
  const items = $derived(s?.items ?? []);

  // On air: what C-Play plays.
  const onAirItem = $derived(s && s.playing >= 0 ? items[s.playing] : undefined);
  const onAirTitle = $derived(onAirItem?.title ?? s?.loaded?.title ?? '');
  const onAirSections = $derived(s?.loaded?.sections ?? []);
  const onAirSection = $derived(s && s.section >= 0 ? onAirSections[s.section] : undefined);
  const onAirDuration = $derived(live.duration || onAirItem?.duration || 0);
  const sectionRemaining = $derived(onAirSection ? onAirSection.end - live.position : null);

  // The prepared cue.
  const cueIndex = $derived(s ? cue.target(s) : -1);
  const cueIsOnAir = $derived(s ? !cue.isNewItem(s) : true);
  const cueTitle = $derived(cueIsOnAir ? onAirTitle : (items[cueIndex]?.title ?? ''));
  const cueSections = $derived(s ? cue.sections(s) : []);
  const cueDuration = $derived(cueIsOnAir ? onAirDuration : (items[cueIndex]?.duration ?? 0));
  const cueStart = $derived(s ? cue.startTime(s) : null);
  const cueSection = $derived(cue.section ?? (cueIsOnAir && s ? s.section : -1));
  const cueChanged = $derived(s ? cue.changed(s) : false);
  const hasState = $derived(!!s);
  const picturesOn = $derived(picturesAvailable && showPictures);
  const cuePictureTime = $derived(scrubTime ?? cueStart);
  const visibility = $derived(live.visibility ?? 100);
  // The cluster nodes still load the file; C-Play holds playback until they have it.
  const nodesLoading = $derived(!!live.mainLoad && !live.mainLoad.ready);
  const nodesLoadingTitle = $derived(nodesLoading ? `Waiting for the nodes to load the file: ${live.mainLoad?.waiting.join(', ')}` : '');
  // Playing is held while the nodes load (pausing is not).
  const playHeld = $derived(nodesLoading && live.paused !== false);

  // Step 2 of a transition: the cue when preparing, otherwise the next playlist item. Loaded paused,
  // so it can be faded in when it plays.
  const nextRequest = $derived.by((): CueRequest | null => {
    if (!s) return null;
    if (!isLive) {
      const request = cue.request(s);
      return request ? { ...request, play: false } : null;
    }
    const next = s.playing + 1;
    return next < items.length ? { index: next, play: false } : null;
  });
  const nextTitle = $derived(
    !nextRequest ? '' : nextRequest.index !== undefined ? (items[nextRequest.index]?.title ?? '') : cueTitle,
  );
  const loadLabel = $derived(isLive ? 'Load next' : 'Load cue');

  $effect(() => {
    document.documentElement.dataset.theme = theme;
  });

  // The playlist changed in C-Play.
  $effect(() => {
    if (s && !isLive) cue.validate(s);
  });

  // The time field shows where the cue starts, unless it is being typed in.
  $effect(() => {
    if (document.activeElement?.id !== 'cue-time') timeText = cueStart === null ? '' : formatTime(cueStart, true);
  });

  $effect(() => {
    if (picturesOn && hasState) livePicture.start();
    else livePicture.stop();
  });

  $effect(() => {
    if (picturesOn && hasState && !isLive) cuePicture.start();
    else cuePicture.stop();
  });

  // The cued item at the cue (or where it is dragged over), or at what plays when the cue keeps
  // C-Play's position.
  $effect(() => {
    if (!picturesOn || !hasState || isLive) return;
    const index = cueIsOnAir ? -1 : cueIndex;
    const position = cuePictureTime ?? untrack(() => live.position);
    void api.previewPrepare(index, position, cuePictureRuns).catch(() => {});
  });

  // What C-Play reports about the pictures, e.g. no picture at a position.
  $effect(() => {
    if (!picturesOn) return;
    const timer = setInterval(async () => {
      try {
        const status = await api.previewStatus();
        livePictureProblem = status.liveError ?? '';
        cuePictureProblem = status.prepareError ?? '';
      } catch {
        // Shown by the connection state.
      }
    }, 2000);
    return () => clearInterval(timer);
  });

  onMount(() => {
    live.start();
    api
      .previewStatus()
      .then((status) => (picturesAvailable = status.available))
      .catch(() => {});
    api
      .fadeDuration()
      .then((seconds) => (fadeSeconds = Number.isFinite(seconds) && seconds > 0 ? seconds : fadeSeconds))
      .catch(() => {});
    const onFullscreen = () => (fullscreen = !!document.fullscreenElement);
    document.addEventListener('fullscreenchange', onFullscreen);
    return () => {
      live.stop();
      livePicture.stop();
      cuePicture.stop();
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
      notify(error instanceof ApiError && error.status !== 400 ? `${failure}: ${error.message}` : failure, true);
      return false;
    } finally {
      void live.refresh();
    }
  }

  const sendCue = (request: CueRequest, failure: string) => run(() => api.cue(request), failure);

  function setLive(value: boolean) {
    isLive = value;
    store('live', value);
  }

  function prepare() {
    if (!isLive) return;
    cue.reset(s);
    setLive(false);
  }

  /** Sends what the cue changes to C-Play, then follows C-Play again. */
  async function goLive() {
    if (isLive || busy) return;
    const request = s ? cue.request(s) : null;
    if (request) {
      busy = true;
      const ok = await sendCue(request, 'Could not send the cue to C-Play');
      busy = false;
      if (!ok) return;
      notify('Cue taken, live');
    }
    cue.reset(s);
    setLive(true);
  }

  function selectItem(index: number) {
    if (!isLive) cue.selectItem(index);
    else if (index !== s?.playing) void sendCue({ index }, 'Could not load the item');
  }

  function selectSection(index: number) {
    if (!isLive) cue.selectSection(index);
    else void sendCue({ section: index }, 'Could not go to the section');
  }

  function seek(seconds: number) {
    if (!isLive) {
      cue.setPosition(seconds, cueSections);
      return;
    }
    // Moving into another section loads it (which pauses), so keep playing when it played.
    const section = sectionAt(onAirSections, seconds);
    const request: CueRequest = { position: seconds };
    if (s && section !== s.section) Object.assign(request, { section, play: live.paused === false });
    void sendCue(request, 'Could not move the position');
  }

  // The section before or after current (-1 for none) around position.
  function neighbour(sections: SectionInfo[], current: number, position: number, step: 1 | -1) {
    if (current >= 0) return Math.min(sections.length - 1, Math.max(0, current + step));
    const candidates = step > 0 ? sections.filter((x) => x.start > position) : sections.filter((x) => x.end <= position).reverse();
    return candidates[0]?.index ?? (step > 0 ? -1 : 0);
  }

  function stepSection(step: 1 | -1) {
    if (!s) return;
    if (isLive) {
      const index = neighbour(onAirSections, s.section, live.position, step);
      if (index >= 0 && index !== s.section) selectSection(index);
    } else if (cueSections.length) {
      const index = neighbour(cueSections, cueSection, cueStart ?? live.position, step);
      if (index >= 0) cue.selectSection(index);
    }
  }

  function stepItem(step: 1 | -1) {
    if (!items.length || isLive) return;
    const from = cueIndex < 0 ? (step > 0 ? -1 : items.length) : cueIndex;
    cue.selectItem(Math.min(items.length - 1, Math.max(0, from + step)));
  }

  const sleep = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms));

  // Waits until condition holds, at most timeoutMs. False when cancelled.
  async function waitUntil(condition: () => boolean, timeoutMs: number) {
    const until = performance.now() + timeoutMs;
    while (!condition() && performance.now() < until) {
      if (transitionCancelled) return false;
      await sleep(100);
    }
    return !transitionCancelled;
  }

  function fadeOut() {
    return run(api.fadeOut, 'Could not fade out');
  }

  /** Sends request; the cue is on air then, so a new one starts from it. */
  async function loadCue(request: CueRequest, failure = 'Could not load') {
    const ok = await sendCue(request, failure);
    if (ok && !isLive) {
      await live.refresh();
      cue.reset(live.state);
    }
    return ok;
  }

  /** Loads the cue (or the next item) paused. */
  function loadNext() {
    return nextRequest ? loadCue(nextRequest) : Promise.resolve(false);
  }

  function playAndFadeIn(play = true) {
    return run(async () => {
      if (play && live.paused !== false) await api.play();
      await api.fadeIn();
    }, play ? 'Could not play and fade in' : 'Could not fade in');
  }

  /** Skips the master slide and shows a black background; returns what puts them back as they were. */
  async function blackOutSurroundings() {
    const [skipped, mode, color] = await Promise.all([
      api.masterSkipped().catch(() => null),
      api.backgroundMode().catch(() => null),
      api.backgroundColor().catch(() => null),
    ]);
    const changes: Promise<unknown>[] = [];
    if (skipped === false) changes.push(api.setMasterSkipped(true));
    if (mode !== null && color) changes.push(api.setBackgroundColor('#000000'), api.setBackgroundMode(1));
    await Promise.allSettled(changes);
    return async () => {
      const restores: Promise<unknown>[] = [];
      if (skipped === false) restores.push(api.setMasterSkipped(false));
      if (mode !== null && color) restores.push(api.setBackgroundColor(color), api.setBackgroundMode(mode === 1 ? 1 : 0));
      const results = await Promise.allSettled(restores);
      if (results.some((result) => result.status === 'rejected')) notify('Could not restore the master slide and background', true);
    };
  }

  /** The three steps in a row: fade out, load request (paused), play and fade in. Clicking again stops it. */
  async function transition(request: CueRequest | null = nextRequest, play = true) {
    if (transitionStep) {
      transitionCancelled = true;
      return;
    }
    if (!request) return;
    transitionCancelled = false;
    let restore: (() => Promise<void>) | null = null;
    try {
      transitionStep = 'out';
      restore = await blackOutSurroundings();
      if (visibility > 0) {
        if (!(await fadeOut())) return;
        // The feed reports the visibility; without it the fade time is all there is.
        if (!(await waitUntil(() => live.visibility !== null && live.visibility <= 0, live.visibility === null ? fadeSeconds * 1000 : fadeSeconds * 1000 + 2000)))
          return;
      }
      transitionStep = 'load';
      const target = request.index;
      const loadBefore = live.mainLoad?.id;
      if (!(await loadCue(request))) return;
      if (target !== undefined && !(await waitUntil(() => live.state?.playing === target, 10000))) return;
      // The nodes load the new file too. C-Play reports when all have it (or it stops waiting
      // for silent nodes after 30 s); without the report there is nothing to wait for.
      if (target !== undefined && live.mainLoad) {
        const loaded = () => !!live.mainLoad && live.mainLoad.id !== loadBefore && live.mainLoad.ready;
        if (!(await waitUntil(loaded, 40000))) return;
        if (!loaded()) {
          notify(`The nodes did not load the file: ${live.mainLoad?.waiting.join(', ')}`, true);
          return;
        }
        if (live.mainLoad.failed.length) notify(`Could not load the file on: ${live.mainLoad.failed.join(', ')}`, true);
        else if (live.mainLoad.timedOut) notify(`No answer from: ${live.mainLoad.waiting.join(', ')}`, true);
      }
      // Let C-Play go to the section and position once the file is there.
      await sleep(500);
      if (transitionCancelled) return;
      transitionStep = 'in';
      if (!(await playAndFadeIn(play))) return;
      // Restore once the picture is fully faded in again.
      await waitUntil(() => live.visibility !== null && live.visibility >= 100, live.visibility === null ? fadeSeconds * 1000 : fadeSeconds * 1000 + 2000);
    } finally {
      if (restore) await restore();
      if (transitionCancelled) notify('Transition stopped');
      transitionStep = null;
      transitionCancelled = false;
    }
  }

  // The cue list.

  /** The prepared cue, or what is on air when live (or nothing is prepared). */
  function newCueEntry(): Omit<CueEntry, 'id'> | null {
    if (!s) return null;
    const go = cueList.cues[cueList.standby]?.go ?? 'cut';
    if (!isLive && cue.changed(s)) {
      const item = items[cueIndex];
      if (!item) return null;
      return { name: '', file: item.file, item: cueIndex, section: cue.section, position: cue.position, then: cue.takeMode, go };
    }
    const item = items[s.playing];
    if (!item) return null;
    const position = Math.round(live.position * 10) / 10;
    return { name: '', file: item.file, item: s.playing, section: s.section, position, then: isLive ? 'auto' : cue.takeMode, go };
  }

  function addCue() {
    const entry = newCueEntry();
    if (entry) cueList.add(entry);
    else notify('Only playlist items can be cued', true);
  }

  function updateCue() {
    const entry = newCueEntry();
    if (!entry) return;
    const { file, item, section, position, then } = entry;
    cueList.update(cueList.standby, { file, item, section, position, then });
    notify(`Cue ${cueList.standby + 1} updated`);
  }

  /** Shows the standby cue as the prepared cue (and in the cue picture). */
  function prepareStandby() {
    const entry = cueList.cues[cueList.standby];
    if (isLive || !entry || !live.state) return;
    const index = cueItem(entry, live.state);
    if (index < 0) return;
    cue.set(index, entry.section, entry.position);
    cue.setTakeMode(entry.then);
  }

  function selectCue(index: number) {
    cueList.select(index);
    prepareStandby();
  }

  function cueBack() {
    cueList.back();
    prepareStandby();
  }

  /** Fires the standby cue, cut or with the transition, and puts the next one on standby. */
  async function goCue() {
    if (!s || transitionStep || firing) return;
    const index = cueList.standby;
    const entry = cueList.cues[index];
    if (!entry) return;
    const request = cueRequest(entry, s);
    if (!request) {
      notify(`Cue ${index + 1}: ${entry.file} is not in the playlist`, true);
      return;
    }
    cueList.fire();
    if (entry.go === 'fade') await transition({ ...request, play: false }, entry.then !== 'pause');
    else {
      firing = true;
      await loadCue(request, `Could not fire cue ${index + 1}`);
      firing = false;
    }
    prepareStandby();
  }

  function togglePlay() {
    if (playHeld) return;
    if (live.paused === false) void run(api.pause, 'Could not pause');
    else void run(api.play, 'Could not play');
  }

  function commitTime() {
    const seconds = parseTime(timeText);
    if (seconds === null) {
      if (timeText.trim()) notify('Type a time as h:mm:ss, mm:ss or seconds', true);
      timeText = cueStart === null ? '' : formatTime(cueStart, true);
      return;
    }
    cue.setPosition(seconds, cueSections);
  }

  // Qt WebView in C-Play has no window.close() signal, it watches the title instead.
  function closeInCPlay() {
    document.title = 'cplay:close';
    setTimeout(() => (document.title = 'C-Play Movie'), 500);
  }

  function toggleFullscreen() {
    if (document.fullscreenElement) void document.exitFullscreen();
    else void document.documentElement.requestFullscreen().catch(() => notify('Fullscreen is not available', true));
  }

  function togglePictures() {
    showPictures = !showPictures;
    store('pictures', showPictures);
  }

  function toggleTheme() {
    theme = theme === 'dark' ? 'light' : 'dark';
    store('theme', theme);
  }

  function onkeydown(event: KeyboardEvent) {
    const target = event.target as HTMLElement | null;
    if (event.defaultPrevented || event.altKey || event.ctrlKey || event.metaKey) return;
    if (target?.closest('textarea, input, select, [contenteditable="true"]')) return;
    const key = event.key;
    if (key === ' ' || key === 'k' || key === 'K') {
      if (target?.closest('button')) return;
      event.preventDefault();
      togglePlay();
    } else if (key === 'Enter' && !isLive && !target?.closest('button')) {
      event.preventDefault();
      void goLive();
    } else if (key === 'l' || key === 'L') {
      if (isLive) prepare();
      else void goLive();
    } else if ((key === 'PageDown' || key === 'PageUp') && cueList.cues.length) {
      // Presentation clickers send Page Down/Up: they run the cue list.
      event.preventDefault();
      if (event.repeat) return;
      if (key === 'PageDown') void goCue();
      else cueBack();
    } else if (key === 'g' || key === 'G') {
      if (!event.repeat) void goCue();
    } else if (key === 'ArrowRight' || key === 'ArrowLeft' || key === 'PageDown' || key === 'PageUp') {
      event.preventDefault();
      stepSection(key === 'ArrowRight' || key === 'PageDown' ? 1 : -1);
    } else if ((key === 'ArrowDown' || key === 'ArrowUp') && !isLive) {
      event.preventDefault();
      stepItem(key === 'ArrowDown' ? 1 : -1);
    } else if (key === 'Escape' && !isLive) {
      cue.reset(s);
    } else if ((key === 'p' || key === 'P') && picturesAvailable) {
      togglePictures();
    } else if ((key === 'r' || key === 'R') && picturesOn && !isLive) {
      cuePictureRuns = !cuePictureRuns;
    } else if (key === '1' && !transitionStep) {
      void fadeOut();
    } else if (key === '2' && !transitionStep && nextRequest) {
      void loadNext();
    } else if (key === '3' && !transitionStep && !playHeld) {
      void playAndFadeIn();
    } else if (key === 't' || key === 'T') {
      void transition();
    } else if (key === 'f' || key === 'F') {
      if (!inCPlay) toggleFullscreen();
    }
  }
</script>

<svelte:window {onkeydown} />

<div class="app" class:preparing={!isLive}>
  <header class="top">
    <div class="title">
      <img src="./favicon.png" alt="" class="logo" />
      <span class="name" title={s?.name}>{s?.name || 'C-Play playlist'}</span>
    </div>

    <div class="mode" role="group" aria-label="Mode">
      <button
        type="button"
        class="live-button"
        class:active={isLive}
        onclick={goLive}
        disabled={busy || !live.connected}
        title={isLive ? 'C-Play follows every action' : cueChanged ? 'Send the cue to C-Play and follow it again (L)' : 'Follow C-Play again (L)'}
      >
        <span class="dot"></span>{!isLive && cueChanged ? 'Take & go live' : 'Live'}
      </button>
      <button
        type="button"
        class="prepare-button"
        class:active={!isLive}
        onclick={prepare}
        disabled={!s}
        title="Prepare a cue without C-Play following (L)"
      >
        Prepare
      </button>
    </div>

    {#if s}
      <div class="transition" role="group" aria-label="Transition">
        <span class="fade-level" title={`Picture ${visibility}%${live.syncFades ? ', the sound fades with it' : ''}`}>
          <span class="fade-bar" style:width={`${visibility}%`}></span>
        </span>
        <button
          type="button"
          class="step"
          class:running={transitionStep === 'out'}
          onclick={fadeOut}
          disabled={!live.connected || !!transitionStep || visibility <= 0}
          title={`Fade the on air picture out${live.syncFades ? ' with the sound' : ''} (1)`}
        >
          <span class="num">1</span>Fade out
        </button>
        <button
          type="button"
          class="step"
          class:running={transitionStep === 'load'}
          onclick={() => void loadNext()}
          disabled={!live.connected || !!transitionStep || !nextRequest}
          title={nextRequest ? `${loadLabel}, paused: ${nextTitle || 'the cue'} (2)` : isLive ? 'No next item in the playlist' : 'Prepare a cue first'}
        >
          <span class="num">2</span>{loadLabel}
        </button>
        <button
          type="button"
          class="step"
          class:running={transitionStep === 'in'}
          onclick={() => playAndFadeIn()}
          disabled={!live.connected || !!transitionStep || playHeld}
          title={playHeld ? nodesLoadingTitle : `Play and fade the picture in${live.syncFades ? ' with the sound' : ''} (3)`}
        >
          <span class="num">3</span>Play & fade in
        </button>
        <button
          type="button"
          class="step auto"
          class:running={!!transitionStep}
          onclick={() => transition()}
          disabled={!live.connected || (!transitionStep && !nextRequest)}
          title={transitionStep === 'load' && nodesLoading ? `${nodesLoadingTitle}. Stop the transition (T)` : transitionStep ? 'Stop the transition (T)' : `All three in a row, ${fadeSeconds} s fades (T)`}
        >
          {transitionStep ? '■ Stop' : '⇄ Transition'}
        </button>
      </div>
    {/if}

    <div class="tools">
      <span class="connection" class:online={live.connected} title={live.connected ? 'Connected to C-Play' : 'Not connected to C-Play'}>
        {live.connected ? 'Connected' : 'Offline'}
      </span>
      {#if picturesAvailable}
        <button
          type="button"
          class="ghost"
          class:selected={showPictures}
          onclick={togglePictures}
          aria-pressed={showPictures}
          title={webRtcSupported ? 'Show the pictures of what plays and of the cue (P)' : 'Pictures need WebRTC, which this browser does not have'}
          >▣ Pictures</button
        >
      {/if}
      <button type="button" class="ghost" onclick={toggleTheme} aria-label="Switch light or dark" title="Light or dark">◐</button>
      {#if inCPlay}
        <button type="button" class="ghost" onclick={closeInCPlay} aria-label="Close movie view" title="Close (Ctrl+W)">✕</button>
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
    <main class="stage" class:with-pictures={picturesOn}>
      {#if picturesOn}
        <section class="pictures" class:both={!isLive} aria-label="Pictures">
          <div class="shot on-air-shot">
            <header>
              <span class="label">On air</span>
              <span class="shot-info">{visibility < 100 ? `Picture ${visibility}%` : ''}</span>
            </header>
            <Picture preview={livePicture} problem={livePictureProblem} />
          </div>
          {#if !isLive}
            <div class="shot cue-shot">
              <header>
                <span class="label">Cue</span>
                <span class="shot-info">
                  {cuePictureTime === null
                    ? 'As on air'
                    : `${scrubTime !== null ? 'Dragging' : cuePictureRuns ? 'Running from' : 'At'} ${formatTime(cuePictureTime, true)}`}
                </span>
                <button
                  type="button"
                  class="run-picture"
                  class:selected={cuePictureRuns}
                  aria-pressed={cuePictureRuns}
                  onclick={() => (cuePictureRuns = !cuePictureRuns)}
                  title="Play the cue picture on from the cue, without sound, or show the one frame (R)"
                  >{cuePictureRuns ? '❚❚ Hold' : '▶ Run'}</button
                >
              </header>
              <Picture preview={cuePicture} problem={cuePictureProblem} />
            </div>
          {/if}
        </section>
      {/if}
      <aside class="side-col">
        <Playlist {items} playing={s.playing} cued={isLive ? null : cueIndex} onselect={selectItem} />
        <CueListPanel
          list={cueList}
          movie={s}
          addLabel={!isLive && cueChanged ? 'Add cue' : 'Add on air'}
          canAdd={!!(!isLive && cueChanged ? items[cueIndex] : items[s.playing])}
          canUpdate={!isLive}
          goDisabled={!live.connected || !!transitionStep || firing}
          onadd={addCue}
          onupdate={updateCue}
          onselect={selectCue}
          ongo={() => void goCue()}
          onback={cueBack}
          {notify}
        />
      </aside>

      <div class="main-col">
        <section class="card on-air" class:compact={!isLive}>
          <header>
            <span class="label">On air</span>
            {#if s.playing >= 0}<span class="position">{s.playing + 1} / {items.length}</span>{/if}
            <span class="item-title" title={onAirTitle}>{onAirTitle || 'Nothing loaded'}</span>
            <span class="state" class:playing={live.paused === false} title={nodesLoadingTitle}>
              {live.paused === null ? '' : playHeld ? 'Loading on nodes' : live.paused ? 'Paused' : 'Playing'}
            </span>
          </header>
          <div class="readout">
            <span class="clock">{formatTime(live.position, true)}</span>
            <span class="of">/ {formatTime(onAirDuration)}</span>
            {#if onAirSection}
              <span class="section-now">
                <span class="muted">Section</span>
                {onAirSection.title || `Section ${onAirSection.index + 1}`}
                <span class="muted">· {sectionEndModes[onAirSection.endMode] ?? 'Pause'} in</span>
                <span class="countdown" class:soon={sectionRemaining !== null && sectionRemaining < 10}>
                  {formatTime(Math.max(0, sectionRemaining ?? 0))}
                </span>
              </span>
            {:else}
              <span class="section-now muted">No section · {formatTime(Math.max(0, onAirDuration - live.position))} left</span>
            {/if}
          </div>
          <Timeline
            duration={onAirDuration}
            sections={onAirSections}
            playhead={live.position}
            active={s.section}
            variant="live"
            disabled={!isLive}
            onseek={seek}
          />
          <div class="transport">
            {#if isLive}
              <button type="button" onclick={() => stepSection(-1)} disabled={!onAirSections.length} title="Previous section (←)">⏮ Section</button>
              <button type="button" onclick={() => void run(() => api.seek(-10), 'Could not seek')} title="Back 10 seconds">−10 s</button>
            {/if}
            <button type="button" class="play" class:primary={live.paused !== false} onclick={togglePlay} disabled={!live.connected || playHeld} title={playHeld ? nodesLoadingTitle : 'Play or pause (Space)'}>
              {live.paused === false ? '❚❚ Pause' : '▶ Play'}
            </button>
            {#if isLive}
              <button type="button" onclick={() => void run(() => api.seek(10), 'Could not seek')} title="Forward 10 seconds">+10 s</button>
              <button type="button" onclick={() => stepSection(1)} disabled={!onAirSections.length} title="Next section (→)">Section ⏭</button>
              <button type="button" class="ghost" onclick={() => void run(api.stop, 'Could not stop')} title="Back to the start, paused">■ Stop</button>
            {/if}
          </div>
        </section>

        {#if isLive}
          <section class="card grow">
            <header>
              <span class="label">Sections</span>
              <span class="hint">Click to go to a section</span>
            </header>
            <SectionList sections={onAirSections} playing={s.section} cued={null} onselect={selectSection} />
          </section>
        {:else}
          <section class="card cue grow">
            <header>
              <span class="label">Cue</span>
              {#if cueIndex >= 0}<span class="position">{cueIndex + 1} / {items.length}</span>{/if}
              <span class="item-title" title={cueTitle}>{cueTitle || 'Pick an item in the playlist'}</span>
              <span class="badge" class:ready={cueChanged}>{cueChanged ? (cueIsOnAir ? 'Jump in on air item' : 'Loads new item') : 'Same as on air'}</span>
            </header>
            <Timeline
              duration={cueDuration}
              sections={cueSections}
              playhead={cueIsOnAir ? live.position : null}
              marker={cueStart}
              active={cueSection}
              variant="cue"
              onseek={(seconds) => {
                scrubTime = null;
                seek(seconds);
              }}
              onscrub={(seconds) => (scrubTime = seconds)}
            />
            <div class="cue-controls">
              <label class="start-at">
                <span>Start at</span>
                <input
                  id="cue-time"
                  type="text"
                  inputmode="decimal"
                  bind:value={timeText}
                  placeholder={cueIsOnAir ? 'as on air' : '00:00'}
                  onkeydown={(event) => {
                    if (event.key === 'Enter') (event.currentTarget as HTMLInputElement).blur();
                    else if (event.key === 'Escape') {
                      timeText = cueStart === null ? '' : formatTime(cueStart, true);
                      (event.currentTarget as HTMLInputElement).blur();
                    }
                  }}
                  onblur={commitTime}
                />
              </label>
              <div class="take-mode" role="radiogroup" aria-label="After taking the cue">
                <span>Then</span>
                {#each takeModes as option (option.mode)}
                  <button
                    type="button"
                    role="radio"
                    aria-checked={cue.takeMode === option.mode}
                    class:selected={cue.takeMode === option.mode}
                    onclick={() => cue.setTakeMode(option.mode)}
                    title={option.title}>{option.label}</button
                  >
                {/each}
              </div>
              <span class="spacer"></span>
              <button type="button" class="ghost" onclick={() => cue.reset(s)} disabled={!cueChanged} title="Back to what is on air (Esc)">Discard</button>
              <button type="button" class="take" onclick={goLive} disabled={busy || !live.connected} title="Send the cue to C-Play and go live (Enter)">
                {busy ? 'Sending…' : cueChanged ? 'Take → Live' : 'Go live'}
              </button>
            </div>
            <SectionList sections={cueSections} playing={cueIsOnAir ? s.section : -1} cued={cue.section !== null && cue.section >= 0 ? cue.section : null} onselect={selectSection} />
          </section>
        {/if}
      </div>
    </main>

    {#if !isLive}
      <div class="prepare-banner" role="status">
        Prepare mode: C-Play does not follow this view. <strong>Take → Live</strong> sends the cue{cueChanged ? '' : ' (nothing changed yet)'}.
      </div>
    {/if}
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
    flex-wrap: wrap;
    align-items: center;
    gap: 0.6rem var(--gap);
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
  .mode {
    display: flex;
    padding: 0.2rem;
    gap: 0.2rem;
    border-radius: 999px;
    background: var(--surface);
    border: 1px solid var(--border);
  }
  .mode button {
    border-radius: 999px;
    border-color: transparent;
    background: transparent;
    font-weight: 600;
    padding: 0.45rem 1.1rem;
    white-space: nowrap;
  }
  .live-button .dot {
    display: inline-block;
    width: 0.6rem;
    height: 0.6rem;
    border-radius: 50%;
    margin-right: 0.45rem;
    background: var(--text-muted);
    vertical-align: 0.05rem;
  }
  .live-button.active {
    background: var(--accent);
    color: var(--accent-text);
  }
  .live-button.active .dot {
    background: var(--accent-text);
    animation: pulse 1.6s ease-in-out infinite;
  }
  .preparing .live-button:not(:disabled) {
    color: var(--accent);
  }
  .preparing .live-button .dot {
    background: var(--accent);
  }
  .prepare-button.active {
    background: var(--next);
    color: #fff;
  }
  .mode button.active:hover:not(:disabled) {
    filter: brightness(1.08);
  }
  .live-button.active:hover:not(:disabled) {
    background: var(--accent);
  }
  .prepare-button.active:hover:not(:disabled) {
    background: var(--next);
  }
  @keyframes pulse {
    50% {
      opacity: 0.35;
    }
  }
  .transition {
    display: flex;
    align-items: center;
    gap: 0.35rem;
    padding: 0.2rem;
    border-radius: 999px;
    background: var(--surface);
    border: 1px solid var(--border);
  }
  .transition .step {
    border-radius: 999px;
    padding: 0.45rem 0.9rem;
    font-weight: 600;
    white-space: nowrap;
  }
  .transition .num {
    display: inline-grid;
    place-items: center;
    width: 1.25rem;
    height: 1.25rem;
    margin-right: 0.4rem;
    border-radius: 50%;
    background: var(--bg);
    color: var(--text-muted);
    font-size: 0.75rem;
  }
  .transition .step.running {
    border-color: var(--warn);
    box-shadow: inset 0 0 0 1px var(--warn);
  }
  .transition .step.running .num {
    background: var(--warn);
    color: #000;
  }
  .transition .auto {
    background: var(--next);
    border-color: var(--next);
    color: #fff;
  }
  .transition .auto:hover:not(:disabled) {
    background: var(--next);
    filter: brightness(1.1);
  }
  .transition .auto.running {
    background: var(--warn-bg);
    color: var(--text);
  }
  .fade-level {
    width: 2.6rem;
    height: 0.5rem;
    margin: 0 0.3rem 0 0.5rem;
    border-radius: 999px;
    background: var(--bg);
    border: 1px solid var(--border);
    overflow: hidden;
    flex: none;
  }
  .fade-bar {
    display: block;
    height: 100%;
    background: var(--accent);
    transition: width 0.1s linear;
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
    white-space: nowrap;
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

  /* The pictures above the controls; beside them on wide screens (see below). */
  .stage {
    display: grid;
    grid-template-columns: minmax(16rem, 1fr) minmax(0, 2fr);
    grid-template-rows: minmax(0, 1fr);
    grid-template-areas: 'side main';
    gap: var(--gap);
    min-height: 0;
  }
  .stage.with-pictures {
    grid-template-rows: auto minmax(0, 1fr);
    grid-template-areas:
      'side pictures'
      'side main';
  }
  .side-col {
    grid-area: side;
  }
  .main-col {
    grid-area: main;
    overflow-y: auto;
  }
  .pictures {
    grid-area: pictures;
    display: grid;
    grid-template-columns: minmax(0, 1fr);
    gap: var(--gap);
    min-width: 0;
    min-height: 0;
    /* As much as the screen spares, so the controls below stay usable. */
    --picture-height: clamp(7rem, 26vh, 22rem);
  }
  .pictures.both {
    grid-template-columns: repeat(2, minmax(0, 1fr));
    --picture-height: clamp(7rem, 24vh, 20rem);
  }
  .shot {
    display: flex;
    flex-direction: column;
    gap: 0.4rem;
    min-width: 0;
    min-height: 0;
    padding: 0.6rem;
    border-radius: var(--radius);
    background: var(--surface);
    border: 1px solid var(--accent);
  }
  .shot.cue-shot {
    border-color: var(--next);
  }
  .shot > header {
    display: flex;
    align-items: center;
    gap: 0.6rem;
    min-width: 0;
    min-height: 1.8rem;
  }
  .on-air-shot .label {
    color: var(--accent);
  }
  .cue-shot .label {
    color: var(--next);
  }
  .shot-info {
    flex: 1;
    min-width: 0;
    color: var(--text-muted);
    font-size: 0.85rem;
    font-variant-numeric: tabular-nums;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }

  /* Landscape screens: the pictures get a column of their own and the whole height. */
  @media (min-width: 1280px) and (min-aspect-ratio: 3/2) {
    .stage.with-pictures {
      grid-template-columns: minmax(13rem, 0.8fr) minmax(26rem, 1.5fr) minmax(18rem, 1.4fr);
      grid-template-rows: minmax(0, 1fr);
      grid-template-areas: 'side main pictures';
    }
    .pictures,
    .pictures.both {
      grid-template-columns: minmax(0, 1fr);
      grid-auto-rows: minmax(0, 1fr);
      --picture-height: auto;
    }
    .pictures .shot > :global(.picture) {
      flex: 1;
    }
  }

  /* Not wide enough for one header line: the transition gets a line of its own. */
  @media (max-width: 1340px) {
    .transition {
      order: 1;
      flex-basis: 100%;
      justify-content: center;
      border-radius: var(--radius);
    }
  }

  /* Short screens (laptops, landscape tablets): smaller pictures. */
  @media (max-height: 820px) {
    .pictures,
    .pictures.both {
      --picture-height: clamp(6rem, 22vh, 14rem);
    }
    .transition .step {
      padding: 0.35rem 0.7rem;
    }
    /* The Prepare button says so already. */
    .prepare-banner {
      display: none;
    }
    .app {
      grid-template-rows: auto minmax(0, 1fr);
    }
  }
  .side-col,
  .main-col {
    display: flex;
    flex-direction: column;
    gap: var(--gap);
    min-height: 0;
    min-width: 0;
  }
  .side-col > :global(.playlist),
  .side-col > :global(.cue-list) {
    flex: 1;
  }

  .card {
    display: flex;
    flex-direction: column;
    gap: 0.7rem;
    padding: 0.9rem 1rem;
    border-radius: var(--radius);
    background: var(--surface);
    border: 1px solid var(--border);
    min-width: 0;
  }
  .card.grow {
    flex: 1;
    min-height: 0;
  }
  .card.on-air {
    border-color: var(--accent);
  }
  .card.cue {
    border-color: var(--next);
  }
  .card > header {
    display: flex;
    align-items: baseline;
    gap: 0.6rem;
    min-width: 0;
  }
  .label {
    font-size: 0.75rem;
    font-weight: 700;
    letter-spacing: 0.08em;
    text-transform: uppercase;
    color: var(--text-muted);
  }
  .on-air .label {
    color: var(--accent);
  }
  .cue .label {
    color: var(--next);
  }
  .position {
    font-variant-numeric: tabular-nums;
    color: var(--text-muted);
  }
  .item-title {
    font-weight: 600;
    font-size: 1.1rem;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
    flex: 1;
  }
  .hint {
    color: var(--text-muted);
    font-size: 0.85rem;
  }
  .state {
    flex: none;
    font-size: 0.8rem;
    font-weight: 600;
    color: var(--text-muted);
  }
  .state.playing {
    color: var(--accent);
  }
  .badge {
    flex: none;
    font-size: 0.75rem;
    padding: 0.15rem 0.55rem;
    border-radius: 999px;
    background: var(--button);
    color: var(--text-muted);
  }
  .badge.ready {
    background: var(--next);
    color: #fff;
  }

  .readout {
    display: flex;
    align-items: baseline;
    flex-wrap: wrap;
    gap: 0.4rem 0.8rem;
    font-variant-numeric: tabular-nums;
  }
  .clock {
    font-size: clamp(1.8rem, 4vw, 2.8rem);
    font-weight: 700;
    line-height: 1;
  }
  .compact .clock {
    font-size: 1.6rem;
  }
  .of {
    color: var(--text-muted);
    font-size: 1.1rem;
  }
  .section-now {
    margin-left: auto;
    font-size: 1rem;
  }
  .muted {
    color: var(--text-muted);
  }
  .countdown {
    font-weight: 700;
    font-size: 1.25rem;
  }
  .countdown.soon {
    color: var(--warn);
  }

  .tools button {
    white-space: nowrap;
  }
  .tools .selected,
  .run-picture.selected {
    border-color: var(--next);
    color: var(--text);
    box-shadow: inset 0 0 0 1px var(--next);
  }
  .run-picture {
    flex: none;
    padding: 0.25rem 0.6rem;
    font-size: 0.8rem;
  }

  .transport {
    display: flex;
    flex-wrap: wrap;
    gap: 0.5rem;
  }
  .transport button {
    padding: 0.7rem 1rem;
    font-weight: 600;
  }
  .transport .play {
    min-width: 8.5rem;
  }
  .compact .transport button {
    padding: 0.45rem 0.9rem;
  }

  .cue-controls {
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    gap: 0.6rem 1rem;
  }
  .start-at,
  .take-mode {
    display: flex;
    align-items: center;
    gap: 0.4rem;
    color: var(--text-muted);
    font-size: 0.9rem;
  }
  .start-at input {
    width: 7.5rem;
    font: inherit;
    font-size: 1rem;
    font-variant-numeric: tabular-nums;
    padding: 0.45rem 0.6rem;
    border-radius: 8px;
    border: 1px solid var(--border);
    background: var(--bg);
    color: var(--text);
  }
  .start-at input:focus {
    outline: 2px solid var(--next);
    outline-offset: -1px;
  }
  .take-mode button {
    padding: 0.35rem 0.7rem;
    font-size: 0.85rem;
  }
  .take-mode button.selected {
    border-color: var(--next);
    color: var(--text);
    box-shadow: inset 0 0 0 1px var(--next);
  }
  .spacer {
    flex: 1;
  }
  .take {
    padding: 0.7rem 1.4rem;
    font-weight: 700;
    font-size: 1.05rem;
    background: var(--next);
    border-color: var(--next);
    color: #fff;
  }
  .take:hover:not(:disabled) {
    background: var(--next);
    filter: brightness(1.1);
  }

  .prepare-banner {
    padding: 0.55rem 0.9rem;
    border-radius: var(--radius);
    background: color-mix(in srgb, var(--next) 16%, transparent);
    border: 1px solid var(--next);
    font-size: 0.9rem;
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

  /* Phones and portrait tablets: one column that scrolls, the cue or sections first. */
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
    .stage,
    .stage.with-pictures {
      grid-template-columns: minmax(0, 1fr);
      grid-template-rows: auto;
      grid-template-areas:
        'pictures'
        'main'
        'side';
    }
    .main-col {
      overflow: visible;
    }
    .pictures,
    .pictures.both {
      grid-template-columns: repeat(auto-fit, minmax(15rem, 1fr));
      --picture-height: auto;
    }
    .pictures .shot > :global(.picture) {
      aspect-ratio: 16 / 9;
    }
    .transition {
      flex-basis: 100%;
      flex-wrap: wrap;
      border-radius: var(--radius);
    }
    .transition .step {
      flex: 1;
    }
    .side-col {
      order: 2;
    }
    .side-col > :global(.playlist),
    .side-col > :global(.cue-list) {
      max-height: 60vh;
    }
    .section-now {
      margin-left: 0;
    }
    .take {
      flex: 1;
    }
  }
</style>
