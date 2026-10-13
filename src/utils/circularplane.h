/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

namespace cplay::circular {
struct Mapping {
    bool enabled = false;
    double widthCm = 1480.0;
    double seamDegrees = 0.0;
    bool clockwise = true;
    double direction() const { return clockwise ? 1.0 : -1.0; }
    bool valid() const { return enabled && std::isfinite(widthCm) && widthCm > 0.0 && std::isfinite(seamDegrees); }
    double angleToCm(double angle) const {
        return direction() * (angle - seamDegrees) * widthCm / 360.0 - widthCm / 2.0;
    }
    double cmToAngle(double x) const {
        return seamDegrees + direction() * (x + widthCm / 2.0) * 360.0 / widthCm;
    }
    double wrapCm(double x) const {
        const double phase = (x + widthCm / 2.0) / widthCm;
        return (phase - std::floor(phase)) * widthCm - widthCm / 2.0;
    }
};
struct Copy {
    glm::mat4 model;
    glm::vec2 sourceBounds;
};
// Models are in the common, un-tilted projection-plane frame (metres).
// Duplicate only portions intersecting the logical canvas, independently of
// output viewport size. Limit coverage to one revolution to avoid double alpha.
inline std::vector<Copy> copies(const glm::mat4 &model, glm::vec2 sizeCm, double widthCm) {
    if (!std::isfinite(widthCm) || widthCm <= 0 || !std::isfinite(sizeCm.x) || !std::isfinite(sizeCm.y)) return {};
    const float period = static_cast<float>(widthCm / 100.0);
    const float half = period / 2.f;
    const float centre = (model * glm::vec4(0, 0, 0, 1)).x;
    if (!std::isfinite(centre)) return {};
    float left = centre, right = centre;
    for (float x : {-sizeCm.x / 200.f, sizeCm.x / 200.f})
        for (float y : {-sizeCm.y / 200.f, sizeCm.y / 200.f}) {
            const float px = (model * glm::vec4(x, y, 0, 1)).x;
            left = std::min(left, px); right = std::max(right, px);
        }
    left = std::max(left, centre - half);
    right = std::min(right, centre + half);
    std::vector<Copy> result;
    for (int offset : {0, -1, 1}) {
        const float shift = offset * period;
        if (right + shift <= -half || left + shift >= half) continue;
        result.push_back({glm::translate(glm::mat4(1.f), glm::vec3(shift, 0, 0)) * model,
                          glm::vec2(centre - half + shift, centre + half + shift)});
    }
    return result;
}
}
