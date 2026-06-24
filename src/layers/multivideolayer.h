/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MULTIVIDEOLAYER_H
#define MULTIVIDEOLAYER_H

#include <layers/videolayer.h>
#include <utils/multivideoconfig.h>
#include <utils/nodeidentityconfig.h>
#include <utils/multivideohandler.h>

// MultiVideoLayer plays multiple videos simultaneously on cluster nodes,
// one per configured entry, all time-synced to a single master reference.
//
// On the MASTER:
//   - Behaves like a regular VideoLayer, playing the master reference video.
//   - hasSubLayers() returns false.
//   - setCompositionJson(json) stores the JSON content which is serialised
//     inside encodeTypeCore so nodes receive it on the next full sync.
//
// On NODES:
//   - After decodeTypeCore receives the JSON, applyCompositionOnNode() is
//     called to build one VideoLayer sub-player per entry in the config.
//   - hasSubLayers() returns true; the renderer skips this layer's own
//     texture and draws each sub-player instead.
//   - setGridMode / setStereoMode are no-ops (sub-players keep JSON mapping).
//   - setTimePosition / setTimePause / setEOFMode / setLoopTime / setValue
//     are forwarded to every sub-player via MultiVideoHandler.
//
class MultiVideoLayer : public VideoLayer {
public:
    MultiVideoLayer(gl_adress_func_v1 opa,
                    bool allowDirectRendering = false,
                    bool loggingOn = false,
                    std::string logLevel = "info",
                    MpvLayer::onFileLoadedCallback flc = nullptr);
    ~MultiVideoLayer();

    // --- Master-side API ---

    // Set the full composition JSON content (master calls this before sync).
    void setCompositionJson(const std::string& json);

    // Return the stored composition JSON content.
    const std::string& compositionJson() const;

    // --- Node-side API ---

    // Parse the stored JSON + load node identity file + build sub-players.
    // Called automatically from decodeTypeCore on nodes, or explicitly when
    // the main-player path receives a new config via SyncHelper.
    void applyCompositionOnNode();

    // Disable multi-video mode and clear all sub-players.
    void disableMultiVideo();

    // --- BaseLayer overrides ---

    // Returns true only on nodes when sub-players are active.
    bool hasSubLayers() const override;
    std::vector<std::shared_ptr<BaseLayer>>& getSubLayers() const override;

    // When sub-players are active, these are no-ops (sub-players keep their
    // own per-entry grid/stereo mapping from the JSON config).
    // NOTE: hides (shadows) non-virtual BaseLayer::setGridMode/setStereoMode.
    void setGridMode(uint8_t gridMode);
    void setStereoMode(uint8_t stereoMode);

    // --- MpvLayer overrides — forward to sub-players when active ---
    void setTimePosition(double timePos, bool updateTime = true) override;
    void setTimePause(bool paused, bool updateTime = true) override;
    void setEOFMode(int eofMode) override;
    void setLoopTime(double A, double B, bool enabled) override;
    void setValue(std::string param, int val) override;

    // --- VideoLayer overrides ---
    void update(bool updateRendering = true) override;
    void updateFrame() override;
    bool renderingIsOn() const override;
    bool ready() const override;

    // --- Serialization ---

    // Core (full-sync): serialises the composition JSON so nodes can rebuild.
    void encodeTypeCore(std::vector<std::byte>& data) override;
    void decodeTypeCore(const std::vector<std::byte>& data, unsigned int& pos) override;

    // Always (every frame): forwards time/state to sub-players.
    void encodeTypeAlways(std::vector<std::byte>& data) override;
    void decodeTypeAlways(const std::vector<std::byte>& data, unsigned int& pos) override;

private:
    std::string m_compositionJson;
    bool m_compositionPendingApply = false; // node side: JSON received, sub-players not yet built

    MultiVideoHandler* m_handler = nullptr;

    // Stored for passing to handler.setConfig()
    bool m_allowDirectRendering = false;
    bool m_loggingOn            = false;
    std::string m_logLevel;
};

#endif // MULTIVIDEOLAYER_H
