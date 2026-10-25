// Per-device conveniences (theme, live mode, the prepared cue). Storage can be unavailable, so nothing depends on it.

export function load<T>(key: string, fallback: T): T {
  try {
    const value = localStorage.getItem(`cplay-movie:${key}`);
    return value === null ? fallback : (JSON.parse(value) as T);
  } catch {
    return fallback;
  }
}

export function store(key: string, value: unknown) {
  try {
    localStorage.setItem(`cplay-movie:${key}`, JSON.stringify(value));
  } catch {
    // Not remembered on this device.
  }
}
