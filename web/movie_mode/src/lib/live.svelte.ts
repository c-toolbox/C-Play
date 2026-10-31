import { api, type AudioTrack, type EventsResponse, type MainVideoLoad, type MovieState, type Topic } from './api';

const sleep = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms));

function isState(value: unknown): value is MovieState {
  return typeof value === 'object' && value !== null && Array.isArray((value as MovieState).items);
}

function isMainLoad(value: unknown): value is MainVideoLoad {
  return typeof value === 'object' && value !== null && typeof (value as MainVideoLoad).ready === 'boolean';
}

/**
 * What C-Play plays: the playlist state, position, duration and pause, kept up to date by long
 * polling the event feed. When the feed is not running (C-Play is not the master) it polls instead.
 */
export class LiveMovie {
  state = $state<MovieState | null>(null);
  position = $state(0);
  duration = $state(0);
  /** null when not known (no event feed). */
  paused = $state<boolean | null>(null);
  volume = $state<number | null>(null);
  audioTracks = $state<AudioTrack[]>([]);
  /** Selected audio id minus one, -1 when none is selected. */
  audioTrack = $state(-1);
  /** How visible the picture is, 0-100 (fades); null when not known. */
  visibility = $state<number | null>(null);
  /** The sound fades with the picture. */
  syncFades = $state<boolean | null>(null);
  /** Whether the nodes have the file loaded; null when not known (no event feed). */
  mainLoad = $state<MainVideoLoad | null>(null);
  connected = $state(false);

  #running = false;
  #feedRunning = false;
  #abort: AbortController | null = null;

  start() {
    if (this.#running) return;
    this.#running = true;
    void this.#loop();
  }

  stop() {
    this.#running = false;
    this.#abort?.abort();
  }

  /** Fetches the state right away, e.g. after an action, instead of waiting for the events. */
  async refresh() {
    try {
      const [state, position, duration, volume] = await Promise.all([api.state(), api.position(), api.duration(), api.volume()]);
      this.state = state;
      this.position = position;
      this.duration = duration;
      this.volume = volume;
      // The feed has these. An answer from before a load would overwrite what it reported since.
      if (!this.#feedRunning) {
        const [audioTracks, audioTrack] = await Promise.all([api.audioTracks(), api.audioTrack()]);
        this.audioTracks = audioTracks;
        this.audioTrack = audioTrack;
      }
      this.connected = true;
    } catch {
      this.connected = false;
    }
  }

  #apply(topic: Topic, value: unknown) {
    if (topic === 'movie' && isState(value)) this.state = value;
    else if (topic === 'position' && typeof value === 'number') this.position = value;
    else if (topic === 'duration' && typeof value === 'number') this.duration = value;
    else if (topic === 'pause' && typeof value === 'boolean') this.paused = value;
    else if (topic === 'volume' && typeof value === 'number') this.volume = value;
    else if (topic === 'audiotracks' && Array.isArray(value)) this.audioTracks = value;
    else if (topic === 'playing_in_audiotracks' && typeof value === 'number') this.audioTrack = value;
    else if (topic === 'visibility' && typeof value === 'number') this.visibility = value;
    else if (topic === 'sync_image_volume_fade' && typeof value === 'boolean') this.syncFades = value;
    else if (topic === 'main_video_load' && isMainLoad(value)) this.mainLoad = value;
  }

  async #loop() {
    let since = '';
    while (this.#running) {
      try {
        if (since && !this.#feedRunning) {
          await sleep(500);
          await this.refresh();
          continue;
        }
        this.#abort = new AbortController();
        const started = performance.now();
        const response: EventsResponse = await api.events(since, this.#abort.signal);
        this.connected = true;
        if (response.type === 'snapshot') {
          this.#feedRunning = isState(response.values.movie);
          if (this.#feedRunning) for (const [topic, value] of Object.entries(response.values)) this.#apply(topic as Topic, value);
          else await this.refresh();
        }
        for (const event of response.events) this.#apply(event.topic, event.value);
        since = response.sequence;
        // The feed answers at once without events when it stops (C-Play quits).
        if (response.type === 'events' && !response.events.length && performance.now() - started < 1000) await sleep(1000);
      } catch (error) {
        if (!this.#running) return;
        this.connected = false;
        since = '';
        await sleep(error instanceof DOMException && error.name === 'AbortError' ? 0 : 2000);
      }
    }
  }
}
