/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef YTDLPRESOLVER_H
#define YTDLPRESOLVER_H

#include <string>

// Helpers for YouTube playback in mpv-based layers (Stream layers).
//
// YouTube URLs are resolved by mpv's embedded ytdl_hook.lua, which spawns the external
// yt-dlp tool. These helpers detect YouTube URLs (using the same host patterns as the
// hook's default "include" option) and locate the yt-dlp executable to pass to mpv via
// script-opts=ytdl_hook-ytdl_path=...
namespace YtdlpResolver {

// True when the path looks like a single YouTube video URL (youtube.com / youtu.be,
// including m.youtube.com and music.youtube.com). Matches ytdl_hook's default include
// pattern, so anything reported here is exactly what the lua hook would try to handle.
bool isYouTubeUrl(const std::string& path);

// Locate the yt-dlp executable on this machine:
//   1. PlaybackSettings::ytdlpPath() when non-empty and the file exists.
//   2. "yt-dlp.exe" (or "yt-dlp") next to C-Play.exe.
//   3. Anywhere on PATH (QStandardPaths::findExecutable).
// Returns an empty string when nothing was found - mpv's ytdl_hook then performs its
// own PATH search (and fails visibly if yt-dlp is missing).
std::string resolveYtdlpPath();

} // namespace YtdlpResolver

#endif // YTDLPRESOLVER_H
