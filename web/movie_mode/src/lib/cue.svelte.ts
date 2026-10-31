import { sectionAt, sectionsOf, type CueRequest, type MovieState, type SectionInfo } from './api';
import { load, store } from './storage';

/** After taking a cue: play, pause, or leave it to C-Play (auto play for a new item, else as it was). */
export type TakeMode = 'play' | 'pause' | 'auto';

interface CueData {
  /** Playlist item, -1 for the file C-Play has loaded. */
  item: number;
  /** Section to go to, -1 for none, null to keep C-Play's. */
  section: number | null;
  /** Seconds in the file, null to keep C-Play's position (or the start of a new item). */
  position: number | null;
  /** Audio track of the item, null to leave it to C-Play. */
  audio: number | null;
  /** The name of the audio track, to show before (or without) the tracks of the item. */
  audioName: string;
}

const noAudio = { audio: null, audioName: '' };

/**
 * The cue prepared while not live: an item, section and position chosen without C-Play following.
 * Only what was changed is sent when it is taken, so taking an unchanged cue does nothing.
 */
export class PreparedCue {
  #data = $state<CueData>({ ...noAudio, ...load<Omit<CueData, keyof typeof noAudio>>('cue', { item: -1, section: null, position: null }) });
  takeMode = $state<TakeMode>(load('takeMode', 'auto'));

  item = $derived(this.#data.item);
  section = $derived(this.#data.section);
  position = $derived(this.#data.position);
  audio = $derived(this.#data.audio);
  audioName = $derived(this.#data.audioName);

  /** Starts from what C-Play plays. */
  reset(state: MovieState | null) {
    this.#set({ item: state?.playing ?? -1, section: null, position: null, ...noAudio });
  }

  selectItem(index: number) {
    this.#set({ item: index, section: null, position: null, ...noAudio });
  }

  /** Takes a cue of the cue list. */
  set(item: number, section: number | null, position: number | null, audio: number | null, audioName: string) {
    this.#set({ item, section, position, audio, audioName });
  }

  selectAudio(audio: number | null, audioName: string) {
    this.#set({ ...this.#data, audio, audioName });
  }

  selectSection(index: number) {
    this.#set({ ...this.#data, section: index, position: null });
  }

  /** A position in the cued item, in the section it falls in (none when between sections). */
  setPosition(seconds: number, sections: SectionInfo[]) {
    this.#set({ ...this.#data, section: sectionAt(sections, seconds), position: Math.max(0, seconds) });
  }

  setTakeMode(mode: TakeMode) {
    this.takeMode = mode;
    store('takeMode', mode);
  }

  /** Drops a cue that no longer fits the playlist (changed in C-Play). */
  validate(state: MovieState) {
    const { item, section } = this.#data;
    if (item >= state.items.length) this.reset(state);
    else if (section !== null && section >= this.sections(state).length) this.#set({ ...this.#data, section: null });
  }

  /** The item that loads on take: the cued one, or the loaded one for -1. */
  target(state: MovieState) {
    return this.#data.item < 0 ? state.playing : this.#data.item;
  }

  sections(state: MovieState) {
    return sectionsOf(state, this.target(state));
  }

  isNewItem(state: MovieState) {
    return this.#data.item >= 0 && this.#data.item !== state.playing;
  }

  changed(state: MovieState) {
    return this.isNewItem(state) || this.#data.section !== null || this.#data.position !== null || this.#data.audio !== null;
  }

  /** Where the cue starts playing, null when it keeps C-Play's position. */
  startTime(state: MovieState): number | null {
    const { section, position } = this.#data;
    if (position !== null) return position;
    if (section !== null && section >= 0) return this.sections(state)[section]?.start ?? 0;
    return this.isNewItem(state) ? (state.items[this.#data.item]?.listStart ?? 0) : null;
  }

  /** The commands for C-Play, null when the cue changes nothing. */
  request(state: MovieState): CueRequest | null {
    if (!this.changed(state)) return null;
    const { section, position, audio } = this.#data;
    return {
      index: this.isNewItem(state) ? this.#data.item : undefined,
      section: section ?? undefined,
      position: position ?? undefined,
      audio: audio ?? undefined,
      play: this.takeMode === 'auto' ? undefined : this.takeMode === 'play',
    };
  }

  #set(data: CueData) {
    this.#data = data;
    store('cue', data);
  }
}
