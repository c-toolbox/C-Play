/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nodestreamencoder.h"

#include <sgct/log.h>
#include <format>
#include <string>

namespace {

// One invocation per 4x4 block. Texels outside the texture replicate the edge.
const char *kEncoderShaderBody = R"(
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D srcTex;

#ifdef FMT_BC1
layout(std430, binding = 0) writeonly buffer OutBlocks { uvec2 outBlocks[]; };
#else
layout(std430, binding = 0) writeonly buffer OutBlocks { uvec4 outBlocks[]; };
#endif
layout(std430, binding = 1) buffer AlphaFlag { uint hasAlpha; };

layout(location = 0) uniform ivec2 texSize;
layout(location = 1) uniform uvec2 blockCount;

const vec3 kLumaWeights = vec3(0.299, 0.587, 0.114);

vec4 px[16];

void loadBlock(uvec2 block) {
    ivec2 base = ivec2(block) * 4;
    ivec2 maxPos = texSize - 1;
    for (int i = 0; i < 16; ++i) {
        ivec2 p = min(base + ivec2(i & 3, i >> 2), maxPos);
        px[i] = clamp(texelFetch(srcTex, p, 0), 0.0, 1.0);
    }
}

// Endpoints along the principal axis (power iteration on the covariance).
void principalEndpoints(vec4 mask, out vec4 e0, out vec4 e1) {
    vec4 mean = vec4(0.0);
    vec4 mn = vec4(1.0);
    vec4 mx = vec4(0.0);
    for (int i = 0; i < 16; ++i) {
        vec4 c = px[i] * mask;
        mean += c;
        mn = min(mn, c);
        mx = max(mx, c);
    }
    mean /= 16.0;

    mat4 cov = mat4(0.0);
    for (int i = 0; i < 16; ++i) {
        vec4 d = px[i] * mask - mean;
        cov += outerProduct(d, d);
    }

    vec4 axis = mx - mn;
    float len = length(axis);
    if (len < 1e-6) {
        e0 = mean;
        e1 = mean;
        return;
    }
    axis /= len;
    for (int k = 0; k < 8; ++k) {
        vec4 next = cov * axis;
        float l = length(next);
        if (l < 1e-8) {
            break;
        }
        axis = next / l;
    }

    float tMin = 1e9;
    float tMax = -1e9;
    for (int i = 0; i < 16; ++i) {
        float t = dot(px[i] * mask - mean, axis);
        tMin = min(tMin, t);
        tMax = max(tMax, t);
    }
    e0 = clamp(mean + axis * tMax, 0.0, 1.0);
    e1 = clamp(mean + axis * tMin, 0.0, 1.0);
}

uint pack565(vec3 c) {
    uvec3 q = uvec3(round(clamp(c, 0.0, 1.0) * vec3(31.0, 63.0, 31.0)));
    return (q.r << 11) | (q.g << 5) | q.b;
}

vec3 unpack565(uint v) {
    return vec3(float((v >> 11) & 31u) / 31.0, float((v >> 5) & 63u) / 63.0, float(v & 31u) / 31.0);
}

uvec2 encodeColorBlock() {
    vec4 e0;
    vec4 e1;
    principalEndpoints(vec4(1.0, 1.0, 1.0, 0.0), e0, e1);
    vec3 inset = (e0.rgb - e1.rgb) / 16.0;
    uint c0 = pack565(e0.rgb - inset);
    uint c1 = pack565(e1.rgb + inset);
    // c0 > c1 selects the 4-color mode.
    if (c0 < c1) {
        uint t = c0;
        c0 = c1;
        c1 = t;
    }
    if (c0 == c1) {
        return uvec2(c0 | (c1 << 16), 0u);
    }

    vec3 p0 = unpack565(c0);
    vec3 p1 = unpack565(c1);
    vec3 pal[4] = vec3[4](p0, p1, (2.0 * p0 + p1) / 3.0, (p0 + 2.0 * p1) / 3.0);

    uint indices = 0u;
    for (int i = 0; i < 16; ++i) {
        uint best = 0u;
        float bestDist = 1e9;
        for (uint k = 0u; k < 4u; ++k) {
            vec3 d = px[i].rgb - pal[k];
            float dist = dot(d * d, kLumaWeights);
            if (dist < bestDist) {
                bestDist = dist;
                best = k;
            }
        }
        indices |= best << (2u * uint(i));
    }
    return uvec2(c0 | (c1 << 16), indices);
}

uvec2 encodeAlphaBlock() {
    float aMin = 1.0;
    float aMax = 0.0;
    for (int i = 0; i < 16; ++i) {
        aMin = min(aMin, px[i].a);
        aMax = max(aMax, px[i].a);
    }
    uint a0 = uint(round(aMax * 255.0));
    uint a1 = uint(round(aMin * 255.0));
    if (a0 == a1) {
        return uvec2(a0 | (a1 << 8), 0u);
    }

    // a0 > a1 selects the 8-value mode.
    float pal[8];
    pal[0] = float(a0);
    pal[1] = float(a1);
    for (int k = 2; k < 8; ++k) {
        pal[k] = (float(8 - k) * float(a0) + float(k - 1) * float(a1)) / 7.0;
    }

    uint lo = 0u;
    uint hi = 0u;
    for (int i = 0; i < 16; ++i) {
        float a = px[i].a * 255.0;
        uint best = 0u;
        float bestDist = 1e9;
        for (uint k = 0u; k < 8u; ++k) {
            float d = abs(a - pal[k]);
            if (d < bestDist) {
                bestDist = d;
                best = k;
            }
        }
        uint bit = 3u * uint(i);
        if (bit + 3u <= 32u) {
            lo |= best << bit;
        } else if (bit >= 32u) {
            hi |= best << (bit - 32u);
        } else {
            lo |= best << bit;
            hi |= best >> (32u - bit);
        }
    }
    return uvec2(a0 | (a1 << 8) | ((lo & 0xFFFFu) << 16), (lo >> 16) | (hi << 16));
}

void putBits(inout uvec4 b, inout uint pos, uint value, uint count) {
    uint word = pos >> 5u;
    uint off = pos & 31u;
    b[word] |= value << off;
    if (off + count > 32u) {
        b[word + 1u] |= value >> (32u - off);
    }
    pos += count;
}

uvec4 quantize7(vec4 e, out uint pbit) {
    vec4 v = e * 255.0;
    uvec4 best = uvec4(0u);
    float bestErr = 1e30;
    pbit = 0u;
    for (uint p = 0u; p < 2u; ++p) {
        uvec4 q = uvec4(clamp(round((v - float(p)) / 2.0), 0.0, 127.0));
        vec4 d = vec4((q << 1u) | uvec4(p)) - v;
        float err = dot(d, d);
        if (err < bestErr) {
            bestErr = err;
            best = q;
            pbit = p;
        }
    }
    return best;
}

// BC7 mode 6: one RGBA subset, 7.7.7.7 endpoints + unique p-bits, 4-bit indices.
uvec4 encodeBC7() {
    vec4 e0;
    vec4 e1;
    principalEndpoints(vec4(1.0), e0, e1);
    vec4 inset = (e0 - e1) / 32.0;
    e0 = clamp(e0 - inset, 0.0, 1.0);
    e1 = clamp(e1 + inset, 0.0, 1.0);

    uint p0;
    uint p1;
    uvec4 q0 = quantize7(e0, p0);
    uvec4 q1 = quantize7(e1, p1);
    vec4 r0 = vec4((q0 << 1u) | uvec4(p0));
    vec4 r1 = vec4((q1 << 1u) | uvec4(p1));
    vec4 dir = r1 - r0;
    float dd = dot(dir, dir);

    uint idx[16];
    for (int i = 0; i < 16; ++i) {
        float t = dd > 0.0 ? dot(px[i] * 255.0 - r0, dir) / dd : 0.0;
        idx[i] = uint(clamp(round(t * 15.0), 0.0, 15.0));
    }

    // The anchor index has an implicit zero MSB.
    if (idx[0] >= 8u) {
        uvec4 tq = q0;
        q0 = q1;
        q1 = tq;
        uint tp = p0;
        p0 = p1;
        p1 = tp;
        for (int i = 0; i < 16; ++i) {
            idx[i] = 15u - idx[i];
        }
    }

    uvec4 b = uvec4(0u);
    uint pos = 0u;
    putBits(b, pos, 64u, 7u);
    putBits(b, pos, q0.r, 7u);
    putBits(b, pos, q1.r, 7u);
    putBits(b, pos, q0.g, 7u);
    putBits(b, pos, q1.g, 7u);
    putBits(b, pos, q0.b, 7u);
    putBits(b, pos, q1.b, 7u);
    putBits(b, pos, q0.a, 7u);
    putBits(b, pos, q1.a, 7u);
    putBits(b, pos, p0, 1u);
    putBits(b, pos, p1, 1u);
    putBits(b, pos, idx[0], 3u);
    for (int i = 1; i < 16; ++i) {
        putBits(b, pos, idx[i], 4u);
    }
    return b;
}

void main() {
    uvec2 block = gl_GlobalInvocationID.xy;
    if (block.x >= blockCount.x || block.y >= blockCount.y) {
        return;
    }

    loadBlock(block);

    float aMin = 1.0;
    for (int i = 0; i < 16; ++i) {
        aMin = min(aMin, px[i].a);
    }
    if (aMin < 0.996) {
        atomicOr(hasAlpha, 1u);
    }

    uint index = block.y * blockCount.x + block.x;
#if defined(FMT_BC1)
    outBlocks[index] = encodeColorBlock();
#elif defined(FMT_BC3)
    outBlocks[index] = uvec4(encodeAlphaBlock(), encodeColorBlock());
#else
    outBlocks[index] = encodeBC7();
#endif
}
)";

const char *formatDefine(nodestream::Format format) {
    switch (format) {
    case nodestream::Format::BC1:
        return "FMT_BC1";
    case nodestream::Format::BC3:
        return "FMT_BC3";
    default:
        return "FMT_BC7";
    }
}

GLuint buildProgram(nodestream::Format format) {
    const std::string source = std::string("#version 460 core\n#define ") + formatDefine(format) + "\n" + kEncoderShaderBody;
    const char *src = source.c_str();

    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[2048] = {};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        sgct::Log::Error(std::format("NodeStreamEncoder: {} compute shader failed to compile: {}", formatDefine(format), log));
        glDeleteShader(shader);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);
    glDeleteShader(shader);

    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[2048] = {};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        sgct::Log::Error(std::format("NodeStreamEncoder: {} compute program failed to link: {}", formatDefine(format), log));
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

} // namespace

size_t NodeStreamEncoder::alphaFlagOffset(nodestream::Format format, int width, int height) {
    return (nodestream::frameBytes(format, width, height) + 15) & ~static_cast<size_t>(15);
}

size_t NodeStreamEncoder::outputSize(nodestream::Format format, int width, int height) {
    return alphaFlagOffset(format, width, height) + 16;
}

GLuint NodeStreamEncoder::program(nodestream::Format format) {
    const int idx = static_cast<int>(format);
    if (idx < 1 || idx > 3)
        return 0;
    if (!m_programs[idx] && !m_programFailed[idx]) {
        m_programs[idx] = buildProgram(format);
        m_programFailed[idx] = (m_programs[idx] == 0);
    }
    return m_programs[idx];
}

void NodeStreamEncoder::collectTimerQueries() {
    for (int i = 0; i < kQueryCount; ++i) {
        if (!m_queryPending[i])
            continue;
        GLint available = 0;
        glGetQueryObjectiv(m_queries[i * 2 + 1], GL_QUERY_RESULT_AVAILABLE, &available);
        if (!available)
            continue;
        GLuint64 start = 0;
        GLuint64 end = 0;
        glGetQueryObjectui64v(m_queries[i * 2], GL_QUERY_RESULT, &start);
        glGetQueryObjectui64v(m_queries[i * 2 + 1], GL_QUERY_RESULT, &end);
        if (end >= start)
            m_lastGpuTimeMs = static_cast<double>(end - start) / 1.0e6;
        m_queryPending[i] = false;
    }
}

double NodeStreamEncoder::lastGpuTimeMs() const {
    return m_lastGpuTimeMs;
}

bool NodeStreamEncoder::encode(GLuint texture, int width, int height, nodestream::Format format, GLuint dstBuffer) {
    if (texture == 0 || dstBuffer == 0 || width <= 0 || height <= 0)
        return false;

    const GLuint prog = program(format);
    if (!prog)
        return false;

    const size_t dataSize = nodestream::frameBytes(format, width, height);
    const uint32_t blocksX = nodestream::blocksAcross(width);
    const uint32_t blocksY = nodestream::blocksAcross(height);

    if (m_blockBufferSize < dataSize) {
        if (m_blockBuffer)
            glDeleteBuffers(1, &m_blockBuffer);
        glGenBuffers(1, &m_blockBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_blockBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(dataSize), nullptr, GL_DYNAMIC_COPY);
        m_blockBufferSize = dataSize;
    }
    if (!m_alphaBuffer) {
        glGenBuffers(1, &m_alphaBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_alphaBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GLuint), nullptr, GL_DYNAMIC_COPY);
    }
    const GLuint zero = 0;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_alphaBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint), &zero);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    if (!m_queries[0])
        glGenQueries(kQueryCount * 2, m_queries);
    collectTimerQueries();
    const int query = m_queryIndex;
    const bool timeThisFrame = !m_queryPending[query];
    if (timeThisFrame)
        glQueryCounter(m_queries[query * 2], GL_TIMESTAMP);

    GLint prevActiveTexture = GL_TEXTURE0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &prevActiveTexture);
    glActiveTexture(GL_TEXTURE0);
    GLint prevTexture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTexture);
    GLint prevProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);

    glUseProgram(prog);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform2i(0, width, height);
    glUniform2ui(1, blocksX, blocksY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_blockBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_alphaBuffer);

    glDispatchCompute((blocksX + 7) / 8, (blocksY + 7) / 8, 1);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

    glBindBuffer(GL_COPY_READ_BUFFER, m_blockBuffer);
    glBindBuffer(GL_COPY_WRITE_BUFFER, dstBuffer);
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, static_cast<GLsizeiptr>(dataSize));
    glBindBuffer(GL_COPY_READ_BUFFER, m_alphaBuffer);
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0,
                        static_cast<GLintptr>(alphaFlagOffset(format, width, height)), sizeof(GLuint));
    glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);

    if (timeThisFrame) {
        glQueryCounter(m_queries[query * 2 + 1], GL_TIMESTAMP);
        m_queryPending[query] = true;
        m_queryIndex = (m_queryIndex + 1) % kQueryCount;
    }

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
    glBindBuffer(GL_COPY_READ_BUFFER, 0);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prevTexture));
    glActiveTexture(static_cast<GLenum>(prevActiveTexture));
    glUseProgram(static_cast<GLuint>(prevProgram));

    return true;
}

void NodeStreamEncoder::cleanupGL() {
    for (int i = 0; i < 4; ++i) {
        if (m_programs[i]) {
            glDeleteProgram(m_programs[i]);
            m_programs[i] = 0;
        }
        m_programFailed[i] = false;
    }
    if (m_blockBuffer) {
        glDeleteBuffers(1, &m_blockBuffer);
        m_blockBuffer = 0;
        m_blockBufferSize = 0;
    }
    if (m_alphaBuffer) {
        glDeleteBuffers(1, &m_alphaBuffer);
        m_alphaBuffer = 0;
    }
    if (m_queries[0]) {
        glDeleteQueries(kQueryCount * 2, m_queries);
        for (int i = 0; i < kQueryCount * 2; ++i)
            m_queries[i] = 0;
    }
    for (int i = 0; i < kQueryCount; ++i)
        m_queryPending[i] = false;
    m_queryIndex = 0;
}
