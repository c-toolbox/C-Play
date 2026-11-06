// Where flat layers are on each canvas of the layer view, and the grid values a point on a canvas
// gives. Mirrors the C-Play renderer (src/layersrendererqtitem.cpp): a plane is rotated by its
// azimuth (around -Y), elevation (around X) and roll (around Z), after being moved by its
// horizontal and vertical offsets and its distance along -Z. Y is up and azimuth 0 is -Z.

import type { CanvasKind, CircularMapping, LayerViewState, PlaneValues } from './api';

type Vec3 = [number, number, number];
export type Point = [number, number];

const rad = Math.PI / 180;
const deg = 180 / Math.PI;

export const clamp = (value: number, min: number, max: number) => Math.min(max, Math.max(min, value));
/** The angle in (-180, 180]. */
export const wrap180 = (angle: number) => angle - 360 * Math.ceil((angle - 180) / 360);

/** The plane point at (x, y) cm from its centre, in the dome frame. */
export function planePoint(p: PlaneValues, x: number, y: number): Vec3 {
  const [px, py, pz] = [x + p.horizontal, y + p.vertical, -p.distance];
  const r = p.roll * rad;
  const x1 = px * Math.cos(r) - py * Math.sin(r);
  const y1 = px * Math.sin(r) + py * Math.cos(r);
  const e = p.elevation * rad;
  const y2 = y1 * Math.cos(e) - pz * Math.sin(e);
  const z2 = y1 * Math.sin(e) + pz * Math.cos(e);
  const a = p.azimuth * rad;
  return [x1 * Math.cos(a) - z2 * Math.sin(a), y2, x1 * Math.sin(a) + z2 * Math.cos(a)];
}

/** Azimuth and elevation (degrees) of a direction in the dome frame. */
export function anglesOf([x, y, z]: Vec3): Point {
  const length = Math.hypot(x, y, z) || 1;
  return [Math.atan2(x, -z) * deg, Math.asin(clamp(y / length, -1, 1)) * deg];
}

/** Points along the outline of the plane, n per edge, from the top left corner clockwise. */
function outline(p: PlaneValues, n: number): Vec3[] {
  const w = p.actualWidth / 2;
  const h = p.actualHeight / 2;
  const points: Vec3[] = [];
  const edge = (x0: number, y0: number, x1: number, y1: number) => {
    for (let i = 0; i < n; ++i) points.push(planePoint(p, x0 + ((x1 - x0) * i) / n, y0 + ((y1 - y0) * i) / n));
  };
  edge(-w, h, w, h);
  edge(w, h, w, -h);
  edge(w, -h, -w, -h);
  edge(-w, -h, -w, h);
  return points;
}

// ---- Fisheye: equidistant around the zenith, front (azimuth 0) down, in the unit disc ----------

export function fisheyePoint([x, y, z]: Vec3, fov: number): Point {
  const length = Math.hypot(x, y, z) || 1;
  const r = Math.acos(clamp(y / length, -1, 1)) / ((fov / 2) * rad);
  const ground = Math.hypot(x, z);
  return ground < 1e-9 ? [0, 0] : [(x / ground) * r, (-z / ground) * r];
}

/** Azimuth and elevation of a point of the fisheye disc. */
export function fisheyeAngles([x, y]: Point, fov: number): Point {
  const r = Math.hypot(x, y);
  return [r < 1e-9 ? 0 : Math.atan2(x, y) * deg, 90 - r * (fov / 2)];
}

// ---- Sphere: equirectangular, x = azimuth, y = -elevation (degrees) ------------------------------

/** Outline polygons on the equirectangular map, repeated around so they wrap at +-180. */
function spherePolygons(p: PlaneValues): Point[][] {
  const points = outline(p, 16).map(anglesOf);
  // Unwrapped, so the outline does not jump across the map.
  for (let i = 1; i < points.length; ++i) points[i][0] = points[i - 1][0] + wrap180(points[i][0] - points[i - 1][0]);
  const first = points[0][0];
  const last = points[points.length - 1][0];
  const polygon: Point[] = points.map(([a, e]) => [a, -e]);
  // The outline goes once around a pole (it turns 360 degrees): close it over that pole.
  if (Math.abs(last + wrap180(first - last) - first) > 180) {
    const pole = anglesOf(planePoint(p, 0, 0))[1] >= 0 ? -90 : 90;
    polygon.push([last, pole], [first, pole]);
  }
  return [-360, 0, 360].map((shift) => polygon.map(([a, e]) => [a + shift, e] as Point));
}

// ---- Circular screen: degrees around (0 at the top), height outwards -----------------------------

/** Radius of the bottom and top of the circular screen in the unit disc. */
export const ringInner = 0.42;
export const ringOuter = 1;

export function circularRadius(heightCm: number, mapping: CircularMapping) {
  return ringInner + (heightCm / mapping.heightCm + 0.5) * (ringOuter - ringInner);
}

export function circularPoint(degrees: number, heightCm: number, mapping: CircularMapping): Point {
  const angle = (mapping.clockwise ? degrees : -degrees) * rad;
  const r = circularRadius(heightCm, mapping);
  return [Math.sin(angle) * r, -Math.cos(angle) * r];
}

/** Degrees around and height (cm) of a point of the circular canvas. */
export function circularAt([x, y]: Point, mapping: CircularMapping): Point {
  const angle = Math.atan2(x, -y) * deg;
  const r = Math.hypot(x, y);
  return [mapping.clockwise ? angle : -angle, ((r - ringInner) / (ringOuter - ringInner) - 0.5) * mapping.heightCm];
}

/** True when C-Play moves the layer in degrees around the circular screen. */
export const circularActive = (p: PlaneValues, mapping: CircularMapping) => p.circular && mapping.enabled;

/** Degrees around the circular screen of horizontal cm, as the circular surface mapping places them. */
export function cmToDegrees(cm: number, mapping: CircularMapping) {
  return mapping.seamDegrees + (mapping.clockwise ? 1 : -1) * ((cm + mapping.widthCm / 2) * 360) / mapping.widthCm;
}

export function degreesToCm(degrees: number, mapping: CircularMapping) {
  const cm = ((mapping.clockwise ? 1 : -1) * (degrees - mapping.seamDegrees) * mapping.widthCm) / 360 - mapping.widthCm / 2;
  const phase = (cm + mapping.widthCm / 2) / mapping.widthCm;
  return (phase - Math.floor(phase)) * mapping.widthCm - mapping.widthCm / 2;
}

/** Centre and width of the layer around the circular screen, in degrees. */
export function circularSpan(p: PlaneValues, mapping: CircularMapping): Point {
  return circularActive(p, mapping)
    ? [p.centreDegrees, p.widthDegrees]
    : [cmToDegrees(p.horizontal, mapping), (p.actualWidth * 360) / mapping.widthCm];
}

function circularPath(p: PlaneValues, mapping: CircularMapping): string {
  const [centre, width] = circularSpan(p, mapping);
  const r0 = circularRadius(p.vertical - p.actualHeight / 2, mapping);
  const r1 = circularRadius(p.vertical + p.actualHeight / 2, mapping);
  if (width >= 359.9) {
    const ring = (r: number) => `M 0 ${-r} A ${r} ${r} 0 1 1 0 ${r} A ${r} ${r} 0 1 1 0 ${-r} Z`;
    return `${ring(r1)} ${ring(Math.max(r0, 0))}`;
  }
  const from = centre - width / 2;
  const to = centre + width / 2;
  const at = (d: number, r: number) => {
    const angle = (mapping.clockwise ? d : -d) * rad;
    return `${Math.sin(angle) * r} ${-Math.cos(angle) * r}`;
  };
  const large = width > 180 ? 1 : 0;
  const sweep = mapping.clockwise ? 1 : 0;
  return `M ${at(from, r1)} A ${r1} ${r1} 0 ${large} ${sweep} ${at(to, r1)} L ${at(to, r0)} A ${r0} ${r0} 0 ${large} ${1 - sweep} ${at(from, r0)} Z`;
}

// ---- All canvases ---------------------------------------------------------------------------------

const toPath = (points: Point[]) => `M ${points.map(([x, y]) => `${x} ${y}`).join(' L ')} Z`;

/** The viewBox of the canvas, in its own units. */
export function viewBox(kind: CanvasKind, state: LayerViewState): [number, number, number, number] {
  if (kind === 'sphere') return [-180, -90, 360, 180];
  if (kind === 'flat') {
    const { horizontalRange: h, verticalRange: v } = state.flat;
    return [-h, -v, 2 * h, 2 * v];
  }
  return [-1.08, -1.08, 2.16, 2.16];
}

/** The SVG path of the layer outline and the point to put its name at. */
export function layerShape(kind: CanvasKind, p: PlaneValues, state: LayerViewState): { path: string; centre: Point } {
  switch (kind) {
    case 'fisheye':
      return { path: toPath(outline(p, 12).map((v) => fisheyePoint(v, state.dome.fov))), centre: fisheyePoint(planePoint(p, 0, 0), state.dome.fov) };
    case 'sphere': {
      const [a, e] = anglesOf(planePoint(p, 0, 0));
      return { path: spherePolygons(p).map(toPath).join(' '), centre: [a, -e] };
    }
    case 'flat': {
      const [w, h] = [p.actualWidth / 2, p.actualHeight / 2];
      const [x, y] = [p.horizontal, -p.vertical];
      return { path: toPath([[x - w, y - h], [x + w, y - h], [x + w, y + h], [x - w, y + h]]), centre: [x, y] };
    }
    case 'circular': {
      const [centre] = circularSpan(p, state.circular);
      return { path: circularPath(p, state.circular), centre: circularPoint(centre, p.vertical, state.circular) };
    }
  }
}

/** The position values of a layer a drag works on, in the units of the canvas. */
export function dragValues(kind: CanvasKind, p: PlaneValues, mapping: CircularMapping): Point {
  switch (kind) {
    case 'fisheye':
    case 'sphere':
      return [p.azimuth, p.elevation];
    case 'flat':
      return [p.horizontal, p.vertical];
    case 'circular':
      return [circularSpan(p, mapping)[0], p.vertical];
  }
}

/** The position values of a canvas point, in the units of dragValues. */
export function valuesAt(kind: CanvasKind, point: Point, state: LayerViewState): Point {
  switch (kind) {
    case 'fisheye':
      return fisheyeAngles(point, state.dome.fov);
    case 'sphere':
      return [point[0], -point[1]];
    case 'flat':
      return [point[0], -point[1]];
    case 'circular':
      return circularAt(point, state.circular);
  }
}

/** The plane values to send for the position values of dragValues. */
export function positionValues(kind: CanvasKind, [u, v]: Point, p: PlaneValues, state: LayerViewState): Partial<PlaneValues> {
  switch (kind) {
    case 'fisheye':
    case 'sphere':
      return { azimuth: u, elevation: clamp(v, -90, 90) };
    case 'flat': {
      const { horizontalRange, verticalRange } = state.flat;
      return { horizontal: clamp(u, -horizontalRange, horizontalRange), vertical: clamp(v, -verticalRange, verticalRange) };
    }
    case 'circular':
      return circularActive(p, state.circular)
        ? { centreDegrees: u, vertical: v }
        : { horizontal: degreesToCm(u, state.circular), vertical: v };
  }
}

/** True when the first position value is an angle that goes around (unwrapped while dragging). */
export const wrapsAround = (kind: CanvasKind) => kind !== 'flat';
