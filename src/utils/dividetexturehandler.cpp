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
