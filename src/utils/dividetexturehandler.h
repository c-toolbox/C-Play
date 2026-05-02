/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DIVIDETEXTUREHANDLER_H
#define DIVIDETEXTUREHANDLER_H

#include <memory>
#include <string>
#include <vector>

class BaseLayer;
class TextureLayer;

// Handler that divides a parent layer's texture into a grid of sublayers.
// Supported divisions: 1x1, 1x2, 2x1, 1x3, 3x1, 2x2, 2x3, 3x2, 3x3.
// Each sublayer is a TextureLayer that displays a portion of the source
// texture using ROI (region of interest). Each sublayer can have its own
// grid mode, plane parameters, rotation, translation, alpha, etc.
//
// The parent layer (NdiLayer, StreamLayer, SpoutLayer, OmtLayer) owns
// an instance of this handler and calls updateSubLayers() each frame
// after producing a new texture.
class DivideTextureHandler {
public:
    DivideTextureHandler();
    ~DivideTextureHandler();

    // Set the division layout as columns x rows (e.g. 2x3 = 2 cols, 3 rows).
    // Recreates all sublayers. Valid ranges: cols [1..3], rows [1..3].
    // Returns true if the division was applied successfully.
    bool setDivision(int cols, int rows, BaseLayer* parent = nullptr);

    // Current division.
    int columns() const;
    int rows() const;

    // Total number of sublayer cells (cols * rows).
    int cellCount() const;

    // Whether division is active (more than 1 cell).
    bool isActive() const;

    // Access individual sublayer by index (row-major: index = row * cols + col).
    // Returns nullptr if index is out of range or handler is not initialized.
    std::shared_ptr<TextureLayer> subLayer(int index) const;
    std::shared_ptr<TextureLayer> subLayer(int col, int row) const;

    // Access all sublayers as BaseLayer pointers (for rendering).
    bool hasSubLayers() const;
    std::vector<std::shared_ptr<BaseLayer>>& subLayers() const;

    // Called every frame by the parent layer after producing a new texture.
    // Copies the parent's live texture into each sublayer and updates their ROI.
    // parentLayer: the owning layer (used to read default render properties)
    // liveTex, liveW, liveH: the current live texture
    void updateSubLayers(BaseLayer* parentLayer,
                         unsigned int liveTex, int liveW, int liveH);

    // Remove all sublayers and reset to 1x1 (no division).
    void clearAll();

    // Re-initialize sublayers from the parent layer's current properties.
    // Call this after changing division or when parent properties change.
    void reinitializeFromParent(BaseLayer* parentLayer);

private:
    void createSubLayers(BaseLayer* parentLayer);
    void applyRoiForCell(std::shared_ptr<TextureLayer>& sub, int col, int row) const;

    int m_cols = 1;
    int m_rows = 1;
    mutable std::vector<std::shared_ptr<BaseLayer>> m_subLayers;
};

#endif // DIVIDETEXTUREHANDLER_H
