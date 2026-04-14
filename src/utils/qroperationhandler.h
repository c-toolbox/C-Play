/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef QROPERATIONHANDLER_H
#define QROPERATIONHANDLER_H

#include <memory>
#include <string>
#include <vector>

class BaseLayer;
class TextureLayer;
class QROperationConfig;
struct QRCommand;
struct QRPlaneDefinition;

// Reusable handler that implements the QR-driven sublayer scheme
// (SetActive / Clear / Freeze / Unfreeze) on behalf of any parent layer
// that produces a live OpenGL texture (e.g. NdiLayer, StreamLayer).
//
// The parent layer owns an instance of this handler and forwards
// QR commands to it. The handler manages the frozen TextureLayer
// sublayers and the QROperationConfig.
class QROperationHandler {
public:
    explicit QROperationHandler();
    ~QROperationHandler();

    // ---- Configuration ----

    // Load plane configuration from a JSON file. Returns true on success.
    bool loadConfig(const std::string& filePath);

    // Access the operation configuration (may be null if not loaded).
    const QROperationConfig* config() const;

    // ---- Command dispatch ----

    // Process a parsed QR command against the parent layer's live texture.
    // parentLayer: the layer that owns us (used to read grid/render properties)
    // liveTex, liveW, liveH: the current live texture to freeze from
    void handleCommand(const QRCommand& command,
                       BaseLayer* parentLayer,
                       unsigned int liveTex, int liveW, int liveH);

    // ---- Per-frame update ----

    // Called every frame by the parent layer after it has produced a new texture.
    // Copies the parent's live texture into the active (unfrozen) sublayer so that
    // the active plane always displays the latest frame from the parent.
    void updateActiveSubLayer(BaseLayer* parentLayer,
                              unsigned int liveTex, int liveW, int liveH);

    // ---- Sublayer access ----

    // Returns true when QR operations are in progress (sublayers exist).
    // When true, the parent layer should not be rendered directly — only
    // the sublayers should be rendered.
    bool isActive() const;

    bool hasSubLayers() const;
    std::vector<std::shared_ptr<BaseLayer>>& subLayers() const;

    // Name of the currently active (live) plane, or empty if none.
    std::string activePlaneName() const;

    // ---- Direct sublayer operations ----

    // Remove all sublayers and reset internal state so we can start fresh.
    void clearAll();

    // Freeze the parent's current texture into a named sublayer.
    bool freezeToSubLayer(const std::string& name,
                          BaseLayer* parentLayer,
                          unsigned int liveTex, int liveW, int liveH);

    // Mark a named sublayer as unfrozen (it will receive live updates again).
    void unfreezeSubLayer(const std::string& name);

    // Mark all sublayers as unfrozen.
    void unfreezeAll();

private:
    // Apply plane grid properties from config (or parent fallback) to a sublayer.
    void applyPlaneProperties(const std::string& planeName,
                              BaseLayer* parentLayer,
                              std::shared_ptr<TextureLayer>& sub);

    QROperationConfig* m_config = nullptr;
    std::string m_activePlaneName;
    mutable std::vector<std::shared_ptr<BaseLayer>> m_subLayers;
};

#endif // QROPERATIONHANDLER_H
