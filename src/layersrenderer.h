/*
 * SPDX-FileCopyrightText:
 * 2024-2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LAYERSRENDERER_H
#define LAYERSRENDERER_H

#include <layers/baselayer.h>
#include <animation/maskglsl.h>
#include <array>
#include <mutex>
#include <sgct/sgct.h>
#include <utils/domegrid.h>
#include <utils/spheregrid.h>

class LayersRenderer {
public:
    LayersRenderer();
    ~LayersRenderer();

    void initializeGL(double radius, double fov);
    void updateMeshes(double radius, double fov);

    void addLayer(std::shared_ptr<BaseLayer> layer);

    void clearLayers();

    const std::vector<std::shared_ptr<BaseLayer>> &getLayers();

    // fxSource: layer whose post effects apply (a sublayer uses its parent's), nullptr = layer.
    void renderLayer(const sgct::RenderData& data, const BaseLayer* layer, sgct::FrustumMode currentEye, float angle,
                     const BaseLayer* fxSource = nullptr);
    void renderLayers(const sgct::RenderData& data, int viewMode, float angle);

private:
    std::vector<std::shared_ptr<BaseLayer>> layers2render;

    double meshRadius;
    double meshFov;

    // Shader programs and their uniform locations, in two variants: [0] renders layers
    // without post effects exactly as before, [1] adds the post effects and is only used
    // for layers with an effect turned on (see cplay::anim::postGlsl()).
    struct ShaderVariant {
        // video
        int videoAlphaLoc = -1;
        int videoEyeModeLoc = -1;
        int videoFlipYLoc = -1;
        int videoStereoscopicModeLoc = -1;
        int videoRoi = -1;
        // mesh
        int meshAlphaLoc = -1;
        int meshEyeModeLoc = -1;
        int meshFlipYLoc = -1;
        int meshMatrixLoc = -1;
        int meshOutsideLoc = -1;
        int meshStereoscopicModeLoc = -1;
        int meshRoi = -1;
        // EAC
        int EACAlphaLoc = -1;
        int EACFlipYLoc = -1;
        int EACMatrixLoc = -1;
        int EACScaleLoc = -1;
        int EACOutsideLoc = -1;
        int EACVideoWidthLoc = -1;
        int EACVideoHeightLoc = -1;
        int EACEyeModeLoc = -1;
        int EACStereoscopicModeLoc = -1;

        cplay::anim::MaskUniformLocations videoMaskLocs;
        cplay::anim::MaskUniformLocations meshMaskLocs;
        cplay::anim::MaskUniformLocations EACMaskLocs;

        const sgct::ShaderProgram *videoPrg = nullptr;
        const sgct::ShaderProgram *meshPrg = nullptr;
        const sgct::ShaderProgram *EACPrg = nullptr;
    };
    std::array<ShaderVariant, 2> variants;
    // Variant in use: the plain one, except while renderLayer() draws a layer with post effects.
    ShaderVariant *sv = &variants[0];

    std::unique_ptr<DomeGrid> domeMesh;
    std::unique_ptr<SphereGrid> sphereMesh;
};

#endif // LAYERSRENDERER_H