/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifdef MULTI_VIDEO_LAYER
#include "multivideolayer.h"
#include <sgct/sgct.h>
#include <utils/multivideoconfig.h>

#include <filesystem>

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
    m_handler = std::make_unique<MultiVideoHandler>();
}

MultiVideoLayer::~MultiVideoLayer() = default;

// ─── Master-side API ─────────────────────────────────────────────────────────

void MultiVideoLayer::setCompositionJson(const std::string& json) {
    if (m_compositionJson != json) {
        m_compositionJson = json;
        m_compositionNeedsMasterApply = true;
        setNeedSync(); // triggers a full sync to nodes
    }
}

const std::string& MultiVideoLayer::compositionJson() const {
    return m_compositionJson;
}

// ─── Node-side API ────────────────────────────────────────────────────────────

namespace {
// Try to locate nodes.json in a set of candidate locations. The default path is
// CWD-relative ("./data/multivideo/nodes.json"), which only works when the app is
// launched from its install directory. To be robust against other working
// directories, also walk up from the current directory looking for
// <dir>/data/multivideo/nodes.json (covers e.g. running from a build/ subfolder).
std::string findNodesJsonPath() {
    std::vector<std::string> candidates;
    candidates.push_back(NodeIdentityConfig::kDefaultFilePath);

    try {
        namespace fs = std::filesystem;
        auto cur = fs::current_path();
        for (int i = 0; i < 6 && !cur.empty(); ++i) {
            candidates.push_back((cur / "data" / "multivideo" / "nodes.json").string());
            if (!cur.has_parent_path() || cur.parent_path() == cur)
                break;
            cur = cur.parent_path();
        }
    } catch (...) {}

    for (const auto& c : candidates) {
        try {
            if (std::filesystem::exists(c))
                return c;
        } catch (...) {}
    }
    return "";
}

// If every entry in the composition targets exactly one logical node and they all
// agree on that single key, return it. This is the common "one video per eye on a
// single node" case (e.g. both eyes keyed under "Node1"). Used as a fallback when
// this node's identity cannot be resolved from nodes.json, so the videos still
// render instead of silently failing. Returns "" otherwise.
std::string singleTargetNodeId(const MultiVideoConfig& config) {
    const auto& entries = config.entries();
    if (entries.empty())
        return "";
    std::string key;
    for (const auto& e : entries) {
        if (e.paths.size() != 1)
            return "";
        const auto& k = e.paths.begin()->first;
        if (!key.empty() && key != k)
            return "";
        key = k;
    }
    return key;
}
} // namespace

void MultiVideoLayer::applyCompositionOnNode() {
    if (m_compositionJson.empty()) {
        m_handler->clearAll();
        m_compositionPendingApply = false;
        sgct::Log::Warning("MultiVideoLayer::applyCompositionOnNode: empty JSON, nothing to do");
        return;
    }

    MultiVideoConfig config;
    if (!config.loadFromString(m_compositionJson)) {
        m_handler->clearAll();
        m_compositionPendingApply = false;
        sgct::Log::Error("MultiVideoLayer::applyCompositionOnNode: failed to parse composition JSON");
        return;
    }

    // Resolve this node's identity. The composition keys per-node video paths by a
    // logical node id (e.g. "Node1"); we must map this machine to that id via
    // nodes.json. This is the most common source of intermittent failure: if the
    // file isn't found at CWD-relative ./data/multivideo/nodes.json, or this node's
    // IP isn't listed in it, identity resolution fails and no sub-layers are created.
    std::string myAddress;
    try {
        myAddress = sgct::Engine::instance().thisNode().address();
    } catch (...) {}

    const std::string nodesPath = findNodesJsonPath();
    NodeIdentityConfig nodeId;
    if (!nodesPath.empty()) {
        nodeId.loadFromFile(nodesPath);
    } else {
        // Last resort: try the default CWD-relative path (may still exist).
        nodeId.loadFromFile();
    }

    std::string myNodeId = nodeId.thisNodeId();
    if (!myNodeId.empty()) {
        sgct::Log::Info(std::format(
            "MultiVideoLayer: node identity resolved to '{}' (address '{}', nodes.json '{}')",
            myNodeId, myAddress, nodesPath));
    } else {
        // Identity could not be resolved from nodes.json. Fall back to the raw SGCT
        // address as the key; if that still doesn't match any entry's paths and the
        // composition targets a single logical node (the common one-video-per-eye-on-
        // one-node case), use that node id so the videos render instead of failing.
        myNodeId = myAddress;
        const std::string fallbackId = (!isMaster()) ? singleTargetNodeId(config) : "";
        if (!fallbackId.empty() && !config.entries().empty()) {
            bool anyMatch = false;
            for (const auto& e : config.entries()) {
                if (e.paths.count(myNodeId)) { anyMatch = true; break; }
            }
            if (!anyMatch) {
                myNodeId = fallbackId;
                sgct::Log::Warning(std::format(
                    "MultiVideoLayer: could not resolve node identity from nodes.json "
                    "(address '{}', file '{}'). Composition targets a single node '{}'; "
                    "using it as fallback. Add this node's IP to data/multivideo/nodes.json "
                    "for correct multi-node routing.",
                    myAddress, nodesPath.empty() ? std::string("<not found>") : nodesPath, fallbackId));
            } else {
                sgct::Log::Warning(std::format(
                    "MultiVideoLayer: no nodeId mapping for address '{}'; using address as key", myNodeId));
            }
        } else if (fallbackId.empty()) {
            sgct::Log::Warning(std::format(
                "MultiVideoLayer: no nodeId mapping found; using address '{}' as fallback", myNodeId));
        }
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
    if (!hasSubLayers() && m_compositionNeedsMasterApply && !m_compositionJson.empty()) {
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
        m_compositionNeedsMasterApply = false;
    }

    if (m_handler && m_handler->isActive()) {
        // Sub-players manage their own lifecycle; skip parent mpv file loading.
        // However, we must still apply decoded time/pause state from decodeTypeAlways.
        if (!isMaster()) {
            // Apply decoded state from MpvLayer::decodeTypeAlways
            setTimePause(m_data.mediaShouldPause, false);
            setTimePosition(m_data.timeToSet, m_data.timeIsDirty);
            m_data.timeIsDirty = false;

            // Mirror the node-side handling in MpvLayer::update(): apply decoded type
            // properties (eofMode / loopTime) so that changes made on the master after
            // initial load reach the per-eye sub-players. Our setEOFMode / setLoopTime
            // overrides forward to the handler, and the base MpvLayer setters are no-ops
            // here because this node's parent mpv instance is never initialized.
            if (m_data.typePropertiesDecode) {
                setEOFMode(m_data.eofMode_Dec);
                setLoopTime(m_data.loopTimeA_Dec, m_data.loopTimeB_Dec, m_data.loopTimeEnabled_Dec);
                m_data.typePropertiesDecode = false;
            }
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
        m_compositionNeedsMasterApply = true;
        // Request sub-player rebuild on the next updateFrame() call
        if (!m_compositionJson.empty()) {
            m_compositionPendingApply = true;
        } else {
            disableMultiVideo();
            m_compositionPendingApply = false;
        }
    }
}

void MultiVideoLayer::encodeTypeAlways(std::vector<std::byte>& data) {
    // Call base (MpvLayer) always encode (pause state + time position)
    MpvLayer::encodeTypeAlways(data);

    // Sub-players are node-local projections of the composition. Their playback
    // state is derived from the parent state above and must not affect packet layout.
    bool hasSubs = false;
    sgct::serializeObject(data, hasSubs);
}

void MultiVideoLayer::decodeTypeAlways(const std::vector<std::byte>& data, unsigned int& pos) {
    MpvLayer::decodeTypeAlways(data, pos);

    bool hasSubs = false;
    sgct::deserializeObject(data, pos, hasSubs);
    if (hasSubs) {
        sgct::Log::Warning("MultiVideoLayer: ignoring unsupported serialized sub-layer state");
        pos = static_cast<unsigned int>(data.size());
    }
}
#endif
