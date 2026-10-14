/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CPLAY_MASKGLSL_H
#define CPLAY_MASKGLSL_H

#include "layeranimstate.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

// Animated transition masks shared by all textured fragment shaders. A macro so it can be
// concatenated with the surrounding raw string literals at compile time.
#define CPLAY_MASK_GLSL R"(
uniform int maskType;
uniform float maskProgress;
uniform float maskFeather;
uniform float maskAngle;
uniform vec2 maskCenter;
uniform float maskAspect;
uniform bool maskInvert;

float cplayMaskHash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float cplayMask(vec2 uv) {
    if (maskType == 0)
        return 1.0;
    const float PI = 3.14159265358979;
    float f = max(maskFeather, 0.0001);
    float v = 0.0;
    if (maskType == 1) {
        vec2 d = vec2(cos(maskAngle), sin(maskAngle));
        float lo = min(0.0, d.x) + min(0.0, d.y);
        float hi = max(0.0, d.x) + max(0.0, d.y);
        v = (dot(uv, d) - lo) / max(hi - lo, 0.0001);
    }
    else if (maskType == 2) {
        vec2 s = vec2(maskAspect, 1.0);
        float maxD = max(max(length((vec2(0.0, 0.0) - maskCenter) * s), length((vec2(1.0, 0.0) - maskCenter) * s)),
                         max(length((vec2(0.0, 1.0) - maskCenter) * s), length((vec2(1.0, 1.0) - maskCenter) * s)));
        v = length((uv - maskCenter) * s) / max(maxD, 0.0001);
    }
    else if (maskType == 3) {
        vec2 q = (uv - maskCenter) * vec2(maskAspect, 1.0);
        v = fract((atan(q.x, q.y) - maskAngle) / (2.0 * PI) + 1.0);
    }
    else {
        v = cplayMaskHash(floor(uv * vec2(512.0 * maskAspect, 512.0)));
    }
    float t = clamp(maskProgress, 0.0, 1.0) * (1.0 + f);
    float m = 1.0 - smoothstep(t - f, t, v);
    return maskInvert ? 1.0 - m : m;
}
)"

namespace cplay::anim {

// Transform for the flat (screen quad) render path, in normalized device coordinates.
inline glm::mat4 flatQuadMatrix(const LayerAnimState &a, bool active, float viewportAspect,
                               const glm::vec4 &screenRect = glm::vec4(0.f, 0.f, 1.f, 1.f)) {
    glm::mat4 m(1.f);
    if (active) {
        const float aspect = viewportAspect > 0.f && std::isfinite(viewportAspect) ? viewportAspect : 1.f;
        m = glm::translate(m, glm::vec3(a.move * 2.f, 0.f));
        if (a.spin != 0.f) {
            m = glm::scale(m, glm::vec3(1.f / aspect, 1.f, 1.f));
            m = glm::rotate(m, glm::radians(a.spin), glm::vec3(0.f, 0.f, 1.f));
            m = glm::scale(m, glm::vec3(aspect, 1.f, 1.f));
        }
        m = glm::scale(m, glm::vec3(a.scale, a.scale, 1.f));
    }
    // Map the full NDC quad into the cell's destination rectangle. Animation then
    // transforms the resulting layout in window coordinates.
    m = glm::translate(m, glm::vec3(2.f * screenRect.x + screenRect.z - 1.f,
                                  2.f * screenRect.y + screenRect.w - 1.f, 0.f));
    m = glm::scale(m, glm::vec3(screenRect.z, screenRect.w, 1.f));
    return m;
}

struct MaskUniformLocations {
    int type = -1;
    int progress = -1;
    int feather = -1;
    int angle = -1;
    int center = -1;
    int aspect = -1;
    int invert = -1;
    int quadMatrix = -1;

    template <typename LocateFn>
    void locate(LocateFn &&fn) {
        type = fn("maskType");
        progress = fn("maskProgress");
        feather = fn("maskFeather");
        angle = fn("maskAngle");
        center = fn("maskCenter");
        aspect = fn("maskAspect");
        invert = fn("maskInvert");
        quadMatrix = fn("quadMatrix");
    }

    // Setter must provide set1i(loc, int), set1f(loc, float) and set2f(loc, float, float).
    template <typename Setter>
    void apply(Setter &&s, const LayerAnimState &a, float aspectRatio) const {
        s.set1i(type, a.maskType);
        if (a.maskType == 0)
            return;
        s.set1f(progress, a.maskProgress);
        s.set1f(feather, a.maskFeather);
        s.set1f(angle, a.maskAngle);
        s.set2f(center, a.maskCenter.x, a.maskCenter.y);
        s.set1f(aspect, aspectRatio > 0.f && std::isfinite(aspectRatio) ? aspectRatio : 1.f);
        s.set1i(invert, a.maskInvert);
    }
};

}

#endif // CPLAY_MASKGLSL_H
