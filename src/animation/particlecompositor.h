/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CPLAY_PARTICLECOMPOSITOR_H
#define CPLAY_PARTICLECOMPOSITOR_H

#include "layeranimstate.h"
#include <cstdint>
#include <map>

class BaseLayer;

namespace cplay::anim {

// Renders particle entrance/exit effects (LayerAnimState::particleMode). The layer's texture
// is cut into a grid of particles that are drawn, scattered, into a texture of the same
// layout, which the renderer then shows instead of the layer's own texture. Every render
// path (screen quad, plane, dome, sphere, fisheye lens) thereby gets the effect unchanged.
//
// The particles move in the space the layer's grid shows the texture in: across the image for
// a flat quad or plane, and by rotations around the viewer for the dome (equidistant fisheye,
// zenith at the centre) and the equirectangular sphere, so they keep their shape on the shown
// surface. EAC sphere textures are treated as flat images.
// Each stereo eye (and the region of interest within it) is a separate particle field.
//
// OpenGL objects are not shared between all contexts, so state is kept per context key.
class ParticleCompositor {
public:
    ParticleCompositor() = default;
    ~ParticleCompositor();
    ParticleCompositor(const ParticleCompositor &) = delete;
    ParticleCompositor &operator=(const ParticleCompositor &) = delete;

    // Texture to sample for the layer: its particle image while a particle effect plays,
    // otherwise layer->textureId(). Renders at most once per frame number and layer.
    // gridMode: the BaseLayer::GridMode the layer is drawn with.
    // context: identifies the current OpenGL context. Restores the OpenGL state it changes.
    unsigned int layerTexture(const BaseLayer *layer, int gridMode, int stereoMode, float domeFovDeg,
                              const void *context, uint64_t frame);

    // After a frame's layers: frees the textures of layers that stopped animating.
    void endFrame(const void *context);

    // Frees everything created in the context, which must be current.
    void release(const void *context);

private:
    struct Target {
        unsigned int fbo = 0;
        unsigned int texture = 0;
        int width = 0;
        int height = 0;
        uint64_t frame = 0;
        bool rendered = false;
        int idleFrames = 0;
        bool usedThisFrame = false;
    };
    struct Context {
        unsigned int program = 0;
        unsigned int vao = 0;
        bool failed = false;
        int locCells = -1;
        int locRegion = -1;
        int locMode = -1;
        int locProgress = -1;
        int locSpread = -1;
        int locStagger = -1;
        int locSeed = -1;
        int locHalfFov = -1;
        int locAspect = -1;
        int locCopyShift = -1;
        std::map<const BaseLayer *, Target> targets;
    };

    bool ensureProgram(Context &ctx);
    static void releaseTarget(Target &t);
    void render(Context &ctx, Target &t, unsigned int source, int width, int height,
                const LayerAnimState &a, int gridMode, int stereoMode, bool roiEnabled,
                const glm::vec4 &roi, float domeFovDeg);

    std::map<const void *, Context> m_contexts;
};

}

#endif // CPLAY_PARTICLECOMPOSITOR_H
