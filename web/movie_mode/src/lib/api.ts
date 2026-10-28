// REST API of the C-Play HTTP server used by the movie view (see src/httpserverthread.cpp).

export interface SectionInfo {
  index: number;
  title: string;
  /** Seconds in the file. */
  start: number;
  end: number;
  /** At the section end: 0 pause, 1 fade out, 2 continue, 3 next section, 4 loop. */
  endMode: number;
}

export interface MovieItem {
  index: number;
  title: string;
  file: string;
  /** Seconds, 0 when not known before the file is loaded. */
  duration: number;
  /** At the file end: 0 pause, 1 next item, 2 loop. */
  endMode: number;
  sections: SectionInfo[];
}

export interface LoadedFile {
  title: string;
  file: string;
  /** The sections as edited in C-Play. */
  sections: SectionInfo[];
}

export interface MovieState {
  name: string;
  /** Playlist item loaded in C-Play, -1 when none is (or the file is not in the playlist). */
  playing: number;
  /** Section playing in the loaded file, -1 when none is. */
  section: number;
  autoPlay: boolean;
  loaded: LoadedFile | null;
  items: MovieItem[];
}

export interface EventsResponse {
  type: 'snapshot' | 'events';
  sequence: string;
  values: Partial<Record<Topic, unknown>>;
  events: { topic: Topic; value: unknown; sequence: string }[];
  reset: boolean;
}

/** Whether the cluster nodes have the main video loaded. Playback waits until they have. */
export interface MainVideoLoad {
  /** Counts the loads, a new file has a new id. */
  id: number;
  ready: boolean;
  /** C-Play stopped waiting for nodes that did not answer. */
  timedOut: boolean;
  /** Nodes still loading the file. */
  waiting: string[];
  /** Nodes that could not load the file. */
  failed: string[];
}

export const topics = ['movie', 'position', 'duration', 'pause', 'visibility', 'sync_image_volume_fade', 'main_video_load'] as const;
export type Topic = (typeof topics)[number];

/** A cue to send to C-Play. Fields left out keep what C-Play has. */
export interface CueRequest {
  /** Playlist item to load, unless it is loaded. */
  index?: number;
  /** Section to go to, -1 for none. */
  section?: number;
  /** Seconds in the file, applied after the section. */
  position?: number;
  play?: boolean;
}

/** The pictures C-Play sends over WebRTC: live (what plays) and prepare (the cue). */
export type PreviewFeed = 'live' | 'prepare';

export interface PreviewStatus {
  /** False when C-Play is built without WebRTC. */
  available: boolean;
  liveError?: string;
  prepareError?: string;
}

export class ApiError extends Error {
  constructor(
    message: string,
    readonly status: number,
  ) {
    super(message);
  }
}

async function request(method: 'GET' | 'POST', path: string, params: Record<string, string | number> = {}) {
  const query = new URLSearchParams(Object.entries(params).map(([key, value]) => [key, String(value)]));
  const url = query.size ? `${path}?${query}` : path;
  const response = await fetch(url, { method, cache: 'no-store' });
  if (!response.ok) throw new ApiError((await response.text()) || response.statusText, response.status);
  return response;
}

const post = (path: string, params?: Record<string, string | number>) => request('POST', path, params);
const text = async (path: string, params?: Record<string, string | number>) => (await request('GET', path, params)).text();
const number = async (path: string) => Number(await text(path));

export const api = {
  state: async () => (await (await request('GET', '/movie_state')).json()) as MovieState,
  position: () => number('/position'),
  duration: () => number('/duration'),
  events: async (since: string, signal?: AbortSignal) => {
    const query = since ? `&since=${since}&wait=20000` : '';
    const response = await fetch(`/events?topics=${topics.join(',')}${query}`, { cache: 'no-store', signal });
    if (!response.ok) throw new ApiError(response.statusText, response.status);
    return (await response.json()) as EventsResponse;
  },
  cue: (cue: CueRequest) => {
    const params: Record<string, number> = {};
    if (cue.index !== undefined) params.index = cue.index;
    if (cue.section !== undefined) params.section = cue.section;
    if (cue.position !== undefined) params.position = Math.max(0, cue.position);
    if (cue.play !== undefined) params.play = cue.play ? 1 : 0;
    return post('/movie_cue', params);
  },
  previewStatus: async () => (await (await request('GET', '/movie_preview')).json()) as PreviewStatus,
  /** Posts an SDP offer (ICE gathering complete), resolves to the session id and the SDP answer. */
  previewOffer: async (feed: PreviewFeed, sdp: string) => {
    const response = await fetch(`/movie_preview/offer?feed=${feed}`, {
      method: 'POST',
      cache: 'no-store',
      headers: { 'Content-Type': 'application/sdp' },
      body: sdp,
    });
    if (!response.ok) throw new ApiError((await response.text()) || response.statusText, response.status);
    return (await response.json()) as { id: string; sdp: string };
  },
  /** Also works while the page unloads. */
  previewClose: (id: string) => navigator.sendBeacon(`/movie_preview/close?id=${encodeURIComponent(id)}`),
  /** Shows playlist item index (-1: the loaded file) at position in the prepare picture. */
  previewPrepare: (index: number, position: number, play: boolean) =>
    post('/movie_preview/prepare', { index, position: Math.max(0, position), play: play ? 1 : 0 }),
  /** Seconds a fade takes in C-Play. */
  fadeDuration: () => number('/fade_duration'),
  /** Fade the picture out or in; the sound follows when C-Play syncs the fades. */
  fadeOut: () => post('/fade_image_down'),
  fadeIn: () => post('/fade_image_up'),
  play: () => post('/play'),
  pause: () => post('/pause'),
  /** Back to the start, paused. */
  stop: () => post('/stop'),
  /** Relative seek in whole seconds. */
  seek: (seconds: number) => post('/seek', { time: Math.round(seconds) }),
  /** Null when C-Play has no master slide. */
  masterSkipped: async () => {
    const value = (await text('/slide_skipped', { slide_idx: -1 })).trim();
    return value === '1' ? true : value === '0' ? false : null;
  },
  setMasterSkipped: (skipped: boolean) => post('/slide_skipped', { slide_idx: -1, on: skipped ? 1 : 0 }),
  /** 0 image, 1 color. */
  backgroundMode: () => number('/background_mode'),
  setBackgroundMode: (mode: 0 | 1) => post('/background_mode', { mode }),
  /** As #rrggbb. */
  backgroundColor: async () => (await text('/background_color')).trim(),
  setBackgroundColor: (color: string) => post('/background_color', { color: color.replace(/^#/, '') }),
  /** The cue lists saved next to the playlist in C-Play. */
  cueLists: async () => (await (await request('GET', '/movie_cuelists')).json()) as CueListFolder,
  loadCueList: async (name: string) => (await (await request('GET', '/movie_cuelist', { name })).json()) as unknown,
  saveCueList: async (name: string, cueList: unknown) => {
    const response = await fetch(`/movie_cuelist?${new URLSearchParams({ name })}`, {
      method: 'POST',
      cache: 'no-store',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(cueList),
    });
    if (!response.ok) throw new ApiError((await response.text()) || response.statusText, response.status);
  },
};

/** Where C-Play keeps the cue lists (*.cplaycue): the folder of the open playlist. */
export interface CueListFolder {
  folder: string;
  /** Name of the open playlist, the default name of its cue list. */
  playlist: string;
  /** Without the extension, newest first. */
  files: { name: string; modified: string }[];
}

/** The sections of playlist item index, as C-Play has them for the loaded item. */
export function sectionsOf(state: MovieState, index: number): SectionInfo[] {
  if (index === state.playing && state.loaded) return state.loaded.sections;
  return state.items[index]?.sections ?? [];
}

/** The section that time is in, -1 when none. */
export function sectionAt(sections: SectionInfo[], time: number): number {
  return sections.find((section) => time >= section.start && time < section.end)?.index ?? -1;
}

export const sectionEndModes = ['Pause', 'Fade out', 'Continue', 'Next section', 'Loop'];
export const itemEndModes = ['Pause', 'Next', 'Loop'];
