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
// Read the SDK's native planes directly; no CPU color conversion or repacking.
const char* Unpack = R"GLSL(
layout(std430, binding=0) readonly buffer Frame { uint words[]; };
layout(rgba8, binding=0) writeonly uniform image2D target;
uniform int receiveFormat;
uniform uint stride;
uniform int inputHeight;
uniform int fieldParity;
uint byteAt(uint offset) {
    return (words[offset/4u] >> ((offset%4u)*8u)) & 255u;
}
float wordAt(uint offset) {
    return float(byteAt(offset) | (byteAt(offset+1u)<<8u));
}
vec4 rgb(float y, float u, float v, float alpha) {
    vec3 w = weights();
    float r = y + 2.0*(1.0-w.r)*v;
    float b = y + 2.0*(1.0-w.b)*u;
    float g = (y-w.r*r-w.b*b)/w.g;
    return vec4(clamp(vec3(r,g,b),0.0,1.0),alpha);
}
vec4 pixel(uint x, uint y) {
    uint plane = stride * uint(inputHeight);
    float luma, u, v, alpha = 1.0;
    if (receiveFormat >= 7) {
        uint offset = y*stride+x*4u;
        vec4 c = vec4(byteAt(offset),byteAt(offset+1u),byteAt(offset+2u),byteAt(offset+3u))/255.0;
        if (receiveFormat == 7 || receiveFormat == 8) c = c.bgra;
        if (receiveFormat == 8 || receiveFormat == 10) c.a = 1.0;
        return c;
    } else if (receiveFormat <= 1) {
        uint offset = y*stride+(x/2u)*4u;
        u = float(byteAt(offset));
        v = float(byteAt(offset+2u));
        luma = float(byteAt(offset+1u+(x%2u)*2u));
        if (receiveFormat == 1) alpha = float(byteAt(plane+y*(stride/2u)+x))/255.0;
    } else if (receiveFormat <= 4) {
        luma = float(byteAt(y*stride+x));
        if (receiveFormat == 2) {
            uint offset = plane+(y/2u)*stride+(x/2u)*2u;
            u = float(byteAt(offset)); v = float(byteAt(offset+1u));
        } else {
            uint chromaPlane = (stride/2u)*uint(inputHeight/2);
            uint offset = (y/2u)*(stride/2u)+x/2u;
            uint first = plane+offset, second = plane+chromaPlane+offset;
            u = float(byteAt(receiveFormat == 3 ? first : second));
            v = float(byteAt(receiveFormat == 3 ? second : first));
        }
    } else {
        luma = wordAt(y*stride+x*2u)/256.0;
        uint offset = plane+y*stride+(x/2u)*4u;
        u = wordAt(offset)/256.0; v = wordAt(offset+2u)/256.0;
        if (receiveFormat == 6) alpha = wordAt(plane*2u+y*stride+x*2u)/65535.0;
    }
    return rgb((luma-16.0)/219.0,(u-128.0)/224.0,(v-128.0)/224.0,alpha);
}
vec4 samplePixel(uint x, int y) {
    if (fieldParity < 0) return pixel(x,uint(y));
    // Bob reconstruction: retain the field's native scanlines and interpolate
    // missing scanlines. Clamp at the first/last row, never sample past the field.
    int row = int(floor(float(y-fieldParity)*0.5));
    vec4 a = pixel(x,uint(clamp(row,0,inputHeight-1)));
    if ((y%2) == fieldParity) return a;
    return (a+pixel(x,uint(clamp(row+1,0,inputHeight-1))))*0.5;
}
void main() {
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    int x = p.x*2;
    if (x >= size.x || p.y >= size.y) return;
    int y = invertY ? size.y-1-p.y : p.y;
    imageStore(target,ivec2(x,y),samplePixel(uint(x),p.y));
    if (x+1 < size.x) imageStore(target,ivec2(x+1,y),samplePixel(uint(x+1),p.y));
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
    glDispatchCompute(((width+1)/2+15)/16, (height+15)/16, 1);
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
                              const unsigned char* data, unsigned int stride, Format format, Field field) {
    const int type = static_cast<int>(format);
    if (!rgbaTexture || !data || width <= 0 || height <= 0 || type < 0 || type > 10) return false;
    if (field != Field::Progressive && field != Field::Upper && field != Field::Lower) return false;
    const bool rgb = type >= static_cast<int>(Format::BGRA);
    const bool planar420 = format == Format::NV12 || format == Format::I420 || format == Format::YV12;
    if ((!rgb && width%2) || (planar420 && height%2)) return false;
    const unsigned int bytesPerPixel = rgb ? 4 : planar420 ? 1 : 2;
    if (stride < static_cast<size_t>(width)*bytesPerPixel) return false;
    if ((format == Format::UYVA || format == Format::I420 || format == Format::YV12) && stride%2) return false;
    if (!initialize()) return false;
    GLint limit = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &limit);
    const bool fielded = field != Field::Progressive;
    if (width > limit || height > limit/(fielded ? 2 : 1)) return false;
    const int outputHeight = height*(fielded ? 2 : 1);
    size_t bytes = static_cast<size_t>(stride)*height;
    if (format == Format::UYVA || planar420) bytes += bytes/2;
    else if (format == Format::P216) bytes *= 2;
    else if (format == Format::PA16) bytes *= 3;
    const size_t bufferBytes = (bytes+3)&~size_t(3);
    GLint64 storageLimit = 0;
    glGetInteger64v(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &storageLimit);
    if (bufferBytes > static_cast<size_t>(storageLimit)) return false;

    State state;
    GLint binding = 0, indexedBinding = 0;
    GLint64 start = 0, length = 0;
    glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING, &binding);
    glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING, 0, &indexedBinding);
    glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_START, 0, &start);
    glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_SIZE, 0, &length);
    if (!m_uploadBuffer) glGenBuffers(1, &m_uploadBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_uploadBuffer);
    // Orphaning lets the driver retain the previous allocation until dispatch completes.
    glBufferData(GL_SHADER_STORAGE_BUFFER, bufferBytes, nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, bytes, data);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_uploadBuffer);
    glUseProgram(m_unpackProgram);
    glUniform1i(glGetUniformLocation(m_unpackProgram, "receiveFormat"), type);
    glUniform1ui(glGetUniformLocation(m_unpackProgram, "stride"), stride);
    glUniform1i(glGetUniformLocation(m_unpackProgram, "inputHeight"), height);
    glUniform1i(glGetUniformLocation(m_unpackProgram, "fieldParity"),
                field == Field::Upper ? 0 : field == Field::Lower ? 1 : -1);
    dispatch(m_unpackProgram, 0, rgbaTexture, width, outputHeight, true);
    if (indexedBinding && length > 0)
        glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 0, indexedBinding, start, length);
    else
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, indexedBinding);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, binding);
    return true;
}

void NdiGpuConversion::cleanup() {
    if (m_packProgram) glDeleteProgram(m_packProgram);
    if (m_unpackProgram) glDeleteProgram(m_unpackProgram);
    if (m_texture) glDeleteTextures(1, &m_texture);
    if (m_uploadBuffer) glDeleteBuffers(1, &m_uploadBuffer);
    m_packProgram = m_unpackProgram = m_texture = m_uploadBuffer = 0;
    m_width = m_height = 0;
    m_attempted = false;
}
