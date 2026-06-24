/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MULTIVIDEOHANDLER_H
#define MULTIVIDEOHANDLER_H

#include <memory>
#include <string>
#include <vector>
#include <layers/baselayer.h>
#include <utils/multivideoconfig.h>
#include <utils/nodeidentityconfig.h>

// Forward declaration for MultiVideoMasterParams
struct MultiVideoMasterParams;

// Manages a set of VideoLayer sub-players on a render node.
// Each sub-player corresponds to one entry in a MultiVideoConfig and
// plays its assigned file with the configured grid/stereo/eye mapping.
//
// The owning MultiVideoLayer creates this handler and calls:
//   1. setConfig() when a new composition JSON is decoded on the node.
//   2. updateSubLayers() each frame (inside updateFrame) to pump mpv.
//   3. applyTime/applyPause/applyEofMode/applyLoopTime/applyValue to
//      keep all sub-players time-synced to the master reference.
//   4. clearAll() on destruction or when switching back to single-video.
class MultiVideoHandler {
public:
    MultiVideoHandler();
    ~MultiVideoHandler();

    // Build (or rebuild) sub-players from a config + node identity.
    // nodeId:  the identifier of this node (from NodeIdentityConfig);
    //          used to look up per-node file paths in each MultiVideoEntry.
    // opa:     OpenGL proc-address function forwarded to each VideoLayer.
    // The master parameters (gridMode, stereoMode, audio, eofMode) from the config
    // are applied as defaults to all sub-players before per-entry overrides.
    void setConfig(const MultiVideoConfig& config,
                   const std::string& nodeId,
                   BaseLayer::gl_adress_func_v1 opa,
                   bool allowDirectRendering,
                   bool loggingOn,
                   const std::string& logLevel);

    // Whether at least one sub-layer was successfully created.
    bool isActive() const;

    // Whether sub-layers are available (delegates to isActive).
    bool hasSubLayers() const;

    // Access the sub-layer list for the renderer.
    std::vector<std::shared_ptr<BaseLayer>>& subLayers() const;

    // Pump mpv for every sub-layer (call each frame inside updateFrame).
    void updateSubLayers();

    // Propagate time/playback state to all sub-players.
    void applyTime(double pos, bool dirty);
    void applyPause(bool paused);
    void applyEofMode(int eofMode);
    void applyLoopTime(double A, double B, bool enabled);
    void applyValue(const std::string& param, int val);

    // Stop, cleanup, and remove all sub-players.
    void clearAll();

private:
    mutable std::vector<std::shared_ptr<BaseLayer>> m_subLayers;
    bool m_active = false;
};

#endif // MULTIVIDEOHANDLER_H
