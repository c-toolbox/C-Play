// Per-device conveniences (font size, timer). Storage can be unavailable, so nothing depends on it.

export function load<T>(key: string, fallback: T): T {
  try {
    const value = localStorage.getItem(`cplay-presenter:${key}`);
    return value === null ? fallback : (JSON.parse(value) as T);
  } catch {
    return fallback;
  }
}

export function store(key: string, value: unknown) {
  try {
    localStorage.setItem(`cplay-presenter:${key}`, JSON.stringify(value));
  } catch {
    // Not remembered on this device.
  }
}
