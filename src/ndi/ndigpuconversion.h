/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef NDIGPUCONVERSION_H
#define NDIGPUCONVERSION_H

#include <sgct/opengl.h>

// Owned by one render context. Call cleanup() with that context current.
// Output packs U,Y0,V,Y1 into RGBA8UI (two bytes per pixel).
// Input uploads native raw planes to a streaming SSBO and converts to RGBA8.
class NdiGpuConversion {
public:
    enum class Format { UYVY, UYVA, NV12, I420, YV12, P216, PA16, BGRA, BGRX, RGBA, RGBX };
    enum class Field { Progressive, Upper, Lower };
    NdiGpuConversion() = default;
    NdiGpuConversion(const NdiGpuConversion&) = delete;
    NdiGpuConversion& operator=(const NdiGpuConversion&) = delete;
    bool initialize();
    bool upload(GLuint rgbaTexture, int width, int height,
                const unsigned char* data, unsigned int stride, Format format = Format::UYVY,
                Field field = Field::Progressive);
    GLuint pack(GLuint rgbaTexture, int width, int height, bool invertY);
    void cleanup();

private:
    bool allocate(int width, int height);
    void dispatch(GLuint program, GLuint input, GLuint output,
                  int width, int height, bool invertY);
    GLuint m_unpackProgram = 0;
    GLuint m_packProgram = 0;
    GLuint m_texture = 0;
    GLuint m_uploadBuffer = 0;
    int m_width = 0;
    int m_height = 0;
    bool m_attempted = false;
};

#endif
