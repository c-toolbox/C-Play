/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CPLAY_LAYERANIMSTATE_H
#define CPLAY_LAYERANIMSTATE_H

#include <glm/glm.hpp>
#include <cstdint>

namespace cplay::anim {

enum class MaskType : int32_t {
    None = 0,
    LinearWipe,
    Iris,
    Clock,
    Dissolve
};

// Render-time modifiers produced by the layer timeline. They are applied on top of the
// layer's own (saved) parameters and are never written back to them.
struct LayerAnimState {
    float alphaMul = 1.f;
    // Layer-relative movement: 1.0 equals one layer width/height.
    glm::vec2 move = glm::vec2(0.f);
    float scale = 1.f;
    // Degrees around the viewing axis.
    float spin = 0.f;
    glm::vec3 rotateOffset = glm::vec3(0.f);
    glm::vec3 translateOffset = glm::vec3(0.f);
    // Azimuth, elevation, roll (degrees).
    glm::vec3 planeAngleOffset = glm::vec3(0.f);
    // Horizontal: degrees for circular planes, otherwise cm. Vertical/distance: cm.
    glm::vec3 planePosOffset = glm::vec3(0.f);

    int32_t maskType = 0;
    float maskProgress = 1.f;
    float maskFeather = 0.1f;
    // Radians; wipe direction or clock start angle.
    float maskAngle = 0.f;
    glm::vec2 maskCenter = glm::vec2(0.5f);
    int32_t maskInvert = 0;

    // Particle entrance/exit: the layer's texture is cut into particles that are scattered
    // around the view, moving as the layer's grid (flat, dome or sphere) shows them. 0 = off.
    int32_t particleMode = 0;
    // How assembled the image is: 0 scattered (and gone), 1 complete.
    float particleProgress = 1.f;
    float particleSpread = 1.f;
    // Share of the effect over which particle start times are spread out, 0..1.
    float particleStagger = 0.5f;
    int32_t particleColumns = 32;
    int32_t particleSeed = 0;

    bool hasTransform() const {
        return alphaMul != 1.f || move != glm::vec2(0.f) || scale != 1.f || spin != 0.f
            || rotateOffset != glm::vec3(0.f) || translateOffset != glm::vec3(0.f)
            || planeAngleOffset != glm::vec3(0.f) || planePosOffset != glm::vec3(0.f);
    }
    // The mask and particle parameters only apply while these are on.
    bool hasMask() const { return maskType != 0; }
    bool hasParticles() const { return particleMode != 0; }

    bool isIdentity() const { return !hasTransform() && !hasMask() && !hasParticles(); }
};

}

#endif // CPLAY_LAYERANIMSTATE_H
