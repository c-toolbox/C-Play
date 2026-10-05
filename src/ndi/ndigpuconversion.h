/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NDIGPUCONVERSION_H
#define NDIGPUCONVERSION_H

#include <sgct/opengl.h>

// Owned by one render context. Call cleanup() with that context current.
// One RGBA8UI texel stores U, Y0, V, Y1: exactly two bytes per video pixel.
class NdiGpuConversion {
public:
    NdiGpuConversion() = default;
    NdiGpuConversion(const NdiGpuConversion&) = delete;
    NdiGpuConversion& operator=(const NdiGpuConversion&) = delete;
    bool initialize();
    bool upload(GLuint rgbaTexture, int width, int height,
                const unsigned char* uyvy, unsigned int stride);
    GLuint pack(GLuint rgbaTexture, int width, int height, bool invertY);
    void cleanup();

private:
    bool allocate(int width, int height);
    void dispatch(GLuint program, GLuint input, GLuint output,
                  int width, int height, bool invertY);
    GLuint m_unpackProgram = 0;
    GLuint m_packProgram = 0;
    GLuint m_texture = 0;
    int m_width = 0;
    int m_height = 0;
    bool m_attempted = false;
};

#endif
