/*
 * SPDX-FileCopyrightText:
 * 2024 Erik Sund�n <eriksunden85@gmail.com>
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

    void initialize(double radius, double fov);
    void updateMeshes(double radius, double fov);

    void addLayer(BaseLayer *layer);

    void clearLayers();

    const std::vector<BaseLayer *> &getLayers();

    void renderLayers(const sgct::RenderData &data, int viewMode, float angle);

private:
    std::vector<BaseLayer *> layers2render;

    double meshRadius;
    double meshFov;

    // video
    int videoAlphaLoc;
    int videoEyeModeLoc;
    int videoStereoscopicModeLoc;
    int videoRoi;
    // mesh
    int meshAlphaLoc;
    int meshEyeModeLoc;
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