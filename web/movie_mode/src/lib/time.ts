const pad = (n: number) => String(n).padStart(2, '0');

/** 1:02:03 or 02:03, with tenths when asked. */
export function formatTime(seconds: number, tenths = false) {
  if (!Number.isFinite(seconds) || seconds < 0) seconds = 0;
  const whole = Math.floor(seconds);
  const h = Math.floor(whole / 3600);
  const m = Math.floor((whole % 3600) / 60);
  const text = `${h > 0 ? `${h}:` : ''}${pad(m)}:${pad(whole % 60)}`;
  return tenths ? `${text}.${Math.floor((seconds - whole) * 10)}` : text;
}

/** Parses "1:02:03.5", "2:03", "123" or "123.5" as seconds, null when it is not a time. */
export function parseTime(text: string): number | null {
  const parts = text.trim().replace(',', '.').split(':');
  if (parts.length > 3 || parts.some((part) => !/^\d+(\.\d*)?$/.test(part))) return null;
  return parts.reduce((total, part) => total * 60 + Number(part), 0);
}
