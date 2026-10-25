import { load, store } from './storage';

interface TimerData {
  /** Milliseconds counted before the timer was last started. */
  elapsed: number;
  /** Date.now() when started, null when paused. */
  startedAt: number | null;
}

const pad = (n: number) => String(n).padStart(2, '0');

export function formatDuration(ms: number) {
  const seconds = Math.floor(ms / 1000);
  const h = Math.floor(seconds / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  return `${h > 0 ? `${h}:` : ''}${pad(m)}:${pad(seconds % 60)}`;
}

/** Wall clock and the presentation timer, which survives reloads of the page. */
export class Clock {
  now = $state(Date.now());
  #timer = $state<TimerData>(load('timer', { elapsed: 0, startedAt: null }));

  running = $derived(this.#timer.startedAt !== null);
  elapsed = $derived(this.#timer.elapsed + (this.#timer.startedAt === null ? 0 : this.now - this.#timer.startedAt));
  time = $derived(new Date(this.now).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }));

  constructor() {
    setInterval(() => (this.now = Date.now()), 250);
  }

  toggle() {
    const t = this.#timer;
    this.#set(t.startedAt === null ? { elapsed: t.elapsed, startedAt: Date.now() } : { elapsed: this.elapsed, startedAt: null });
  }

  /** Starts the timer when the presentation is started and the timer was never run. */
  startIfFresh() {
    if (this.#timer.startedAt === null && this.#timer.elapsed === 0) this.toggle();
  }

  reset() {
    this.#set({ elapsed: 0, startedAt: this.#timer.startedAt === null ? null : Date.now() });
  }

  #set(data: TimerData) {
    this.#timer = data;
    store('timer', data);
  }
}
