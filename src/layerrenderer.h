/*
 * SPDX-FileCopyrightText:
 * 2024-2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LAYERRENDERER_H
#define LAYERRENDERER_H

#include <layers/baselayer.h>
#include <mutex>
#include <sgct/sgct.h>
#include <sgct/utils/dome.h>
#include <sgct/utils/sphere.h>

class LayerRenderer {
public:
    LayerRenderer();
    ~LayerRenderer();

    void initializeGL(double radius, double fov);
    void updateMeshes(double radius, double fov);

    void addLayer(std::shared_ptr<BaseLayer> layer);

    void clearLayers();

    const std::vector<std::shared_ptr<BaseLayer>> &getLayers();

    void renderLayers(const sgct::RenderData &data, int viewMode, float angle);

private:
    std::vector<std::shared_ptr<BaseLayer>> layers2render;

    double meshRadius;
    double meshFov;

    // video
    int videoAlphaLoc;
    int videoEyeModeLoc;
    int videoFlipYLoc;
    int videoStereoscopicModeLoc;
    int videoRoi;
    // mesh
    int meshAlphaLoc;
    int meshEyeModeLoc;
    int meshFlipYLoc;
    int meshMatrixLoc;
    int meshOutsideLoc;
    int meshStereoscopicModeLoc;
    int meshRoi;
    // EAC
    int EACAlphaLoc;
    int EACMatrixLoc;
    int EACScaleLoc;
    int EACOutsideLoc;
    int EACVideoWidthLoc;
    int EACVideoHeightLoc;
    int EACEyeModeLoc;
    int EACStereoscopicModeLoc;

    const sgct::ShaderProgram *videoPrg;
    const sgct::ShaderProgram *meshPrg;
    const sgct::ShaderProgram *EACPrg;

    std::unique_ptr<sgct::utils::Dome> domeMesh;
    std::unique_ptr<sgct::utils::Sphere> sphereMesh;
};

#endif // LAYERRENDERER_H