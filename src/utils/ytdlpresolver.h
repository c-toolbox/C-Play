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
//   1. PlaybackSettings::ytdlpPath() when non-empty and pointing at an existing file (or a
//      folder containing one).
//   2. "yt-dlp.exe" (or "yt-dlp") next to C-Play.exe, or inside a "yt-dlp" folder there.
//   3. "<working directory>/plugins/yt-dlp(.exe)" - also checked relative to the application
//      directory, since a GUI app may be started with a different current directory; a
//      PyInstaller onedir build ("<dir>/plugins/yt-dlp/" containing yt-dlp.exe + _internal/)
//      is resolved to the executable inside that folder.
//   4. Anywhere on PATH (QStandardPaths::findExecutable).
// Only real files are ever returned - a folder named "yt-dlp" must not be handed to mpv's
// ytdl_hook as an executable path. Returns an empty string when nothing was found, in which
// case mpv's ytdl_hook performs its own PATH search (and fails visibly if yt-dlp is missing).
std::string resolveYtdlpPath();

} // namespace YtdlpResolver

#endif // YTDLPRESOLVER_H
