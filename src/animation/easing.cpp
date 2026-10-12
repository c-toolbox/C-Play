/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "easing.h"
#include <algorithm>
#include <cmath>

namespace cplay::anim {

namespace {

constexpr float kPi = 3.14159265358979323846f;

float outBounce(float t) {
    constexpr float n1 = 7.5625f;
    constexpr float d1 = 2.75f;
    if (t < 1.f / d1)
        return n1 * t * t;
    if (t < 2.f / d1) {
        t -= 1.5f / d1;
        return n1 * t * t + 0.75f;
    }
    if (t < 2.5f / d1) {
        t -= 2.25f / d1;
        return n1 * t * t + 0.9375f;
    }
    t -= 2.625f / d1;
    return n1 * t * t + 0.984375f;
}

float inOut(float t, float (*in)(float)) {
    return t < 0.5f ? 0.5f * in(2.f * t) : 1.f - 0.5f * in(2.f - 2.f * t);
}

float inQuad(float t) { return t * t; }
float inCubic(float t) { return t * t * t; }
float inQuart(float t) { return t * t * t * t; }
float inQuint(float t) { return t * t * t * t * t; }
float inSine(float t) { return 1.f - std::cos(t * kPi * 0.5f); }
float inExpo(float t) { return t <= 0.f ? 0.f : std::pow(2.f, 10.f * t - 10.f); }
float inCirc(float t) { return 1.f - std::sqrt(std::max(0.f, 1.f - t * t)); }
float inBack(float t) {
    constexpr float c1 = 1.70158f;
    constexpr float c3 = c1 + 1.f;
    return c3 * t * t * t - c1 * t * t;
}
float inElastic(float t) {
    if (t <= 0.f || t >= 1.f)
        return t;
    constexpr float c4 = (2.f * kPi) / 3.f;
    return -std::pow(2.f, 10.f * t - 10.f) * std::sin((t * 10.f - 10.75f) * c4);
}
float inBounce(float t) { return 1.f - outBounce(1.f - t); }

float out(float t, float (*in)(float)) { return 1.f - in(1.f - t); }

const char *const kNames[] = {
    "Linear",
    "Ease in (quad)", "Ease out (quad)", "Ease in-out (quad)",
    "Ease in (cubic)", "Ease out (cubic)", "Ease in-out (cubic)",
    "Ease in (quart)", "Ease out (quart)", "Ease in-out (quart)",
    "Ease in (quint)", "Ease out (quint)", "Ease in-out (quint)",
    "Ease in (sine)", "Ease out (sine)", "Ease in-out (sine)",
    "Ease in (expo)", "Ease out (expo)", "Ease in-out (expo)",
    "Ease in (circ)", "Ease out (circ)", "Ease in-out (circ)",
    "Back in", "Back out", "Back in-out",
    "Elastic in", "Elastic out", "Elastic in-out",
    "Bounce in", "Bounce out", "Bounce in-out",
    "Hold",
    "Custom bezier"
};
static_assert(sizeof(kNames) / sizeof(kNames[0]) == static_cast<int>(Easing::Count));

}

float cubicBezier(float t, const BezierHandles &b) {
    t = std::clamp(t, 0.f, 1.f);
    const float x1 = std::clamp(b.x1, 0.f, 1.f);
    const float x2 = std::clamp(b.x2, 0.f, 1.f);
    auto bez = [](float a1, float a2, float s) {
        const float inv = 1.f - s;
        return 3.f * inv * inv * s * a1 + 3.f * inv * s * s * a2 + s * s * s;
    };
    auto bezDx = [](float a1, float a2, float s) {
        const float inv = 1.f - s;
        return 3.f * inv * inv * a1 + 6.f * inv * s * (a2 - a1) + 3.f * s * s * (1.f - a2);
    };
    // Newton-Raphson with bisection fallback to solve x(s) = t.
    float s = t;
    for (int i = 0; i < 8; ++i) {
        const float dx = bezDx(x1, x2, s);
        if (std::abs(dx) < 1e-6f)
            break;
        const float err = bez(x1, x2, s) - t;
        if (std::abs(err) < 1e-6f)
            return bez(b.y1, b.y2, s);
        s = std::clamp(s - err / dx, 0.f, 1.f);
    }
    float lo = 0.f, hi = 1.f;
    s = t;
    for (int i = 0; i < 30; ++i) {
        const float x = bez(x1, x2, s);
        if (std::abs(x - t) < 1e-6f)
            break;
        if (x < t)
            lo = s;
        else
            hi = s;
        s = 0.5f * (lo + hi);
    }
    return bez(b.y1, b.y2, s);
}

float ease(Easing type, float t, const BezierHandles &bezier) {
    t = std::clamp(t, 0.f, 1.f);
    switch (type) {
    case Easing::Linear: return t;
    case Easing::InQuad: return inQuad(t);
    case Easing::OutQuad: return out(t, inQuad);
    case Easing::InOutQuad: return inOut(t, inQuad);
    case Easing::InCubic: return inCubic(t);
    case Easing::OutCubic: return out(t, inCubic);
    case Easing::InOutCubic: return inOut(t, inCubic);
    case Easing::InQuart: return inQuart(t);
    case Easing::OutQuart: return out(t, inQuart);
    case Easing::InOutQuart: return inOut(t, inQuart);
    case Easing::InQuint: return inQuint(t);
    case Easing::OutQuint: return out(t, inQuint);
    case Easing::InOutQuint: return inOut(t, inQuint);
    case Easing::InSine: return inSine(t);
    case Easing::OutSine: return out(t, inSine);
    case Easing::InOutSine: return inOut(t, inSine);
    case Easing::InExpo: return inExpo(t);
    case Easing::OutExpo: return t >= 1.f ? 1.f : out(t, inExpo);
    case Easing::InOutExpo: return inOut(t, inExpo);
    case Easing::InCirc: return inCirc(t);
    case Easing::OutCirc: return out(t, inCirc);
    case Easing::InOutCirc: return inOut(t, inCirc);
    case Easing::InBack: return inBack(t);
    case Easing::OutBack: return out(t, inBack);
    case Easing::InOutBack: return inOut(t, inBack);
    case Easing::InElastic: return inElastic(t);
    case Easing::OutElastic: return out(t, inElastic);
    case Easing::InOutElastic: return inOut(t, inElastic);
    case Easing::InBounce: return inBounce(t);
    case Easing::OutBounce: return outBounce(t);
    case Easing::InOutBounce: return inOut(t, inBounce);
    case Easing::Hold: return t >= 1.f ? 1.f : 0.f;
    case Easing::Bezier: return cubicBezier(t, bezier);
    case Easing::Count: break;
    }
    return t;
}

float ease(int type, float t, const BezierHandles &bezier) {
    if (type < 0 || type >= static_cast<int>(Easing::Count))
        type = 0;
    return ease(static_cast<Easing>(type), t, bezier);
}

const char *easingName(int type) {
    if (type < 0 || type >= static_cast<int>(Easing::Count))
        return kNames[0];
    return kNames[type];
}

int easingCount() {
    return static_cast<int>(Easing::Count);
}

}
