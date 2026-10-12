/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CPLAY_LAYEREFFECTS_H
#define CPLAY_LAYEREFFECTS_H

#include "easing.h"
#include "layeranimstate.h"

namespace cplay::anim {

enum class EffectPhase : int { Intro = 0, Outro = 1 };

enum class EffectTrigger : int { OnClick = 0, WithPrevious = 1, AfterPrevious = 2 };

// Order is persisted in presentation files; only append.
enum class EffectPreset : int {
    Appear = 0,
    Fade,
    Fly,
    Float,
    Zoom,
    Spin,
    Wipe,
    Iris,
    Clock,
    Dissolve,
    Move,
    Count
};

enum class EffectDirection : int {
    Left = 0, Right, Top, Bottom, TopLeft, TopRight, BottomLeft, BottomRight, Count
};

// Absolute plane placement (degrees and cm), as in the layer's plane parameters.
struct PlanePose {
    float azimuth = 0.f;
    float elevation = 0.f;
    float roll = 0.f;
    float distance = 0.f;
    float horizontal = 0.f;
    float vertical = 0.f;
};

PlanePose lerpPose(const PlanePose &a, const PlanePose &b, float t);

// A PowerPoint-style entrance/exit effect, stored parametrically.
struct LayerEffect {
    int order = 0;
    int phase = static_cast<int>(EffectPhase::Intro);
    int preset = static_cast<int>(EffectPreset::Fade);
    int trigger = static_cast<int>(EffectTrigger::WithPrevious);
    int delayMs = 0;
    int durationMs = 500;
    int easing = static_cast<int>(Easing::OutCubic);
    int direction = static_cast<int>(EffectDirection::Left);
    float feather = 0.1f;
    float amount = 1.f;
    // Move: where the plane ends up; it starts from wherever earlier moves left it,
    // or from start when hasStart is set.
    PlanePose target;
    bool hasStart = false;
    PlanePose start;
};

// Applies one effect at linear progress p in [0,1] of its own time span.
// Move effects depend on the pose before them and are evaluated by the timeline instead.
void applyEffect(const LayerEffect &effect, float progress, LayerAnimState &state);

const char *effectPresetName(int preset);
int effectPresetCount();
// Category for the effect gallery: 0 Basic, 1 Wipe & mask, 2 Motion, 3 Zoom & spin, 4 Move.
int effectPresetCategory(int preset);
// Moves keep the layer visible and carry their end pose into following slides.
bool effectPresetIsMove(int preset);
bool effectPresetUsesDirection(int preset);
bool effectPresetUsesFeather(int preset);

const char *effectDirectionName(int direction);
int effectDirectionCount();

}

#endif // CPLAY_LAYEREFFECTS_H
