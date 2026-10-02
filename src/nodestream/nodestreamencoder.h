/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef NODESTREAMENCODER_H
#define NODESTREAMENCODER_H

#include "nodestreamprotocol.h"
#include <sgct/opengl.h>
#include <cstddef>

// Real-time GPU block compression (BC1, BC3, BC7 mode 6) of an OpenGL texture
// with compute shaders. Output blocks are row-major, followed by a 4-byte flag
// at alphaFlagOffset() that is non-zero when any texel had alpha below 1.
// All methods must be called with the texture's OpenGL context current.
class NodeStreamEncoder {
public:
    NodeStreamEncoder() = default;
    ~NodeStreamEncoder() = default;

    NodeStreamEncoder(const NodeStreamEncoder &) = delete;
    NodeStreamEncoder &operator=(const NodeStreamEncoder &) = delete;

    static size_t alphaFlagOffset(nodestream::Format format, int width, int height);
    static size_t outputSize(nodestream::Format format, int width, int height);

    // Compresses the texture and copies the result into dstBuffer at offset 0.
    bool encode(GLuint texture, int width, int height, nodestream::Format format, GLuint dstBuffer);

    // GPU time of a recent encode in milliseconds, or a negative value when unknown.
    double lastGpuTimeMs() const;

    void cleanupGL();

private:
    GLuint program(nodestream::Format format);
    void collectTimerQueries();

    GLuint m_programs[4] = {0, 0, 0, 0};
    bool m_programFailed[4] = {false, false, false, false};
    GLuint m_blockBuffer = 0;
    size_t m_blockBufferSize = 0;
    GLuint m_alphaBuffer = 0;

    static constexpr int kQueryCount = 4;
    GLuint m_queries[kQueryCount * 2] = {};
    bool m_queryPending[kQueryCount] = {false, false, false, false};
    int m_queryIndex = 0;
    double m_lastGpuTimeMs = -1.0;
};

#endif // NODESTREAMENCODER_H
