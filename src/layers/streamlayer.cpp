/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "streamlayer.h"
#include <utils/streampathresolver.h>
#include <utils/texturesubdivision.h>
#include <sgct/sgct.h>

StreamLayer::StreamLayer(gl_adress_func_v1 opa,
    bool allowDirectRendering,
    bool loggingOn,
    std::string logLevel,
    MpvLayer::onFileLoadedCallback flc) : VideoLayer(opa, allowDirectRendering, loggingOn, logLevel, flc) {
    m_data.isStream = true;
    setType(BaseLayer::LayerType::STREAM);
    enableTextureSubdivision();
}

StreamLayer::~StreamLayer() {
}

void StreamLayer::initialize() {
    VideoLayer::initialize();
}

bool StreamLayer::refreshMasterPlaceholder() {
    bool placeholder = false;
    std::string resolved;
    if (isMaster() && !m_streamKey.empty())
        StreamPathResolver::instance().resolve(m_streamKey, true, resolved, &placeholder);
    setMasterPlaceholder(placeholder);
    if (masterPlaceholderEnabled()) {
        unload();
        return updateMasterPlaceholder();
    }
    return false;
}

void StreamLayer::update(bool updateRendering) {
    if (refreshMasterPlaceholder())
        return;
    VideoLayer::update(updateRendering);
}

void StreamLayer::updateFrame() {
    if (refreshMasterPlaceholder())
        return;
    VideoLayer::updateFrame();

    // renderData.texId holds the just-rendered frame (primary or ping-pong texture), which
    // stays in its FBO; a QR control frame is shown as the last clean frame instead.
    renderData.texId = m_subdivision->filterControlFrame(renderData.texId, renderData.width, renderData.height);
    m_subdivision->updateSubLayers(renderData.texId, renderData.width, renderData.height);
}

bool StreamLayer::ready() const {
    if (masterPlaceholderEnabled())
        return masterPlaceholderReady();
    // A load that ended in an error (MPV_EVENT_END_FILE reason=error, e.g. a bad YouTube
    // URL or a missing yt-dlp) is never "ready", so the layer status drops to 0 and the
    // error overlay can be shown instead of a frozen/black frame.
    return !m_data.loadedFile.empty() && !m_data.loadFailed;
}

std::string StreamLayer::streamKey() const {
    return m_streamKey;
}

void StreamLayer::setStreamKey(const std::string& key) {
    if (m_streamKey != key) {
        m_streamKey = key;
        if (isMaster())
            setNeedSync();
    }
}

std::string StreamLayer::effectiveFilePath() const {
    if (m_streamKey.empty()) {
        // Custom stream path or legacy layer: use the synced file path verbatim.
        return filepath();
    }

    std::string resolved;
    if (StreamPathResolver::instance().resolve(m_streamKey, isMaster(), resolved)) {
        // May be empty - meaning this machine intentionally does not open this stream.
        return resolved;
    }

    // Entry not found in the local predefined-streams.json: fall back to the synced path.
    return filepath();
}

void StreamLayer::encodeTypeCore(std::vector<std::byte>& data) {
    VideoLayer::encodeTypeCore(data);
    sgct::serializeObject(data, m_streamKey);
}

void StreamLayer::decodeTypeCore(const std::vector<std::byte>& data, unsigned int& pos) {
    VideoLayer::decodeTypeCore(data, pos);
    sgct::deserializeObject(data, pos, m_streamKey);
}

void StreamLayer::encodeTypeAlways(std::vector<std::byte>& data) {
    MpvLayer::encodeTypeAlways(data);
    m_subdivision->encodeAlways(data);
}

void StreamLayer::decodeTypeAlways(const std::vector<std::byte>& data, unsigned int& pos) {
    MpvLayer::decodeTypeAlways(data, pos);
    m_subdivision->decodeAlways(data, pos);
}

void StreamLayer::encodeTypeProperties(std::vector<std::byte>& data) {
    MpvLayer::encodeTypeProperties(data);
    m_subdivision->encodeProperties(data);
}

void StreamLayer::decodeTypeProperties(const std::vector<std::byte>& data, unsigned int& pos) {
    MpvLayer::decodeTypeProperties(data, pos);
    m_subdivision->decodeProperties(data, pos);
}
