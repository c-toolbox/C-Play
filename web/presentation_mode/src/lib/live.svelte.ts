import { api, type PresentationState } from './api';

const sleep = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms));

function isState(value: unknown): value is PresentationState {
  return typeof value === 'object' && value !== null && Array.isArray((value as PresentationState).slides);
}

/**
 * The presentation state of C-Play, kept up to date by long polling the "presentation" event
 * topic. When the event feed is not running (C-Play is not the master) it polls the state instead.
 */
export class LivePresentation {
  state = $state<PresentationState | null>(null);
  connected = $state(false);

  #running = false;
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

  /** Fetches the state right away, e.g. after an action, instead of waiting for the event. */
  async refresh() {
    try {
      this.state = await api.state();
      this.connected = true;
    } catch {
      this.connected = false;
    }
  }

  async #loop() {
    let since = '';
    let feedRunning = false;
    while (this.#running) {
      try {
        if (since && !feedRunning) {
          await sleep(1000);
          await this.refresh();
          continue;
        }
        this.#abort = new AbortController();
        const started = performance.now();
        const response = await api.events(since, this.#abort.signal);
        this.connected = true;
        if (response.type === 'snapshot') {
          const value = response.values.presentation;
          feedRunning = isState(value);
          if (isState(value)) this.state = value;
          else await this.refresh();
        }
        for (const event of response.events) {
          if (event.topic === 'presentation' && isState(event.value)) this.state = event.value;
        }
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
