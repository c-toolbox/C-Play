import type { CueRequest, MovieState } from './api';
import type { TakeMode } from './cue.svelte';
import { load, store } from './storage';

/** How a cue goes on air: at once, or with the transition (fade out, load, fade in). */
export type GoMode = 'cut' | 'fade';

export interface CueEntry {
  id: string;
  /** Label, empty to show the item and section. */
  name: string;
  /** Media file name of the playlist item, by which it is found when the playlist changed. */
  file: string;
  /** Playlist item when the cue was made. */
  item: number;
  /** Section to go to, -1 for none, null to keep C-Play's. */
  section: number | null;
  /** Seconds in the file, null for the section start (or where C-Play is, or the start of a new item). */
  position: number | null;
  then: TakeMode;
  go: GoMode;
}

/** The content of a *.cplaycue file. */
export interface CueListFile {
  type: 'cplaycue';
  version: 1;
  /** The playlist the cues were made with. */
  playlist: string;
  cues: CueEntry[];
}

const takeModes: TakeMode[] = ['auto', 'play', 'pause'];
let counter = 0;
const newId = () => `${Date.now().toString(36)}-${(counter++).toString(36)}-${Math.random().toString(36).slice(2, 7)}`;

const optionalNumber = (value: unknown) => (typeof value === 'number' && Number.isFinite(value) ? value : null);

function toEntry(value: unknown): CueEntry | null {
  if (typeof value !== 'object' || value === null) return null;
  const v = value as Record<string, unknown>;
  if (typeof v.file !== 'string' || typeof v.item !== 'number') return null;
  const section = optionalNumber(v.section);
  const position = optionalNumber(v.position);
  return {
    id: newId(),
    name: typeof v.name === 'string' ? v.name : '',
    file: v.file,
    item: Math.trunc(v.item),
    section: section === null ? null : Math.max(-1, Math.trunc(section)),
    position: position === null ? null : Math.max(0, position),
    then: takeModes.includes(v.then as TakeMode) ? (v.then as TakeMode) : 'auto',
    go: v.go === 'fade' ? 'fade' : 'cut',
  };
}

/** The playlist item of the cue: the one it was made with if it still has the file, else the first with it. -1 when none has. */
export function cueItem(entry: CueEntry, state: MovieState): number {
  if (state.items[entry.item]?.file === entry.file) return entry.item;
  return state.items.find((item) => item.file === entry.file)?.index ?? -1;
}

/** The commands for C-Play, null when the file of the cue is not in the playlist. */
export function cueRequest(entry: CueEntry, state: MovieState): CueRequest | null {
  const index = cueItem(entry, state);
  if (index < 0) return null;
  return {
    index: index !== state.playing ? index : undefined,
    section: entry.section ?? undefined,
    position: entry.position ?? undefined,
    play: entry.then === 'auto' ? undefined : entry.then === 'play',
  };
}

/**
 * A list of cues into the playlist, fired one after the other (GO, e.g. from a presentation
 * clicker). Kept on the device, and saved in C-Play as *.cplaycue next to the playlist.
 */
export class CueList {
  cues = $state<CueEntry[]>(load<unknown[]>('cueList', []).map(toEntry).filter((x): x is CueEntry => !!x));
  /** File name in C-Play without .cplaycue, empty when not saved. */
  name = $state<string>(load('cueListName', ''));
  /** The cue the next GO fires (also the one edited), cues.length when past the last. */
  standby = $state<number>(load('cueStandby', 0));
  /** The cue fired last, -1 for none. */
  fired = $state<number>(load('cueFired', -1));
  /** Changed since it was saved or opened. */
  edited = $state<boolean>(load('cueListEdited', false));

  /** Inserts after the standby cue (at the end when there is none), which becomes standby. */
  add(entry: Omit<CueEntry, 'id'>) {
    const at = Math.min(this.standby + 1, this.cues.length);
    this.cues = [...this.cues.slice(0, at), { ...entry, id: newId() }, ...this.cues.slice(at)];
    if (this.fired >= at) this.fired++;
    this.standby = at;
    this.#changed();
  }

  update(index: number, changes: Partial<Omit<CueEntry, 'id'>>) {
    if (!this.cues[index]) return;
    this.cues = this.cues.map((cue, i) => (i === index ? { ...cue, ...changes } : cue));
    this.#changed();
  }

  remove(index: number) {
    if (!this.cues[index]) return;
    this.cues = this.cues.filter((_, i) => i !== index);
    if (this.fired === index) this.fired = -1;
    else if (this.fired > index) this.fired--;
    if (this.standby > index) this.standby--;
    this.#changed();
  }

  move(index: number, step: 1 | -1) {
    const to = index + step;
    if (!this.cues[index] || !this.cues[to]) return;
    const cues = [...this.cues];
    [cues[index], cues[to]] = [cues[to], cues[index]];
    this.cues = cues;
    const swap = (i: number) => (i === index ? to : i === to ? index : i);
    this.standby = swap(this.standby);
    this.fired = swap(this.fired);
    this.#changed();
  }

  select(index: number) {
    this.standby = Math.max(0, Math.min(this.cues.length, index));
    this.#store();
  }

  /** The standby cue, which is fired: the next one is standby then. */
  fire(): CueEntry | undefined {
    const entry = this.cues[this.standby];
    if (entry) {
      this.fired = this.standby;
      this.standby++;
      this.#store();
    }
    return entry;
  }

  back() {
    this.select(this.standby - 1);
  }

  /** An empty list, or one opened from C-Play. */
  replace(cues: CueEntry[], name: string) {
    this.cues = cues;
    this.name = name;
    this.standby = 0;
    this.fired = -1;
    this.edited = false;
    this.#store();
  }

  saved(name: string) {
    this.name = name;
    this.edited = false;
    this.#store();
  }

  toFile(playlist: string): CueListFile {
    return { type: 'cplaycue', version: 1, playlist, cues: $state.snapshot(this.cues) };
  }

  /** The cues of a *.cplaycue file, null when it is none. */
  static parse(value: unknown): CueEntry[] | null {
    const cues = (value as Partial<CueListFile> | null)?.cues;
    if (!Array.isArray(cues)) return null;
    return cues.map(toEntry).filter((x): x is CueEntry => !!x);
  }

  #changed() {
    this.edited = true;
    this.#store();
  }

  #store() {
    store('cueList', this.cues);
    store('cueListName', this.name);
    store('cueStandby', this.standby);
    store('cueFired', this.fired);
    store('cueListEdited', this.edited);
  }
}
