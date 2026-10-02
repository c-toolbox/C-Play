/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef YOUTUBELAYER_H
#define YOUTUBELAYER_H

#include <layers/videolayer.h>

// A dedicated layer type for playing single YouTube video URLs through mpv's embedded
// ytdl_hook.lua (which spawns the external yt-dlp tool to resolve the media streams).
//
// Unlike a Stream layer, a YouTube video is VOD content: it gets the full set of player
// controls (play/pause, seek slider, stop/rewind, end-of-file mode, A-B section loop)
// exactly like a Video layer, and it is never forced into the live-stream low-latency /
// untimed demuxing mode (which causes audio drift for VOD). The URL is stored verbatim
// in the layer's file path and synced to all machines; each machine resolves it locally
// with its own yt-dlp, so every cluster node needs yt-dlp available.
class YoutubeLayer : public VideoLayer {
public:
    YoutubeLayer(gl_adress_func_v1 opa,
        bool allowDirectRendering = false,
        bool loggingOn = false,
        std::string logLevel = "info",
        MpvLayer::onFileLoadedCallback flc = nullptr);
    ~YoutubeLayer();

    void initialize() override;

    // A YouTube layer always goes through ytdl_hook / yt-dlp, regardless of the exact URL
    // form (watch, shorts, youtu.be, ...). This drives the ytdl_hook script-opts and the
    // low-latency/untimed skip in initMPV().
    bool usesYtdl() const override;

    bool ready() const override;
};

#endif // YOUTUBELAYER_H
