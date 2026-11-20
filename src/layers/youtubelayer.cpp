/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "youtubelayer.h"

#include <sgct/sgct.h>

YoutubeLayer::YoutubeLayer(gl_adress_func_v1 opa,
    bool allowDirectRendering,
    bool loggingOn,
    std::string logLevel,
    MpvLayer::onFileLoadedCallback flc) : VideoLayer(opa, allowDirectRendering, loggingOn, logLevel, flc) {
    setType(BaseLayer::LayerType::YOUTUBE);
    // VOD content: loop by default, like a Video layer. isStream stays false, so the
    // live-stream low-latency/untimed options are never applied and time-threshold sync
    // between master and nodes works like it does for regular video.
    setEOFMode(2);
}

YoutubeLayer::~YoutubeLayer() {
}

void YoutubeLayer::initialize() {
    VideoLayer::initialize();
}

bool YoutubeLayer::usesYtdl() const {
    return true;
}

bool YoutubeLayer::refreshMasterPlaceholder() {
    // Only the nodes open the video: the master unloads it and shows a placeholder. Play,
    // pause and seeks are still synced; the nodes play on their own clock meanwhile.
    setMasterPlaceholder(masterSkipsReceive());
    if (masterPlaceholderEnabled()) {
        unload();
        return updateMasterPlaceholder();
    }
    return false;
}

void YoutubeLayer::update(bool updateRendering) {
    if (refreshMasterPlaceholder())
        return;
    VideoLayer::update(updateRendering);
}

void YoutubeLayer::updateFrame() {
    if (refreshMasterPlaceholder())
        return;
    VideoLayer::updateFrame();
}

bool YoutubeLayer::ready() const {
    if (masterPlaceholderEnabled())
        return masterPlaceholderReady();
    // Same readiness as a Video layer (loaded + rendering active), plus a load that ended in
    // an error (MPV_EVENT_END_FILE reason=error - e.g. an invalid URL, a removed video, or a
    // missing yt-dlp on this machine) is never "ready", so the layer status drops to 0 and
    // the LayerView error overlay is shown instead of a frozen frame.
    return !m_data.loadedFile.empty() && !m_data.loadFailed && m_data.updateRendering;
}
