/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "multivideolayer.h"
#include <sgct/sgct.h>
#include <utils/multivideoconfig.h>

MultiVideoLayer::MultiVideoLayer(gl_adress_func_v1 opa,
                                   bool allowDirectRendering,
                                   bool loggingOn,
                                   std::string logLevel,
                                   MpvLayer::onFileLoadedCallback flc)
    : VideoLayer(opa, allowDirectRendering, loggingOn, logLevel, flc)
    , m_allowDirectRendering(allowDirectRendering)
    , m_loggingOn(loggingOn)
    , m_logLevel(logLevel)
{
    setType(BaseLayer::LayerType::MULTIVIDEO);
    m_handler = new MultiVideoHandler();
}

MultiVideoLayer::~MultiVideoLayer() {
    delete m_handler;
    m_handler = nullptr;
}

// ─── Master-side API ─────────────────────────────────────────────────────────

void MultiVideoLayer::setCompositionJson(const std::string& json) {
    if (m_compositionJson != json) {
        m_compositionJson = json;
        setNeedSync(); // triggers a full sync to nodes
    }
}

const std::string& MultiVideoLayer::compositionJson() const {
    return m_compositionJson;
}

// ─── Node-side API ────────────────────────────────────────────────────────────

void MultiVideoLayer::applyCompositionOnNode() {
    if (m_compositionJson.empty()) {
        sgct::Log::Warning("MultiVideoLayer::applyCompositionOnNode: empty JSON, nothing to do");
        return;
    }

    MultiVideoConfig config;
    if (!config.loadFromString(m_compositionJson)) {
        sgct::Log::Error("MultiVideoLayer::applyCompositionOnNode: failed to parse composition JSON");
        return;
    }

    // Resolve node identity
    NodeIdentityConfig nodeId;
    nodeId.loadFromFile(); // loads ./data/multivideo/nodes.json

    std::string myNodeId = nodeId.thisNodeId();
    if (myNodeId.empty()) {
        // Fallback: use SGCT address as the nodeId key directly
        try {
            myNodeId = sgct::Engine::instance().thisNode().address();
        } catch (...) {
            myNodeId = "";
        }
        sgct::Log::Warning(std::format(
            "MultiVideoLayer: no nodeId mapping found; using address '{}' as fallback", myNodeId));
    }

    m_handler->setConfig(config, myNodeId, m_openglProcAdr,
                          m_allowDirectRendering, m_loggingOn, m_logLevel);

    // Seed the freshly-created sub-players with the current playback mode/loop
    // state. eofMode must be applied before loopTime: MpvLayer::setLoopTime only
    // programs ab-loop when eofMode == 2, so ordering matters here.
    if (m_handler->isActive()) {
        m_handler->applyEofMode(eofMode());
        m_handler->applyLoopTime(loopTimeA(), loopTimeB(), loopTimeEnabled());
    }

    m_compositionPendingApply = false;
}

void MultiVideoLayer::disableMultiVideo() {
    if (m_handler) {
        m_handler->clearAll();
    }
}

// ─── BaseLayer overrides ──────────────────────────────────────────────────────

bool MultiVideoLayer::hasSubLayers() const {
    return m_handler && m_handler->hasSubLayers();
}

std::vector<std::shared_ptr<BaseLayer>>& MultiVideoLayer::getSubLayers() const {
    return m_handler->subLayers();
}

void MultiVideoLayer::setGridMode(uint8_t gridMode) {
    // On nodes with active sub-players, do NOT override per-entry mappings.
    if (hasSubLayers()) return;
    BaseLayer::setGridMode(gridMode);
}

void MultiVideoLayer::setStereoMode(uint8_t stereoMode) {
    if (hasSubLayers()) return;
    BaseLayer::setStereoMode(stereoMode);
}

// ─── MpvLayer overrides — forward to sub-players ─────────────────────────────

void MultiVideoLayer::setTimePosition(double timePos, bool updateTime) {
    // Always forward to base (master reference playback)
    MpvLayer::setTimePosition(timePos, updateTime);
    // Also forward to all sub-players on nodes
    if (m_handler && m_handler->isActive()) {
        m_handler->applyTime(timePos, updateTime);
    }
}

void MultiVideoLayer::setTimePause(bool paused, bool updateTime) {
    MpvLayer::setTimePause(paused, updateTime);
    if (m_handler && m_handler->isActive()) {
        m_handler->applyPause(paused);
    }
}

void MultiVideoLayer::setEOFMode(int eofMode) {
    // Apply to base (parent) mpv instance for the master preview playback...
    MpvLayer::setEOFMode(eofMode);
    // ...and forward to all sub-players on nodes so they share the same loop/eof
    // behaviour. Forwarded unconditionally (not gated by a value change) so that
    // sub-players created after the last change still receive the current eofMode.
    if (m_handler && m_handler->isActive()) {
        m_handler->applyEofMode(eofMode);
    }
}

void MultiVideoLayer::setLoopTime(double A, double B, bool enabled) {
    MpvLayer::setLoopTime(A, B, enabled);
    if (m_handler && m_handler->isActive()) {
        m_handler->applyLoopTime(A, B, enabled);
    }
}

void MultiVideoLayer::setValue(std::string param, int val) {
    MpvLayer::setValue(param, val);
    if (m_handler && m_handler->isActive()) {
        m_handler->applyValue(param, val);
    }
}

// ─── VideoLayer overrides ─────────────────────────────────────────────────────

void MultiVideoLayer::update(bool updateRendering) {
    if (m_compositionPendingApply) {
        applyCompositionOnNode();
        m_compositionPendingApply = false;
    }

    // On master side (no sub-players active), parse the composition JSON to apply
    // masterFile parameters for correct preview rendering.
    if (!hasSubLayers() && !m_compositionJson.empty()) {
        MultiVideoConfig config;
        if (config.loadFromString(m_compositionJson)) {
            const auto& mp = config.masterParams();
            if (!mp.file.empty()) {
                // Apply master parameters to this layer for correct preview rendering.
                // These are only used when hasSubLayers() is false (master side).
                setGridMode(mp.gridMode);
                setStereoMode(mp.stereoMode);
                enableAudio(mp.audio);
            }
        }
    }

    if (m_handler && m_handler->isActive()) {
        // Sub-players manage their own lifecycle; skip parent mpv file loading.
        // However, we must still apply decoded time/pause state from decodeTypeAlways.
        if (!isMaster()) {
            // Apply decoded state from MpvLayer::decodeTypeAlways
            setTimePause(m_data.mediaShouldPause, false);
            setTimePosition(m_data.timeToSet, m_data.timeIsDirty);
            m_data.timeIsDirty = false;
        }
        if (updateRendering) {
            m_handler->updateSubLayers();
        }
        return;
    }

    // No sub-players: behave like a normal VideoLayer (master reference).
    VideoLayer::update(updateRendering);
}

void MultiVideoLayer::updateFrame() {
    if (m_handler && m_handler->isActive()) {
        // Sub-players handle their own rendering; skip parent mpv render.
        m_handler->updateSubLayers();
        return;
    }
    // No sub-players: behave like a normal VideoLayer (master reference playback).
    VideoLayer::updateFrame();
}

bool MultiVideoLayer::renderingIsOn() const {
    if (m_handler && m_handler->isActive()) {
        // Sub-players have their own renderingIsOn; treat parent as always "on"
        // so postSyncPreDraw considers this layer for adding to the render list.
        return true;
    }
    return VideoLayer::renderingIsOn();
}

bool MultiVideoLayer::ready() const {
    if (m_handler && m_handler->isActive()) {
        // Consider the layer ready when sub-players exist (they manage own readiness).
        return true;
    }
    return VideoLayer::ready();
}

// ─── Serialization ───────────────────────────────────────────────────────────

void MultiVideoLayer::encodeTypeCore(std::vector<std::byte>& data) {
    // Serialize the composition JSON string so nodes can rebuild sub-players.
    sgct::serializeObject(data, m_compositionJson);
}

void MultiVideoLayer::decodeTypeCore(const std::vector<std::byte>& data, unsigned int& pos) {
    std::string newJson;
    sgct::deserializeObject(data, pos, newJson);
    if (newJson != m_compositionJson) {
        m_compositionJson = newJson;
        // Request sub-player rebuild on the next updateFrame() call
        if (!m_compositionJson.empty()) {
            m_compositionPendingApply = true;
        }
    }
}

void MultiVideoLayer::encodeTypeAlways(std::vector<std::byte>& data) {
    // Call base (MpvLayer) always encode (pause state + time position)
    MpvLayer::encodeTypeAlways(data);

    // Encode sub-layer always data if sub-players are active
    bool hasSubs = (m_handler && m_handler->hasSubLayers());
    sgct::serializeObject(data, hasSubs);
    if (hasSubs) {
        auto& subs = m_handler->subLayers();
        int count = static_cast<int>(subs.size());
        sgct::serializeObject(data, count);
        for (auto& sub : subs) {
            if (sub) {
                sub->encodeBaseAlways(data);
                sub->encodeTypeAlways(data);
            }
        }
    }
}

void MultiVideoLayer::decodeTypeAlways(const std::vector<std::byte>& data, unsigned int& pos) {
    MpvLayer::decodeTypeAlways(data, pos);

    bool hasSubs = false;
    sgct::deserializeObject(data, pos, hasSubs);
    if (hasSubs) {
        int count = 0;
        sgct::deserializeObject(data, pos, count);

        if (m_handler && m_handler->hasSubLayers()) {
            auto& subs = m_handler->subLayers();
            for (int i = 0; i < count; ++i) {
                if (i < static_cast<int>(subs.size()) && subs[i]) {
                    subs[i]->decodeBaseAlways(data, pos);
                    subs[i]->decodeTypeAlways(data, pos);
                } else {
                    // Consume data for missing sub-layer (encodeBaseAlways = 2xbool + float)
                    bool tmpBool; float tmpFloat;
                    sgct::deserializeObject(data, pos, tmpBool);
                    sgct::deserializeObject(data, pos, tmpBool);
                    sgct::deserializeObject(data, pos, tmpFloat);
                    // MpvLayer::encodeTypeAlways = bool + double + bool
                    double tmpDouble;
                    sgct::deserializeObject(data, pos, tmpBool);
                    sgct::deserializeObject(data, pos, tmpDouble);
                    sgct::deserializeObject(data, pos, tmpBool);
                }
            }
        } else {
            // Sub-players not yet ready; consume data to keep stream aligned
            for (int i = 0; i < count; ++i) {
                bool tmpBool; float tmpFloat; double tmpDouble;
                sgct::deserializeObject(data, pos, tmpBool);
                sgct::deserializeObject(data, pos, tmpBool);
                sgct::deserializeObject(data, pos, tmpFloat);
                sgct::deserializeObject(data, pos, tmpBool);
                sgct::deserializeObject(data, pos, tmpDouble);
                sgct::deserializeObject(data, pos, tmpBool);
            }
        }
    }
}
