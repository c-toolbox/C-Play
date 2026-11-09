// REST API of the C-Play HTTP server used by the layer view (see src/httpserverthread.cpp).

/** Grid parameters of a flat layer (cm and degrees), as in the C-Play layer grid parameters. */
export interface PlaneValues {
  azimuth: number;
  elevation: number;
  roll: number;
  distance: number;
  horizontal: number;
  vertical: number;
  width: number;
  height: number;
  /** 0 = width and height, 1 = height and media aspect ratio, 2 = width and media aspect ratio. */
  aspectRatio: number;
  /** The size the plane is drawn with (follows the media aspect ratio). */
  actualWidth: number;
  actualHeight: number;
  /** Moves in degrees around the circular screen instead of horizontal cm. */
  circular: boolean;
  centreDegrees: number;
  widthDegrees: number;
}

export interface LayerInfo {
  /** Row in the slide's layer list, top of the render order first. */
  row: number;
  /**
   * 0 for the layer itself. A layer in duplicate mode is listed as its duplicates instead,
   * which share its row and are numbered from 1.
   */
  sub: number;
  /** Unique within the slide, from row and sub (see layerKey). */
  key: number;
  id: string;
  title: string;
  type: string;
  /** 0 = 2D, 1 = flat plane, 2 = dome, 3 = sphere (equirectangular), 4 = sphere (EAC). */
  gridMode: number;
  /** "<number>. <name>" of the earlier slide that owns a layer kept visible from it, else empty. */
  source: string;
  locked: boolean;
  /** 0-100. */
  visibility: number;
  /** Visibility limit in percent, negative when unlocked. */
  visibilityMaximum: number;
  hasAudio: boolean;
  /** 0-100. */
  volume: number;
  plane: PlaneValues;
}

export interface SlideInfo {
  /** -1 is the master. */
  index: number;
  name: string;
  skipped: boolean;
  layers: LayerInfo[];
}

/** What C-Play moves the flat layers on (Views > Layer view > Move layers on). */
export type CanvasKind = 'fisheye' | 'flat' | 'sphere' | 'circular';

export interface CircularMapping {
  enabled: boolean;
  /** 360 degrees around the screen. */
  widthCm: number;
  heightCm: number;
  seamDegrees: number;
  clockwise: boolean;
}

export interface LayerViewState {
  name: string;
  canSave: boolean;
  needsSave: boolean;
  /** Slide selected in the C-Play slide list (-1 is the master). */
  selected: number;
  /** Slide shown on the output, -1 when none is. */
  triggered: number;
  canvas: CanvasKind;
  dome: { fov: number; radius: number };
  flat: { horizontalRange: number; verticalRange: number };
  circular: CircularMapping;
  slides: SlideInfo[];
}

/** Values the layer view can set on a layer. */
export type LayerValues = Partial<Pick<PlaneValues, 'azimuth' | 'elevation' | 'roll' | 'distance' | 'horizontal' | 'vertical' | 'width' | 'height' | 'centreDegrees' | 'widthDegrees'>> & {
  volume?: number;
};

export interface EventsResponse {
  type: 'snapshot' | 'events';
  sequence: string;
  values: { layerview?: LayerViewState | Record<string, never> };
  events: { topic: string; value: unknown; sequence: string }[];
  reset: boolean;
}

/** Identifies a layer or duplicate within its slide. */
export const layerKey = (row: number, sub: number) => row * 16 + sub;

/** Adds the layer keys to a state from C-Play. */
export function withLayerKeys(state: LayerViewState): LayerViewState {
  for (const slide of state.slides) {
    for (const layer of slide.layers) {
      layer.sub ??= 0;
      layer.key = layerKey(layer.row, layer.sub);
    }
  }
  return state;
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

export const api = {
  state: async () => withLayerKeys((await (await request('GET', '/layerview_state')).json()) as LayerViewState),
  events: async (since: string, signal?: AbortSignal) => {
    const query = since ? `&since=${since}&wait=20000` : '';
    const response = await fetch(`/events?topics=layerview${query}`, { cache: 'no-store', signal });
    if (!response.ok) throw new ApiError(response.statusText, response.status);
    return (await response.json()) as EventsResponse;
  },
  /** Sets values of the layer (or duplicate sub) in row of slide, and answers it as C-Play has it then. */
  setLayer: async (slide: number, row: number, sub: number, values: LayerValues) =>
    (await (await request('POST', '/layerview_layer', { slide, row, sub, ...values })).json()) as LayerInfo,
  /** Fades the layer (or duplicate sub) up or down, as double-clicking it in C-Play does. */
  fade: (slide: number, row: number, sub: number, on: boolean) =>
    request('POST', '/layerview_fade', { slide, row, sub, on: on ? 1 : 0 }),
  save: () => request('POST', '/presentation_save'),
};
