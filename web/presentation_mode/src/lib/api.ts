// REST API of the C-Play HTTP server used by the presenter view (see src/httpserverthread.cpp).

export interface SlideInfo {
  index: number;
  name: string;
  notes: string;
  /** Passed over by next and previous. */
  skipped: boolean;
  /** Thumbnail revision, 0 when the slide has no thumbnail. */
  thumbnail: number;
}

export interface PresentationState {
  /** Changes each time C-Play starts. */
  session: string;
  name: string;
  /** The presentation has a file to save to. */
  canSave: boolean;
  needsSave: boolean;
  /** Slide selected in the C-Play slide list. */
  selected: number;
  /** Slide shown on the output, -1 when none is. */
  triggered: number;
  /** Slide that "next" goes to, -1 at the end. */
  next: number;
  /** "next" plays an on-click animation step of the shown slide instead of changing slide. */
  pendingStep: boolean;
  /** C-Play can capture the output as thumbnail now. */
  canCapture: boolean;
  slides: SlideInfo[];
}

/** A file C-Play loaded recently. */
export interface RecentFile {
  path: string;
  name: string;
}

export interface EventsResponse {
  type: 'snapshot' | 'events';
  sequence: string;
  values: { presentation?: PresentationState | Record<string, never> };
  events: { topic: string; value: unknown; sequence: string }[];
  reset: boolean;
}

export class ApiError extends Error {
  constructor(
    message: string,
    readonly status: number,
  ) {
    super(message);
  }
}

async function request(method: 'GET' | 'POST', path: string, params: Record<string, string | number> = {}, body?: string) {
  const query = new URLSearchParams(Object.entries(params).map(([key, value]) => [key, String(value)]));
  const url = query.size ? `${path}?${query}` : path;
  const response = await fetch(url, {
    method,
    body,
    cache: 'no-store',
    headers: body === undefined ? undefined : { 'Content-Type': 'text/plain;charset=UTF-8' },
  });
  if (!response.ok) throw new ApiError((await response.text()) || response.statusText, response.status);
  return response;
}

const post = (path: string, params?: Record<string, string | number>, body?: string) => request('POST', path, params, body);

export const api = {
  state: async () => (await (await request('GET', '/presentation_state')).json()) as PresentationState,
  events: async (since: string, signal?: AbortSignal) => {
    const query = since ? `&since=${since}&wait=20000` : '';
    const response = await fetch(`/events?topics=presentation${query}`, { cache: 'no-store', signal });
    if (!response.ok) throw new ApiError(response.statusText, response.status);
    return (await response.json()) as EventsResponse;
  },
  next: () => post('/slide_next'),
  previous: () => post('/slide_previous'),
  goTo: (index: number) => post('/load_from_slides', { index }),
  setNotes: (index: number, notes: string) => post('/slide_notes', { index }, notes),
  /** Marks the slide to be passed over by next and previous, or to be shown again. */
  setSkipped: (index: number, skipped: boolean) => post('/slide_skipped', { slide_idx: index, on: skipped ? 1 : 0 }),
  /** Captures what the C-Play output shows now as the thumbnail of the shown slide. */
  updateThumbnail: () => post('/slide_thumbnail_update'),
  save: () => post('/presentation_save'),
  /** The recently loaded presentations, the newest first. */
  recent: async () => ((await (await request('GET', '/presentation_recent')).json()) as { files?: RecentFile[] }).files ?? [],
  /** Opens one of the recent presentations in C-Play. */
  openRecent: (path: string) => post('/presentation_open_recent', { path }),
};

export function thumbnailUrl(slide: SlideInfo | undefined): string | null {
  return slide?.thumbnail ? `/slide_thumbnail?index=${slide.index}&rev=${slide.thumbnail}` : null;
}
