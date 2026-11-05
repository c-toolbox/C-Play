/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "particlecompositor.h"
#include "layereffects.h"
#include <layers/baselayer.h>
#include <sgct/log.h>
#include <sgct/opengl.h>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>
#include <format>
#include <string>
#include <vector>

namespace cplay::anim {

namespace {

// One quad (two triangles) per particle, without vertex attributes: the cell comes from
// gl_InstanceID and the corner from gl_VertexID. Coordinates are 0..1 over the drawn
// region (one eye's shown area), whose viewport is set by the caller.
constexpr const char *ParticleVert = R"(
#version 410 core

uniform ivec2 cells;
uniform vec4 region;
uniform int mode; // 1 flat, 2 dome, 3 sphere (equirectangular)
uniform float progress;
uniform float spread;
uniform float stagger;
uniform int seed;
uniform float halfFov;
uniform float aspect;
uniform float copyShift;

out vec2 v_uv;
out float v_visible;

const float PI = 3.14159265358979;
const vec2 corners[6] = vec2[](vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0),
                               vec2(0.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0));

uint pcg(uint v) {
    uint state = v * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float rnd(uint cell, uint k) {
    return float(pcg(cell * 8u + k + pcg(uint(seed)))) / 4294967295.0;
}

mat2 rot2(float a) {
    float c = cos(a), s = sin(a);
    return mat2(c, s, -s, c);
}

vec3 rotateAxis(vec3 v, vec3 k, float a) {
    float c = cos(a), s = sin(a);
    return v * c + cross(k, v) * s + k * dot(k, v) * (1.0 - c);
}

// Equidistant fisheye: the zenith (+z) at the centre, halfFov at the inscribed circle.
vec3 domeDir(vec2 q) {
    vec2 p = (q - 0.5) * 2.0;
    float r = length(p);
    if (r < 1e-6)
        return vec3(0.0, 0.0, 1.0);
    float th = r * halfFov;
    return vec3(p / r * sin(th), cos(th));
}

vec2 domeUv(vec3 d) {
    float th = acos(clamp(d.z, -1.0, 1.0));
    float s = length(d.xy);
    vec2 n = s > 1e-7 ? d.xy / s : vec2(0.0);
    return 0.5 + 0.5 * n * (th / max(halfFov, 1e-4));
}

// Equirectangular: longitude along u, latitude along v. Corners on the poles keep their
// longitude, so the image is exact when the particles are in place.
vec3 sphereDir(vec2 q) {
    float lon = (q.x - 0.5) * 2.0 * PI;
    float lat = clamp((q.y - 0.5) * PI, -0.5 * PI + 1e-3, 0.5 * PI - 1e-3);
    return vec3(cos(lat) * sin(lon), sin(lat), cos(lat) * cos(lon));
}

vec2 sphereUv(vec3 d) {
    return vec2(atan(d.x, d.z) / (2.0 * PI) + 0.5, asin(clamp(d.y, -1.0, 1.0)) / PI + 0.5);
}

void main() {
    int cell = gl_InstanceID;
    vec2 size = 1.0 / vec2(cells);
    vec2 c0 = vec2(float(cell % cells.x), float(cell / cells.x)) * size;
    vec2 q = c0 + corners[gl_VertexID] * size;
    vec2 m = c0 + 0.5 * size;
    v_uv = region.xy + q * region.zw;

    uint id = uint(cell);
    // Each particle has its own time span within the effect.
    float t = clamp((progress - rnd(id, 0u) * stagger) / max(1.0 - stagger, 1e-4), 0.0, 1.0);
    t = t * t * (3.0 - 2.0 * t);
    float s = 1.0 - t;
    v_visible = smoothstep(0.0, 0.35, t);
    float scale = mix(1.0, mix(0.25, 1.75, rnd(id, 1u)), s);
    float tumble = (rnd(id, 2u) - 0.5) * 3.0 * PI * s;

    vec2 outQ;
    bool cull = false;
    if (mode == 1) {
        // Flat: out from the centre in a curve, in units of the region height.
        vec2 P = vec2(q.x * aspect, q.y);
        vec2 M = vec2(m.x * aspect, m.y);
        float a = rnd(id, 3u) * 2.0 * PI;
        vec2 rdir = vec2(cos(a), sin(a));
        vec2 radial = M - vec2(0.5 * aspect, 0.5);
        radial = length(radial) > 1e-4 ? normalize(radial) : rdir;
        vec2 dir = mix(rdir, radial, 0.6);
        dir = length(dir) > 1e-3 ? normalize(dir) : rdir;
        float dist = spread * mix(0.35, 1.1, rnd(id, 4u));
        float curl = (rnd(id, 5u) - 0.5) * 3.0 * s;
        vec2 Mp = M + rot2(curl) * dir * dist * s;
        vec2 Pp = Mp + rot2(tumble) * (P - M) * scale;
        outQ = vec2(Pp.x / aspect, Pp.y);
    }
    else {
        // Dome and sphere: rigid rotations around the viewer keep the particle's shape on
        // the shown surface; tumble and scale act around the particle's own direction.
        bool dome = mode == 2;
        vec3 C = dome ? domeDir(m) : sphereDir(m);
        vec3 D = dome ? domeDir(q) : sphereDir(q);
        D = normalize(C + rotateAxis(D - C, C, tumble) * scale);
        float z = rnd(id, 3u) * 2.0 - 1.0;
        float a = rnd(id, 4u) * 2.0 * PI;
        vec3 axis = vec3(sqrt(max(1.0 - z * z, 0.0)) * vec2(cos(a), sin(a)), z);
        float angle = spread * mix(0.3, 1.0, rnd(id, 5u)) * (dome ? 1.2 : 1.8) * s;
        vec3 up = dome ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
        float swirl = (rnd(id, 6u) - 0.5) * 2.0 * spread * s;
        D = rotateAxis(rotateAxis(D, axis, angle), up, swirl);
        vec3 Cn = rotateAxis(rotateAxis(C, axis, angle), up, swirl);
        if (dome) {
            // Cells outside the fisheye circle hold nothing that is shown, and near the
            // opposite pole the fisheye folds around: drop particles there.
            float diag = length(size) * 2.0;
            float rest = length((m - 0.5) * 2.0);
            float th = acos(clamp(Cn.z, -1.0, 1.0));
            cull = rest > 1.0 + diag || (s > 0.0 && th > min(halfFov * 1.4, 2.8));
            outQ = domeUv(D);
        }
        else {
            // Corners follow the particle across the seam; copyShift draws the part that
            // wraps around on the other side.
            vec2 cq = sphereUv(Cn);
            outQ = sphereUv(D);
            float du = outQ.x - cq.x;
            du -= floor(du + 0.5);
            outQ.x = cq.x + clamp(du, -0.25, 0.25) + copyShift;
        }
        // In place exactly, so the image matches the layer's own when the effect ends.
        if (s <= 0.0)
            outQ = q + vec2(copyShift, 0.0);
    }
    gl_Position = cull ? vec4(2.0, 2.0, 2.0, 1.0) : vec4(outQ * 2.0 - 1.0, 0.0, 1.0);
}
)";

// Particles fade out as a dissolve rather than with alpha, so that they need no blending
// between each other: in place, they reproduce the texture exactly (straight alpha).
constexpr const char *ParticleFrag = R"(
#version 410 core

uniform sampler2D tex;

in vec2 v_uv;
in float v_visible;
out vec4 out_color;

float hash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

void main() {
    vec4 c = texture(tex, v_uv);
    if (c.a <= 0.0 || hash(floor(v_uv * vec2(textureSize(tex, 0)))) >= v_visible)
        discard;
    out_color = c;
}
)";

GLuint compile(GLenum type, const char *src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::string log(static_cast<size_t>(std::max(len, 1)), '\0');
        glGetShaderInfoLog(shader, len, nullptr, log.data());
        sgct::Log::Error(std::format("Particle effect shader failed to compile: {}", log));
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

// Restores the OpenGL state changed while rendering particles.
struct StateGuard {
    GLint drawFbo = 0, readFbo = 0, program = 0, vao = 0, activeTexture = 0, texture = 0;
    GLint viewport[4] = {};
    GLboolean blend = GL_FALSE, scissor = GL_FALSE, depth = GL_FALSE, cull = GL_FALSE, stencil = GL_FALSE;
    GLboolean colorMask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
    GLfloat clearColor[4] = {};

    StateGuard() {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFbo);
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
        glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
        blend = glIsEnabled(GL_BLEND);
        scissor = glIsEnabled(GL_SCISSOR_TEST);
        depth = glIsEnabled(GL_DEPTH_TEST);
        cull = glIsEnabled(GL_CULL_FACE);
        stencil = glIsEnabled(GL_STENCIL_TEST);
    }

    ~StateGuard() {
        auto setEnabled = [](GLenum cap, GLboolean on) { on ? glEnable(cap) : glDisable(cap); };
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(drawFbo));
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(readFbo));
        glUseProgram(static_cast<GLuint>(program));
        glBindVertexArray(static_cast<GLuint>(vao));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture));
        glActiveTexture(static_cast<GLenum>(activeTexture));
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
        glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
        setEnabled(GL_BLEND, blend);
        setEnabled(GL_SCISSOR_TEST, scissor);
        setEnabled(GL_DEPTH_TEST, depth);
        setEnabled(GL_CULL_FACE, cull);
        setEnabled(GL_STENCIL_TEST, stencil);
    }
};

// Texture regions of the eyes, as the layer shaders pick them (see "stereoscopicMode").
std::vector<glm::vec4> eyeRegions(int stereoMode) {
    switch (stereoMode) {
    case 1: return {glm::vec4(0.f, 0.f, 0.5f, 1.f), glm::vec4(0.5f, 0.f, 0.5f, 1.f)};
    case 2:
    case 3: return {glm::vec4(0.f, 0.f, 1.f, 0.5f), glm::vec4(0.f, 0.5f, 1.f, 0.5f)};
    default: return {glm::vec4(0.f, 0.f, 1.f, 1.f)};
    }
}

constexpr int kMaxTextureSize = 8192;
// Frames a layer's particle texture is kept after its effect ended.
constexpr int kIdleFrames = 3;

}

ParticleCompositor::~ParticleCompositor() {
    // Without a current context the OpenGL objects can not be freed here; owners call
    // release() while theirs is current.
}

unsigned int ParticleCompositor::layerTexture(const BaseLayer *layer, int gridMode, int stereoMode,
                                              float domeFovDeg, const void *context, uint64_t frame) {
    if (!layer)
        return 0;
    const unsigned int source = layer->textureId();
    const LayerAnimState &a = layer->animState();
    if (source == 0 || !layer->animActive() || a.particleMode == 0)
        return source;

    Context &ctx = m_contexts[context];
    if (!ensureProgram(ctx))
        return source;

    // The source texture's own size, which a sublayer's width() need not be.
    GLint prevTexture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTexture);
    glBindTexture(GL_TEXTURE_2D, source);
    GLint width = 0, height = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prevTexture));
    if (width <= 0 || height <= 0)
        return source;

    Target &t = ctx.targets[layer];
    t.usedThisFrame = true;
    if (!t.rendered || t.frame != frame) {
        render(ctx, t, source, width, height, a, gridMode, stereoMode, layer->roiEnabled(), layer->roi(),
               domeFovDeg);
        t.frame = frame;
        t.rendered = true;
    }
    return t.texture ? t.texture : source;
}

void ParticleCompositor::endFrame(const void *context) {
    auto it = m_contexts.find(context);
    if (it == m_contexts.end())
        return;
    auto &targets = it->second.targets;
    for (auto t = targets.begin(); t != targets.end();) {
        Target &target = t->second;
        target.idleFrames = target.usedThisFrame ? 0 : target.idleFrames + 1;
        target.usedThisFrame = false;
        if (target.idleFrames > kIdleFrames) {
            releaseTarget(target);
            t = targets.erase(t);
        }
        else {
            ++t;
        }
    }
}

void ParticleCompositor::release(const void *context) {
    auto it = m_contexts.find(context);
    if (it == m_contexts.end())
        return;
    Context &ctx = it->second;
    for (auto &kv : ctx.targets)
        releaseTarget(kv.second);
    if (ctx.program)
        glDeleteProgram(ctx.program);
    if (ctx.vao)
        glDeleteVertexArrays(1, &ctx.vao);
    m_contexts.erase(it);
}

bool ParticleCompositor::ensureProgram(Context &ctx) {
    if (ctx.program)
        return true;
    if (ctx.failed)
        return false;
    ctx.failed = true;

    const GLuint vs = compile(GL_VERTEX_SHADER, ParticleVert);
    const GLuint fs = compile(GL_FRAGMENT_SHADER, ParticleFrag);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }
    const GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        std::string log(static_cast<size_t>(std::max(len, 1)), '\0');
        glGetProgramInfoLog(program, len, nullptr, log.data());
        sgct::Log::Error(std::format("Particle effect shader failed to link: {}", log));
        glDeleteProgram(program);
        return false;
    }

    ctx.program = program;
    ctx.locCells = glGetUniformLocation(program, "cells");
    ctx.locRegion = glGetUniformLocation(program, "region");
    ctx.locMode = glGetUniformLocation(program, "mode");
    ctx.locProgress = glGetUniformLocation(program, "progress");
    ctx.locSpread = glGetUniformLocation(program, "spread");
    ctx.locStagger = glGetUniformLocation(program, "stagger");
    ctx.locSeed = glGetUniformLocation(program, "seed");
    ctx.locHalfFov = glGetUniformLocation(program, "halfFov");
    ctx.locAspect = glGetUniformLocation(program, "aspect");
    ctx.locCopyShift = glGetUniformLocation(program, "copyShift");
    GLint previous = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previous);
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "tex"), 0);
    glUseProgram(static_cast<GLuint>(previous));
    // Core profiles draw only with a vertex array bound, even without attributes.
    glGenVertexArrays(1, &ctx.vao);
    ctx.failed = false;
    return true;
}

void ParticleCompositor::releaseTarget(Target &t) {
    if (t.fbo)
        glDeleteFramebuffers(1, &t.fbo);
    if (t.texture)
        glDeleteTextures(1, &t.texture);
    t = Target();
}

void ParticleCompositor::render(Context &ctx, Target &t, unsigned int source, int width, int height,
                                const LayerAnimState &a, int gridMode, int stereoMode, bool roiEnabled,
                                const glm::vec4 &roi, float domeFovDeg) {
    StateGuard guard;

    GLint maxSize = kMaxTextureSize;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
    const float fit = std::min(1.f, static_cast<float>(std::min(maxSize, kMaxTextureSize))
                                        / static_cast<float>(std::max(width, height)));
    const int w = std::max(1, static_cast<int>(std::lround(width * fit)));
    const int h = std::max(1, static_cast<int>(std::lround(height * fit)));

    if (!t.texture || t.width != w || t.height != h) {
        if (!t.texture)
            glGenTextures(1, &t.texture);
        glBindTexture(GL_TEXTURE_2D, t.texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        if (!t.fbo)
            glGenFramebuffers(1, &t.fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.texture, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            sgct::Log::Error("Particle effect framebuffer is incomplete");
            releaseTarget(t);
            return;
        }
        t.width = w;
        t.height = h;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glViewport(0, 0, w, h);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(ctx.program);
    glBindVertexArray(ctx.vao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, source);

    const bool sphere = gridMode == BaseLayer::Sphere_EQR;
    const int mode = gridMode == BaseLayer::Dome ? 2 : sphere ? 3 : 1;
    glUniform1i(ctx.locMode, mode);
    glUniform1f(ctx.locProgress, std::clamp(a.particleProgress, 0.f, 1.f));
    glUniform1f(ctx.locSpread, std::max(a.particleSpread, 0.f));
    glUniform1f(ctx.locStagger, std::clamp(a.particleStagger, 0.f, 0.95f));
    glUniform1i(ctx.locSeed, a.particleSeed);
    glUniform1f(ctx.locHalfFov, glm::radians(std::clamp(domeFovDeg, 1.f, 360.f) * 0.5f));

    const glm::vec4 shown = roiEnabled ? roi : glm::vec4(0.f, 0.f, 1.f, 1.f);
    for (const glm::vec4 &eye : eyeRegions(stereoMode)) {
        const glm::vec4 region(eye.x + shown.x * eye.z, eye.y + shown.y * eye.w, shown.z * eye.z, shown.w * eye.w);
        const int x0 = static_cast<int>(std::lround(region.x * w));
        const int y0 = static_cast<int>(std::lround(region.y * h));
        const int rw = static_cast<int>(std::lround((region.x + region.z) * w)) - x0;
        const int rh = static_cast<int>(std::lround((region.y + region.w) * h)) - y0;
        if (rw <= 0 || rh <= 0)
            continue;
        const float aspect = static_cast<float>(rw) / static_cast<float>(rh);
        const int cols = std::clamp(a.particleColumns, kMinParticles, kMaxParticles);
        const int rows = std::clamp(static_cast<int>(std::lround(cols / aspect)), 1, kMaxParticles);

        glViewport(x0, y0, rw, rh);
        glUniform4f(ctx.locRegion, region.x, region.y, region.z, region.w);
        glUniform2i(ctx.locCells, cols, rows);
        glUniform1f(ctx.locAspect, aspect);
        for (const float shift : sphere ? std::vector<float>{0.f, -1.f, 1.f} : std::vector<float>{0.f}) {
            glUniform1f(ctx.locCopyShift, shift);
            glDrawArraysInstanced(GL_TRIANGLES, 0, 6, cols * rows);
        }
    }
}

}
