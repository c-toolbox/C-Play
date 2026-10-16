/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CPLAY_POSTEFFECTS_H
#define CPLAY_POSTEFFECTS_H

#include <glm/glm.hpp>
#include <array>
#include <cstdint>

namespace cplay::anim {

// Bits of PostEffects::enabled. Order is persisted in presentation files; only append.
enum PostEffectFlag : uint32_t {
    PostChromaKey = 1u << 0,
    PostLumaKey = 1u << 1,
    PostColorCorrection = 1u << 2,
    PostTint = 1u << 3,
    PostBlur = 1u << 4,
    PostSharpen = 1u << 5,
    PostPixelate = 1u << 6,
    PostVignette = 1u << 7,
    PostInvert = 1u << 8
};

// Texture effects that are always applied to a layer when it is rendered (after the
// texture is sampled, before entrance/exit masks and opacity). They are a property of
// the layer and follow it into every slide, unlike the per-slide entrance/exit effects.
struct PostEffects {
    uint32_t enabled = 0;

    // Chroma key: chroma distance (CbCr) to keyColor below similarity is removed,
    // smoothness widens the transition, spill desaturates the key color left on edges.
    glm::vec3 keyColor = glm::vec3(0.f, 1.f, 0.f);
    float keySimilarity = 0.4f;
    float keySmoothness = 0.08f;
    float keySpill = 0.1f;

    // Luma key: luma below threshold is removed (above it when inverted).
    float lumaThreshold = 0.1f;
    float lumaSmoothness = 0.05f;
    bool lumaInvert = false;

    // Color correction.
    float brightness = 0.f;  // -1..1
    float contrast = 1.f;    // 0..3
    float saturation = 1.f;  // 0..3
    float gamma = 1.f;       // 0.1..4
    float hue = 0.f;         // degrees, -180..180

    // Tint: blends towards the luma colored by tintColor.
    glm::vec3 tintColor = glm::vec3(1.f, 0.85f, 0.6f);
    float tintAmount = 0.5f;

    float blurRadius = 4.f;      // texels
    float sharpenAmount = 0.5f;  // 0..3
    float pixelSize = 16.f;      // texels

    // Vignette: radius and softness in half layer sizes; mode 0 darkens, 1 fades to transparent.
    float vignetteRadius = 0.75f;
    float vignetteSoftness = 0.45f;
    float vignetteAmount = 1.f;
    int32_t vignetteMode = 0;

    bool active() const { return enabled != 0; }
    bool has(PostEffectFlag f) const { return (enabled & f) != 0; }

    static constexpr int ParamCount = 7;
    // Packs the parameters as the postParams uniform array of CPLAY_POST_GLSL.
    std::array<glm::vec4, ParamCount> packParams() const {
        return {
            glm::vec4(keyColor, keySimilarity),
            glm::vec4(keySmoothness, keySpill, lumaThreshold, lumaSmoothness),
            glm::vec4(brightness, contrast, saturation, gamma),
            glm::vec4(glm::radians(hue), lumaInvert ? 1.f : 0.f, 0.f, 0.f),
            glm::vec4(tintColor, tintAmount),
            glm::vec4(blurRadius, sharpenAmount, pixelSize, 0.f),
            glm::vec4(vignetteRadius, vignetteSoftness, vignetteAmount, static_cast<float>(vignetteMode))
        };
    }
};

}

// Layer shaders are built in two variants, each defining cplayPost(uv, localUv) in place of
// texture(tex, uv) and requiring "uniform sampler2D tex;" to be declared first:
//  - CPLAY_POST_NONE_GLSL: a plain texture fetch, so layers without post effects render
//    exactly as before (no extra uniforms, branches or code).
//  - CPLAY_POST_GLSL: the post effects, used only for layers with an effect turned on.
// uv is the texture coordinate, localUv is 0..1 over the shown layer (used by the vignette).
// Macros so they concatenate with raw string literals.
#define CPLAY_POST_NONE_GLSL R"(
vec4 cplayPost(vec2 uv, vec2 localUv) {
    return texture(tex, uv);
}
)"

#define CPLAY_POST_GLSL R"(
uniform int postFlags;
uniform vec4 postParams[7];

vec4 cplayPostSample(vec2 uv) {
    vec2 size = vec2(textureSize(tex, 0));
    vec2 texel = 1.0 / max(size, vec2(1.0));
    if ((postFlags & 64) != 0) {
        vec2 block = max(postParams[5].z, 1.0) * texel;
        uv = (floor(uv / block) + 0.5) * block;
    }
    vec4 c = texture(tex, uv);
    if ((postFlags & 16) != 0 && postParams[5].x > 0.0) {
        // Golden angle spiral, weighted towards the centre.
        vec4 acc = c;
        float wsum = 1.0;
        for (int i = 0; i < 32; i++) {
            float r = sqrt((float(i) + 0.5) / 32.0);
            float th = float(i) * 2.39996323;
            float w = exp(-2.0 * r * r);
            acc += texture(tex, uv + vec2(cos(th), sin(th)) * r * postParams[5].x * texel) * w;
            wsum += w;
        }
        c = acc / wsum;
    }
    if ((postFlags & 32) != 0) {
        vec4 n = texture(tex, uv + vec2(texel.x, 0.0)) + texture(tex, uv - vec2(texel.x, 0.0))
               + texture(tex, uv + vec2(0.0, texel.y)) + texture(tex, uv - vec2(0.0, texel.y));
        c.rgb = max(c.rgb + postParams[5].y * (c.rgb - n.rgb * 0.25), vec3(0.0));
    }
    return c;
}

vec4 cplayPost(vec2 uv, vec2 localUv) {
    if (postFlags == 0)
        return texture(tex, uv);
    vec4 c = cplayPostSample(uv);
    const vec3 lumaW = vec3(0.2126, 0.7152, 0.0722);
    if ((postFlags & 1) != 0) {
        // BT.709 CbCr distance (as OBS) so the familiar similarity values apply.
        const vec3 cbW = vec3(-0.100644, -0.338572, 0.439216);
        const vec3 crW = vec3(0.439216, -0.398942, -0.040274);
        vec3 key = postParams[0].rgb;
        float d = distance(vec2(dot(c.rgb, cbW), dot(c.rgb, crW)), vec2(dot(key, cbW), dot(key, crW)));
        float base = d - postParams[0].a;
        c.a *= pow(clamp(base / max(postParams[1].x, 0.0001), 0.0, 1.0), 1.5);
        float spill = pow(clamp(base / max(postParams[1].y, 0.0001), 0.0, 1.0), 1.5);
        c.rgb = mix(vec3(dot(c.rgb, lumaW)), c.rgb, spill);
    }
    if ((postFlags & 2) != 0) {
        float y = dot(c.rgb, lumaW);
        float s = max(postParams[1].w, 0.0001);
        float k = smoothstep(postParams[1].z - s, postParams[1].z + s, y);
        c.a *= postParams[3].y > 0.5 ? 1.0 - k : k;
    }
    if ((postFlags & 4) != 0) {
        vec3 rgb = (c.rgb - 0.5) * postParams[2].y + 0.5 + postParams[2].x;
        rgb = mix(vec3(dot(rgb, lumaW)), rgb, postParams[2].z);
        float a = postParams[3].x;
        if (a != 0.0) {
            const vec3 k = vec3(0.57735027);
            float ca = cos(a);
            rgb = rgb * ca + cross(k, rgb) * sin(a) + k * dot(k, rgb) * (1.0 - ca);
        }
        c.rgb = pow(clamp(rgb, 0.0, 1.0), vec3(1.0 / max(postParams[2].w, 0.01)));
    }
    if ((postFlags & 256) != 0)
        c.rgb = 1.0 - c.rgb;
    if ((postFlags & 8) != 0)
        c.rgb = mix(c.rgb, dot(c.rgb, lumaW) * postParams[4].rgb, postParams[4].a);
    if ((postFlags & 128) != 0) {
        float d = length((localUv - 0.5) * 2.0);
        float v = 1.0 - postParams[6].z * smoothstep(postParams[6].x, postParams[6].x + max(postParams[6].y, 0.0001), d);
        if (postParams[6].w > 0.5)
            c.a *= v;
        else
            c.rgb *= v;
    }
    return c;
}
)"

namespace cplay::anim {

// GLSL defining cplayPost() for a shader variant (see above).
inline const char *postGlsl(bool withEffects) {
    return withEffects ? CPLAY_POST_GLSL : CPLAY_POST_NONE_GLSL;
}

}

#endif // CPLAY_POSTEFFECTS_H
