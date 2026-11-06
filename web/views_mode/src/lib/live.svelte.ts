// What C-Play shows, for the cards of the start page: small event topics only, long polled.

/** The event topics read, all of them small. */
const topics = ['slides', 'triggered_slide', 'playing_in_slides', 'media_title', 'pause'] as const;
type Topic = (typeof topics)[number];

interface EventsResponse {
  type: 'snapshot' | 'events';
  sequence: string;
  values: Partial<Record<Topic, unknown>>;
  events: { topic: string; value: unknown; sequence: string }[];
}

export interface SlideName {
  index: number;
  name: string;
}

const sleep = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms));

export class LiveStatus {
  connected = $state(false);
  /** False when C-Play answers but its event feed does not run (not the master): no details. */
  details = $state(false);
  slides = $state<SlideName[]>([]);
  triggered = $state(-1);
  selected = $state(-1);
  mediaTitle = $state('');
  paused = $state(true);

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

  #set(topic: string, value: unknown) {
    if (topic === 'slides' && Array.isArray(value)) this.slides = value as SlideName[];
    else if (topic === 'triggered_slide' && typeof value === 'number') this.triggered = value;
    else if (topic === 'playing_in_slides' && typeof value === 'number') this.selected = value;
    else if (topic === 'media_title') this.mediaTitle = typeof value === 'string' ? value : '';
    else if (topic === 'pause' && typeof value === 'boolean') this.paused = value;
  }

  async #loop() {
    let since = '';
    while (this.#running) {
      try {
        if (since && !this.details) {
          // No feed to wait on: only whether C-Play answers.
          await sleep(3000);
          this.connected = (await fetch('/status', { cache: 'no-store' })).ok;
          continue;
        }
        this.#abort = new AbortController();
        const started = performance.now();
        const query = since ? `&since=${since}&wait=20000` : '';
        const response = await fetch(`/events?topics=${topics.join(',')}${query}`, { cache: 'no-store', signal: this.#abort.signal });
        if (!response.ok) throw new Error(response.statusText);
        const feed = (await response.json()) as EventsResponse;
        this.connected = true;
        if (feed.type === 'snapshot') {
          this.details = Array.isArray(feed.values.slides);
          for (const [topic, value] of Object.entries(feed.values)) this.#set(topic, value);
        }
        for (const event of feed.events) this.#set(event.topic, event.value);
        since = feed.sequence;
        // The feed answers at once without events when it stops (C-Play quits).
        if (feed.type === 'events' && !feed.events.length && performance.now() - started < 1000) await sleep(1000);
      } catch (error) {
        if (!this.#running) return;
        this.connected = false;
        since = '';
        await sleep(error instanceof DOMException && error.name === 'AbortError' ? 0 : 2000);
      }
    }
  }
}
