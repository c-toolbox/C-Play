/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CPLAY_EASING_H
#define CPLAY_EASING_H

namespace cplay::anim {

// Order is persisted in presentation files; only append.
enum class Easing : int {
    Linear = 0,
    InQuad, OutQuad, InOutQuad,
    InCubic, OutCubic, InOutCubic,
    InQuart, OutQuart, InOutQuart,
    InQuint, OutQuint, InOutQuint,
    InSine, OutSine, InOutSine,
    InExpo, OutExpo, InOutExpo,
    InCirc, OutCirc, InOutCirc,
    InBack, OutBack, InOutBack,
    InElastic, OutElastic, InOutElastic,
    InBounce, OutBounce, InOutBounce,
    Hold,
    Bezier,
    Count
};

struct BezierHandles {
    float x1 = 0.25f;
    float y1 = 0.1f;
    float x2 = 0.25f;
    float y2 = 1.f;
};

// Maps linear progress t in [0,1] to eased progress. Stateless, so any time can be evaluated.
float ease(Easing type, float t, const BezierHandles &bezier = {});
float ease(int type, float t, const BezierHandles &bezier = {});

// CSS-style cubic-bezier(x1, y1, x2, y2) evaluated at x = t.
float cubicBezier(float t, const BezierHandles &bezier);

const char *easingName(int type);
int easingCount();

}

#endif // CPLAY_EASING_H
