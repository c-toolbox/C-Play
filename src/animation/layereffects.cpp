/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "layereffects.h"
#include <algorithm>
#include <cmath>

namespace cplay::anim {

namespace {

// Unit vector pointing to the side the effect comes from (y up).
glm::vec2 directionVector(int direction) {
    constexpr float d = 0.70710678f;
    switch (static_cast<EffectDirection>(direction)) {
    case EffectDirection::Left: return {-1.f, 0.f};
    case EffectDirection::Right: return {1.f, 0.f};
    case EffectDirection::Top: return {0.f, 1.f};
    case EffectDirection::Bottom: return {0.f, -1.f};
    case EffectDirection::TopLeft: return {-d, d};
    case EffectDirection::TopRight: return {d, d};
    case EffectDirection::BottomLeft: return {-d, -d};
    case EffectDirection::BottomRight: return {d, -d};
    case EffectDirection::Count: break;
    }
    return {-1.f, 0.f};
}

void setMask(LayerAnimState &state, MaskType type, float progress, float feather, float angle) {
    state.maskType = static_cast<int32_t>(type);
    state.maskProgress = std::clamp(progress, 0.f, 1.f);
    state.maskFeather = std::clamp(feather, 0.f, 1.f);
    state.maskAngle = angle;
    state.maskCenter = glm::vec2(0.5f);
    state.maskInvert = 0;
}

const char *const kPresetNames[] = {
    "Appear", "Fade", "Fly", "Float", "Zoom", "Spin", "Wipe", "Iris", "Clock", "Dissolve", "Move"
};
static_assert(sizeof(kPresetNames) / sizeof(kPresetNames[0]) == static_cast<int>(EffectPreset::Count));

const char *const kDirectionNames[] = {
    "Left", "Right", "Top", "Bottom", "Top left", "Top right", "Bottom left", "Bottom right"
};
static_assert(sizeof(kDirectionNames) / sizeof(kDirectionNames[0]) == static_cast<int>(EffectDirection::Count));

}

PlanePose lerpPose(const PlanePose &a, const PlanePose &b, float t) {
    auto mix = [t](float x, float y) { return x + t * (y - x); };
    return {mix(a.azimuth, b.azimuth), mix(a.elevation, b.elevation), mix(a.roll, b.roll),
            mix(a.distance, b.distance), mix(a.horizontal, b.horizontal), mix(a.vertical, b.vertical)};
}

void applyEffect(const LayerEffect &effect, float progress, LayerAnimState &state) {
    progress = std::clamp(progress, 0.f, 1.f);
    const bool intro = effect.phase == static_cast<int>(EffectPhase::Intro);
    const float eased = ease(effect.easing, progress);
    // How much of the layer's normal state is shown: 0 hidden/displaced, 1 normal.
    const float v = intro ? eased : 1.f - eased;
    const glm::vec2 dir = directionVector(effect.direction);
    const float amount = std::max(effect.amount, 0.f);

    switch (static_cast<EffectPreset>(effect.preset)) {
    case EffectPreset::Appear:
        if (intro ? progress <= 0.f : progress >= 1.f)
            state.alphaMul = 0.f;
        break;
    case EffectPreset::Fade:
        state.alphaMul *= std::clamp(v, 0.f, 1.f);
        break;
    case EffectPreset::Fly:
        state.move += dir * (1.f - v) * amount;
        break;
    case EffectPreset::Float:
        state.move += dir * (1.f - v) * 0.15f * amount;
        state.alphaMul *= std::clamp(v, 0.f, 1.f);
        break;
    case EffectPreset::Zoom:
        state.scale *= std::max(0.f, 1.f - (1.f - v) * std::min(amount, 1.f));
        state.alphaMul *= std::clamp(v, 0.f, 1.f);
        break;
    case EffectPreset::Spin: {
        const float sign = dir.x > 0.f ? -1.f : 1.f;
        state.spin += sign * (1.f - v) * 360.f * amount;
        state.alphaMul *= std::clamp(v, 0.f, 1.f);
        break;
    }
    case EffectPreset::Wipe: {
        // The reveal starts at the side the effect comes from.
        const glm::vec2 towards = -dir;
        setMask(state, MaskType::LinearWipe, v, effect.feather, std::atan2(towards.y, towards.x));
        break;
    }
    case EffectPreset::Iris:
        setMask(state, MaskType::Iris, v, effect.feather, 0.f);
        break;
    case EffectPreset::Clock:
        setMask(state, MaskType::Clock, v, effect.feather, 0.f);
        break;
    case EffectPreset::Dissolve:
        setMask(state, MaskType::Dissolve, v, effect.feather, 0.f);
        break;
    case EffectPreset::Move:
    case EffectPreset::Count:
        break;
    }
}

const char *effectPresetName(int preset) {
    if (preset < 0 || preset >= effectPresetCount())
        return kPresetNames[0];
    return kPresetNames[preset];
}

int effectPresetCount() {
    return static_cast<int>(EffectPreset::Count);
}

int effectPresetCategory(int preset) {
    switch (static_cast<EffectPreset>(preset)) {
    case EffectPreset::Appear:
    case EffectPreset::Fade:
        return 0;
    case EffectPreset::Wipe:
    case EffectPreset::Iris:
    case EffectPreset::Clock:
    case EffectPreset::Dissolve:
        return 1;
    case EffectPreset::Fly:
    case EffectPreset::Float:
        return 2;
    case EffectPreset::Move:
        return 4;
    default:
        return 3;
    }
}

bool effectPresetIsMove(int preset) {
    return preset == static_cast<int>(EffectPreset::Move);
}

bool effectPresetUsesDirection(int preset) {
    switch (static_cast<EffectPreset>(preset)) {
    case EffectPreset::Fly:
    case EffectPreset::Float:
    case EffectPreset::Wipe:
    case EffectPreset::Spin:
        return true;
    default:
        return false;
    }
}

bool effectPresetUsesFeather(int preset) {
    return effectPresetCategory(preset) == 1;
}

const char *effectDirectionName(int direction) {
    if (direction < 0 || direction >= effectDirectionCount())
        return kDirectionNames[0];
    return kDirectionNames[direction];
}

int effectDirectionCount() {
    return static_cast<int>(EffectDirection::Count);
}

}
