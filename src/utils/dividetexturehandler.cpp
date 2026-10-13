/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "dividetexturehandler.h"
#include <layers/baselayer.h>
#include <layers/texturelayer.h>
#include <sgct/sgct.h>
#include <algorithm>
#include <cmath>

DivideTextureHandler::DivideTextureHandler() {
}

DivideTextureHandler::~DivideTextureHandler() {
    m_subLayers.clear();
}

bool DivideTextureHandler::setDivision(int cols, int rows, BaseLayer* parent) {
    if (cols < 1 || cols > 3 || rows < 1 || rows > 3) {
        sgct::Log::Error(std::format("DivideTextureHandler::setDivision: Invalid division {}x{}", cols, rows));
        return false;
    }

    if (cols == m_cols && rows == m_rows && !m_subLayers.empty()) {
        return true; // No change needed
    }

    m_cols = cols;
    m_rows = rows;
    m_subLayers.clear();

    if(m_cols + m_rows > 2) {
        // Create sublayers if the division is more than 1x1
        createSubLayers(parent);
    }

    sgct::Log::Info(std::format("DivideTextureHandler::setDivision: Set to {}x{} ({} cells)",
        m_cols, m_rows, cellCount()));
    return true;
}

int DivideTextureHandler::columns() const {
    return m_cols;
}

int DivideTextureHandler::rows() const {
    return m_rows;
}

int DivideTextureHandler::cellCount() const {
    return m_cols * m_rows;
}

bool DivideTextureHandler::isActive() const {
    return (m_cols * m_rows) > 1 && !m_subLayers.empty();
}

std::shared_ptr<TextureLayer> DivideTextureHandler::subLayer(int index) const {
    if (index < 0 || index >= static_cast<int>(m_subLayers.size())) {
        return nullptr;
    }
    return std::dynamic_pointer_cast<TextureLayer>(m_subLayers[index]);
}

std::shared_ptr<TextureLayer> DivideTextureHandler::subLayer(int col, int row) const {
    return subLayer(row * m_cols + col);
}

bool DivideTextureHandler::hasSubLayers() const {
    return !m_subLayers.empty() && (m_cols * m_rows) > 1;
}

std::vector<std::shared_ptr<BaseLayer>>& DivideTextureHandler::subLayers() const {
    return m_subLayers;
}

void DivideTextureHandler::updateSubLayers(BaseLayer* parentLayer,
                                           unsigned int liveTex, int liveW, int liveH) {
    if (liveTex == 0 || liveW <= 0 || liveH <= 0)
        return;

    // Create sublayers on first update if they don't exist yet
    if (m_subLayers.empty() && cellCount() > 1) {
        createSubLayers(parentLayer);
    }

    if (m_subLayers.empty())
        return;

    for (int i = 0; i < static_cast<int>(m_subLayers.size()); ++i) {
        auto texSub = std::dynamic_pointer_cast<TextureLayer>(m_subLayers[i]);
        if (texSub) {
            // Point to the parent's texture directly (no copy)
            texSub->pointToTexture(liveTex, liveW, liveH);

            // Do not override sublayer alpha here - it may have been set
            // independently via the UI. Alpha is set on creation and can
            // be changed per-sublayer through the LayerViewGridParams.

            if (parentLayer->gridMode() == BaseLayer::GridMode::Plane) {
                texSub->updatePlane();
            }
        }
    }
}

void DivideTextureHandler::clearAll() {
    m_subLayers.clear();
    m_cols = 1;
    m_rows = 1;
    sgct::Log::Info("DivideTextureHandler::clearAll: All sublayers removed, reset to 1x1");
}

void DivideTextureHandler::reinitializeFromParent(BaseLayer* parentLayer) {
    m_subLayers.clear();
    if (cellCount() > 1 && parentLayer) {
        createSubLayers(parentLayer);
    }
}

void DivideTextureHandler::createSubLayers(BaseLayer* parentLayer) {
    m_subLayers.clear();
    m_subLayers.reserve(cellCount());

    for (int row = 0; row < m_rows; ++row) {
        for (int col = 0; col < m_cols; ++col) {
            int index = row * m_cols + col;
            std::string name = std::string("Div_") + std::to_string(col) + "_" + std::to_string(row);

            auto sub = std::make_shared<TextureLayer>(name);
            sub->initialize();

            // Copy base render properties from parent
            sub->setGridMode(parentLayer->gridMode());
            sub->setStereoMode(parentLayer->stereoMode());
            sub->setRotate(const_cast<glm::vec3&>(parentLayer->rotate()));
            sub->setTranslate(const_cast<glm::vec3&>(parentLayer->translate()));
            sub->setAlpha(parentLayer->alpha());
            sub->setFlipY(parentLayer->flipY());

            // Set ROI to select this cell's portion of the texture
            applyRoiForCell(sub, col, row);

            // Copy plane properties from parent if in Plane grid mode
            if (parentLayer->gridMode() == BaseLayer::GridMode::Plane) {
                sub->setPlaneAzimuth(parentLayer->planeAzimuth());
                sub->setPlaneElevation(parentLayer->planeElevation());
                sub->setPlaneRoll(parentLayer->planeRoll());
                sub->setPlaneDistance(parentLayer->planeDistance());
                sub->setPlaneCircular(parentLayer->planeCircular());
                sub->setPlaneCentreDegrees(parentLayer->planeCentreDegrees());
                sub->setPlaneWidthDegrees(parentLayer->planeWidthDegrees());
                sub->setPlaneHorizontal(parentLayer->planeHorizontal());
                sub->setPlaneVertical(parentLayer->planeVertical());
                sub->setPlaneSize(glm::vec2(static_cast<float>(parentLayer->planeWidth()),
                                             static_cast<float>(parentLayer->planeHeight())),
                                  parentLayer->planeAspectRatio());
                sub->updatePlane();
            }

            m_subLayers.push_back(sub);

            sgct::Log::Info(std::format("DivideTextureHandler: Created sublayer '{}' (col={}, row={}, index={})",
                name, col, row, index));
        }
    }

    sgct::Log::Info(std::format("DivideTextureHandler: Created {} sublayers for {}x{} division",
        m_subLayers.size(), m_cols, m_rows));
}

void DivideTextureHandler::applyRoiForCell(std::shared_ptr<TextureLayer>& sub, int col, int row) const {
    // ROI is defined as (x, y, width, height) in normalized [0..1] coordinates
    float cellW = 1.f / static_cast<float>(m_cols);
    float cellH = 1.f / static_cast<float>(m_rows);
    float x = static_cast<float>(col) * cellW;
    float y = static_cast<float>(row) * cellH;

    sub->setRoiEnabled(true);
    sub->setRoi(x, y, cellW, cellH);
}

namespace {
constexpr double kMergeEpsilon = 1e-6;

bool differs(double a, double b) {
    return std::abs(a - b) > kMergeEpsilon;
}

// Setters mark the layer for sync, so only call them for an actual change.
void assign(BaseLayer* layer, double (BaseLayer::*get)() const, void (BaseLayer::*set)(double), double value) {
    if (differs((layer->*get)(), value))
        (layer->*set)(value);
}
}

DivideTextureHandler::MergedCell DivideTextureHandler::readCell(const BaseLayer* cell) {
    MergedCell m;
    m.layer = cell;
    m.gridMode = cell->gridMode();
    m.circular = cell->planeCircular();
    m.aspectRatio = cell->planeAspectRatio();
    m.azimuth = cell->planeAzimuth();
    m.elevation = cell->planeElevation();
    m.roll = cell->planeRoll();
    m.distance = cell->planeDistance();
    m.horizontal = cell->planeHorizontal();
    m.vertical = cell->planeVertical();
    m.centreDegrees = cell->planeCentreDegrees();
    m.width = cell->planeWidth();
    m.height = cell->planeHeight();
    m.widthDegrees = cell->planeWidthDegrees();
    return m;
}

bool DivideTextureHandler::cellChanged(const MergedCell& applied, const BaseLayer* cell) {
    const MergedCell now = readCell(cell);
    return now.gridMode != applied.gridMode || now.circular != applied.circular
        || now.aspectRatio != applied.aspectRatio
        || differs(now.azimuth, applied.azimuth) || differs(now.elevation, applied.elevation)
        || differs(now.roll, applied.roll) || differs(now.distance, applied.distance)
        || differs(now.horizontal, applied.horizontal) || differs(now.vertical, applied.vertical)
        || differs(now.centreDegrees, applied.centreDegrees)
        || differs(now.width, applied.width) || differs(now.height, applied.height)
        || differs(now.widthDegrees, applied.widthDegrees);
}

void DivideTextureHandler::applyMerge(BaseLayer* parent, int mergeCols, int mergeRows, bool followMovedCell) {
    std::lock_guard<std::mutex> lock(m_mergeMutex);
    const int count = static_cast<int>(m_subLayers.size());
    if (!parent || count == 0 || count != cellCount() || mergeCols < 1 || mergeRows < 1
        || mergeCols * mergeRows < count)
        return;
    for (const auto& sub : m_subLayers) {
        if (!sub)
            return;
    }

    // Previous state only counts for the same cells (a new division recreates them).
    bool known = static_cast<int>(m_merged.size()) == count;
    for (int i = 0; known && i < count; ++i)
        known = m_merged[i].layer == m_subLayers[i].get();

    // A cell moved through its own parameters becomes the reference for the parent.
    int moved = -1;
    if (followMovedCell && known) {
        for (int i = 0; i < count; ++i) {
            if (cellChanged(m_merged[i], m_subLayers[i].get())) {
                moved = i;
                break;
            }
        }
    }

    if (moved >= 0) {
        const BaseLayer* cell = m_subLayers[moved].get();
        if (parent->gridMode() != cell->gridMode())
            parent->setGridMode(cell->gridMode());
        if (parent->planeCircular() != cell->planeCircular())
            parent->setPlaneCircular(cell->planeCircular());
        if (parent->planeAspectRatio() != cell->planeAspectRatio())
            parent->setPlaneAspectRatio(cell->planeAspectRatio());
        assign(parent, &BaseLayer::planeAzimuth, &BaseLayer::setPlaneAzimuth, cell->planeAzimuth());
        assign(parent, &BaseLayer::planeElevation, &BaseLayer::setPlaneElevation, cell->planeElevation());
        assign(parent, &BaseLayer::planeRoll, &BaseLayer::setPlaneRoll, cell->planeRoll());
        assign(parent, &BaseLayer::planeDistance, &BaseLayer::setPlaneDistance, cell->planeDistance());
        assign(parent, &BaseLayer::planeWidth, &BaseLayer::setPlaneWidth, cell->planeWidth() * m_cols);
        assign(parent, &BaseLayer::planeHeight, &BaseLayer::setPlaneHeight, cell->planeHeight() * m_rows);
        // With aspect ratio 1 a circular plane derives its width from the height instead.
        if (differs(m_merged[moved].widthDegrees, cell->planeWidthDegrees())
            && !(parent->circularPlaneActive() && parent->planeAspectRatio() == 1))
            assign(parent, &BaseLayer::planeWidthDegrees, &BaseLayer::setPlaneWidthDegrees,
                   std::min(cell->planeWidthDegrees() * m_cols, 360.0));
    }

    const uint8_t gridMode = parent->gridMode();
    const bool plane = gridMode == BaseLayer::GridMode::Plane;
    for (auto& sub : m_subLayers) {
        if (sub->gridMode() != gridMode)
            sub->setGridMode(gridMode);
        if (!plane)
            continue;
        if (sub->stereoMode() != parent->stereoMode())
            sub->setStereoMode(parent->stereoMode());
        if (sub->flipY() != parent->flipY())
            sub->setFlipY(parent->flipY());
        if (sub->planeCircular() != parent->planeCircular())
            sub->setPlaneCircular(parent->planeCircular());
        if (sub->planeAspectRatio() != parent->planeAspectRatio())
            sub->setPlaneAspectRatio(parent->planeAspectRatio());
        assign(sub.get(), &BaseLayer::planeWidth, &BaseLayer::setPlaneWidth, parent->planeWidth() / m_cols);
        assign(sub.get(), &BaseLayer::planeHeight, &BaseLayer::setPlaneHeight, parent->planeHeight() / m_rows);
        assign(sub.get(), &BaseLayer::planeWidthDegrees, &BaseLayer::setPlaneWidthDegrees,
               parent->planeWidthDegrees() / m_cols);
    }

    if (plane) {
        // All cells share one size: from the texture once it arrived, the specified one before.
        const bool circular = parent->circularPlaneActive();
        const auto mapping = parent->circularMapping();
        glm::vec2 cellSize(0.f);
        for (const auto& sub : m_subLayers) {
            if (const auto size = sub->computedPlaneSize()) {
                cellSize = *size;
                break;
            }
        }
        if (cellSize.x <= 0.f || cellSize.y <= 0.f) {
            const BaseLayer* sub = m_subLayers[0].get();
            cellSize.x = static_cast<float>(circular ? mapping.widthCm * sub->planeWidthDegrees() / 360.0
                                                     : sub->planeWidth());
            cellSize.y = static_cast<float>(sub->planeHeight());
        }

        // Offset of a cell from the layout centre, in cm on the plane (x right, y up).
        // The plane mesh has texture row 0 at the bottom, so the top row of the picture is
        // the first cell row when flipped and the last one otherwise.
        const bool topRowFirst = parent->flipY();
        auto offset = [&](int index) {
            const int col = index % m_cols;
            const int row = index / m_cols;
            const int readingIndex = (topRowFirst ? row : m_rows - 1 - row) * m_cols + col;
            const int mergedCol = readingIndex % mergeCols;
            const int mergedRow = readingIndex / mergeCols;
            return glm::dvec2((mergedCol + 0.5 - mergeCols * 0.5) * cellSize.x,
                              (mergeRows * 0.5 - mergedRow - 0.5) * cellSize.y);
        };
        // On a circular plane the horizontal position is the centre angle.
        auto degrees = [&](double cm) { return mapping.direction() * cm * 360.0 / mapping.widthCm; };

        if (moved >= 0) {
            const BaseLayer* cell = m_subLayers[moved].get();
            const glm::dvec2 o = offset(moved);
            if (circular)
                assign(parent, &BaseLayer::planeCentreDegrees, &BaseLayer::setPlaneCentreDegrees,
                       cell->planeCentreDegrees() - degrees(o.x));
            else
                assign(parent, &BaseLayer::planeHorizontal, &BaseLayer::setPlaneHorizontal, cell->planeHorizontal() - o.x);
            assign(parent, &BaseLayer::planeVertical, &BaseLayer::setPlaneVertical, cell->planeVertical() - o.y);
        }

        for (int i = 0; i < count; ++i) {
            BaseLayer* sub = m_subLayers[i].get();
            const glm::dvec2 o = offset(i);
            assign(sub, &BaseLayer::planeAzimuth, &BaseLayer::setPlaneAzimuth, parent->planeAzimuth());
            assign(sub, &BaseLayer::planeElevation, &BaseLayer::setPlaneElevation, parent->planeElevation());
            assign(sub, &BaseLayer::planeRoll, &BaseLayer::setPlaneRoll, parent->planeRoll());
            assign(sub, &BaseLayer::planeDistance, &BaseLayer::setPlaneDistance, parent->planeDistance());
            if (circular) {
                assign(sub, &BaseLayer::planeCentreDegrees, &BaseLayer::setPlaneCentreDegrees,
                       parent->planeCentreDegrees() + degrees(o.x));
                assign(sub, &BaseLayer::planeHorizontal, &BaseLayer::setPlaneHorizontal, parent->planeHorizontal());
            } else {
                assign(sub, &BaseLayer::planeCentreDegrees, &BaseLayer::setPlaneCentreDegrees, parent->planeCentreDegrees());
                assign(sub, &BaseLayer::planeHorizontal, &BaseLayer::setPlaneHorizontal, parent->planeHorizontal() + o.x);
            }
            assign(sub, &BaseLayer::planeVertical, &BaseLayer::setPlaneVertical, parent->planeVertical() + o.y);
        }
    }

    m_merged.resize(count);
    for (int i = 0; i < count; ++i)
        m_merged[i] = readCell(m_subLayers[i].get());
}
