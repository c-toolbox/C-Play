import { api, type LayerInfo, type LayerValues } from './api';

const key = (slide: number, row: number, sub: number) => `${slide}:${row}:${sub}`;
// How long values sent to C-Play are kept shown over the state, which can be older than them.
const settleMs = 600;

interface Pending {
  /** Shown over the state until C-Play has them. */
  shown: LayerValues;
  /** Not sent yet. */
  queued: LayerValues | null;
  sending: boolean;
  settled: ReturnType<typeof setTimeout> | undefined;
}

/**
 * Sends layer values to C-Play: one request per layer at a time, the newest values once it
 * returns, so dragging does not queue up requests. The values are shown at once (see apply).
 */
export class LayerEdits {
  #pending = $state<Record<string, Pending>>({});

  constructor(private readonly onerror: (error: unknown) => void) {}

  set(slide: number, row: number, sub: number, values: LayerValues) {
    const k = key(slide, row, sub);
    const pending = (this.#pending[k] ??= { shown: {}, queued: null, sending: false, settled: undefined });
    clearTimeout(pending.settled);
    Object.assign(pending.shown, values);
    pending.queued = { ...pending.queued, ...values };
    if (!pending.sending) void this.#send(slide, row, sub);
  }

  /** The layer with the values not yet in the state. */
  apply(slide: number, layer: LayerInfo): LayerInfo {
    const shown = this.#pending[key(slide, layer.row, layer.sub)]?.shown;
    if (!shown) return layer;
    const { volume, ...plane } = shown;
    return { ...layer, volume: volume ?? layer.volume, plane: { ...layer.plane, ...plane } };
  }

  async #send(slide: number, row: number, sub: number) {
    const k = key(slide, row, sub);
    const pending = this.#pending[k];
    while (pending.queued) {
      const values = pending.queued;
      pending.queued = null;
      pending.sending = true;
      try {
        await api.setLayer(slide, row, sub, values);
      } catch (error) {
        pending.queued = null;
        this.onerror(error);
      }
      pending.sending = false;
    }
    pending.settled = setTimeout(() => {
      if (this.#pending[k] === pending && !pending.sending && !pending.queued) delete this.#pending[k];
    }, settleMs);
  }
}
