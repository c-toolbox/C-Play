/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ndigpuconversion.h"
#include <sgct/log.h>
#include <string>

namespace {
const char* Common = R"GLSL(
#version 430 core
layout(local_size_x=16, local_size_y=16) in;
uniform ivec2 size;
uniform bool invertY;
// NDI's resolution-dependent matrix: SD 601, HD 709, UHD 2020.
vec3 weights() {
    if (size.x > 1920 || size.y > 1080) return vec3(0.2627, 0.6780, 0.0593);
    if (size.x > 720 || size.y > 576) return vec3(0.2126, 0.7152, 0.0722);
    return vec3(0.2990, 0.5870, 0.1140);
}
)GLSL";
const char* Pack = R"GLSL(
layout(binding=0) uniform sampler2D source;
layout(rgba8ui, binding=0) writeonly uniform uimage2D target;
void main() {
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    if (p.x >= size.x/2 || p.y >= size.y) return;
    int y = invertY ? size.y-1-p.y : p.y;
    vec3 a = texelFetch(source, ivec2(p.x*2,y), 0).rgb;
    vec3 b = texelFetch(source, ivec2(p.x*2+1,y), 0).rgb;
    vec3 w = weights();
    float ya = dot(a,w), yb = dot(b,w);
    vec3 avg = (a+b)*0.5;
    float luma = (ya+yb)*0.5;
    float u = (avg.b-luma)/(2.0*(1.0-w.b));
    float v = (avg.r-luma)/(2.0*(1.0-w.r));
    // Studio-range YCbCr; chroma shared by each horizontal pair.
    vec4 bytes = vec4(128.0+224.0*u, 16.0+219.0*ya,
                      128.0+224.0*v, 16.0+219.0*yb);
    imageStore(target, p, uvec4(round(clamp(bytes, vec4(16), vec4(240,235,240,235)))));
}
)GLSL";
const char* Unpack = R"GLSL(
layout(binding=0) uniform usampler2D source;
layout(rgba8, binding=0) writeonly uniform image2D target;
vec4 rgb(float y, float u, float v) {
    vec3 w = weights();
    float r = y + 2.0*(1.0-w.r)*v;
    float b = y + 2.0*(1.0-w.b)*u;
    float g = (y-w.r*r-w.b*b)/w.g;
    return vec4(clamp(vec3(r,g,b),0.0,1.0),1.0);
}
void main() {
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    if (p.x >= size.x/2 || p.y >= size.y) return;
    vec4 bytes = vec4(texelFetch(source,p,0));
    float u = (bytes.x-128.0)/224.0, v = (bytes.z-128.0)/224.0;
    int y = invertY ? size.y-1-p.y : p.y;
    imageStore(target,ivec2(p.x*2,y),rgb((bytes.y-16.0)/219.0,u,v));
    imageStore(target,ivec2(p.x*2+1,y),rgb((bytes.w-16.0)/219.0,u,v));
}
)GLSL";

GLuint compile(const char* body) {
    const GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    const char* sources[] = {Common, body};
    glShaderSource(shader, 2, sources, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    GLuint program = 0;
    if (ok) {
        program = glCreateProgram();
        glAttachShader(program, shader);
        glLinkProgram(program);
        glGetProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[2048] = {};
            glGetProgramInfoLog(program, sizeof(log), nullptr, log);
            sgct::Log::Warning(std::string("NDI compute link: ") + log);
            glDeleteProgram(program);
            program = 0;
        }
    } else {
        char log[2048] = {};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        sgct::Log::Warning(std::string("NDI compute shader: ") + log);
    }
    glDeleteShader(shader);
    return program;
}

// Conversion runs inside existing renderers; preserve their bindings.
struct State {
    GLint program, active, texture, sampler;
    GLint image, level, layered, layer, access, format;
    State() {
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
        glGetIntegeri_v(GL_SAMPLER_BINDING, 0, &sampler);
        glGetIntegeri_v(GL_IMAGE_BINDING_NAME, 0, &image);
        glGetIntegeri_v(GL_IMAGE_BINDING_LEVEL, 0, &level);
        glGetIntegeri_v(GL_IMAGE_BINDING_LAYERED, 0, &layered);
        glGetIntegeri_v(GL_IMAGE_BINDING_LAYER, 0, &layer);
        glGetIntegeri_v(GL_IMAGE_BINDING_ACCESS, 0, &access);
        glGetIntegeri_v(GL_IMAGE_BINDING_FORMAT, 0, &format);
        glBindSampler(0, 0);
    }
    ~State() {
        glUseProgram(program);
        glBindImageTexture(0, image, level, layered, layer, access, format);
        glBindSampler(0, sampler);
        glBindTexture(GL_TEXTURE_2D, texture);
        glActiveTexture(active);
    }
};
}

bool NdiGpuConversion::initialize() {
    if (m_attempted) return m_packProgram && m_unpackProgram;
    m_attempted = true;
    GLint major = 0, minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    if (!GLAD_GL_VERSION_4_3 || major < 4 || (major == 4 && minor < 3)) {
        sgct::Log::Warning("NDI GPU conversion requires OpenGL 4.3; using CPU conversion");
        return false;
    }
    m_packProgram = compile(Pack);
    m_unpackProgram = compile(Unpack);
    return m_packProgram && m_unpackProgram;
}

bool NdiGpuConversion::allocate(int width, int height) {
    if (width <= 0 || height <= 0 || width % 2 || !initialize()) return false;
    GLint limit = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &limit);
    if (width > limit || height > limit) return false;
    if (!m_texture) glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    if (width != m_width || height != m_height) {
        // A bound unpack PBO would make nullptr an offset into that buffer.
        GLint pbo = 0;
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &pbo);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, width/2, height, 0,
                     GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, nullptr);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        m_width = width;
        m_height = height;
    }
    return true;
}

void NdiGpuConversion::dispatch(GLuint program, GLuint input, GLuint output,
                                int width, int height, bool invertY) {
    glUseProgram(program);
    glUniform2i(glGetUniformLocation(program, "size"), width, height);
    glUniform1i(glGetUniformLocation(program, "invertY"), invertY);
    glBindTexture(GL_TEXTURE_2D, input);
    glBindImageTexture(0, output, 0, GL_FALSE, 0, GL_WRITE_ONLY,
                       program == m_packProgram ? GL_RGBA8UI : GL_RGBA8);
    glDispatchCompute((width/2+15)/16, (height+15)/16, 1);
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT
                    | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

GLuint NdiGpuConversion::pack(GLuint rgbaTexture, int width, int height, bool invertY) {
    if (!rgbaTexture || !initialize()) return 0;
    State state;
    if (!allocate(width, height)) return 0;
    dispatch(m_packProgram, rgbaTexture, m_texture, width, height, invertY);
    return m_texture;
}

bool NdiGpuConversion::upload(GLuint rgbaTexture, int width, int height,
                              const unsigned char* uyvy, unsigned int stride) {
    if (!rgbaTexture || !uyvy || width <= 0 || stride < static_cast<unsigned int>(width)*2 || !initialize()) return false;
    State state;
    if (!allocate(width, height)) return false;
    GLint pbo, alignment, rowLength, skipRows, skipPixels;
    glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &pbo);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &rowLength);
    glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skipRows);
    glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skipPixels);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    // Upload only active bytes, also supporting strides not divisible by four.
    if (stride == static_cast<unsigned int>(width)*2) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width/2, height,
                        GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, uyvy);
    } else {
        for (int y = 0; y < height; ++y)
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y, width/2, 1,
                            GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, uyvy + static_cast<size_t>(y)*stride);
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, rowLength);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, skipRows);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, skipPixels);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    dispatch(m_unpackProgram, m_texture, rgbaTexture, width, height, true);
    return true;
}

void NdiGpuConversion::cleanup() {
    if (m_packProgram) glDeleteProgram(m_packProgram);
    if (m_unpackProgram) glDeleteProgram(m_unpackProgram);
    if (m_texture) glDeleteTextures(1, &m_texture);
    m_packProgram = m_unpackProgram = m_texture = 0;
    m_width = m_height = 0;
    m_attempted = false;
}
