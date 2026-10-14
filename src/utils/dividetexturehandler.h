/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DIVIDETEXTUREHANDLER_H
#define DIVIDETEXTUREHANDLER_H

#include <cstdint>
#include <memory>
#include <mutex>
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

    // Places the cells, in reading order (top row first), side by side on a
    // mergeCols x mergeRows grid centred on the parent's plane. The parent's specified
    // width/height (or circular width) constrains the complete merge layout:
    // pose, size and stereo/flip come from the parent, each cell gets its offset within the
    // layout. In None/Pre-Split mode each cell fills its normalized destination in
    // the window; other grid modes only share the grid mode.
    // With followMovedCell, a cell changed since the last call (moved through its own grid
    // parameters) first moves the parent, so the layout moves along with it.
    void applyMerge(BaseLayer* parentLayer, int mergeCols, int mergeRows, bool followMovedCell);

private:
    // What applyMerge() last set on a cell, to notice cells changed elsewhere.
    struct MergedCell {
        const BaseLayer* layer = nullptr;
        uint8_t gridMode = 0;
        bool circular = false;
        uint8_t aspectRatio = 0;
        double azimuth = 0.0;
        double elevation = 0.0;
        double roll = 0.0;
        double distance = 0.0;
        double horizontal = 0.0;
        double vertical = 0.0;
        double centreDegrees = 0.0;
        double width = 0.0;
        double height = 0.0;
        double widthDegrees = 0.0;
    };
    static MergedCell readCell(const BaseLayer* cell);
    static bool cellChanged(const MergedCell& applied, const BaseLayer* cell);

    void createSubLayers(BaseLayer* parentLayer);
    void applyRoiForCell(std::shared_ptr<TextureLayer>& sub, int col, int row) const;

    int m_cols = 1;
    int m_rows = 1;
    mutable std::vector<std::shared_ptr<BaseLayer>> m_subLayers;

    // applyMerge() runs on the render thread every frame and on the GUI thread after edits.
    std::mutex m_mergeMutex;
    std::vector<MergedCell> m_merged;
    int m_appliedMergeCols = 1;
    int m_appliedMergeRows = 1;
};

#endif // DIVIDETEXTUREHANDLER_H
