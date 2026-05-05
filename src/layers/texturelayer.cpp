/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "texturelayer.h"
#include <sgct/sgct.h>
#include <cstring>

TextureLayer::TextureLayer(const std::string& name) {
    setType(BaseLayer::LayerType::BASE);
    setTitle(name);
}

TextureLayer::~TextureLayer() {
    releaseTexture();
}

void TextureLayer::initialize() {
    m_hasInitialized = true;
}

bool TextureLayer::ready() const {
    return m_hasTexture && renderData.texId > 0;
}

bool TextureLayer::copyFromTexture(GLuint srcTexId, int width, int height, GLenum internalFormat) {
    if (srcTexId == 0 || width <= 0 || height <= 0) {
        return false;
    }

    // If we were pointing to an external texture, clear that state first
    if (m_pointingToExternal) {
        renderData.texId = 0;
        renderData.width = 0;
        renderData.height = 0;
        m_hasTexture = false;
        m_pointingToExternal = false;
    }

    // Allocate destination texture if needed (size or format changed)
    bool reallocated = false;
    if (renderData.texId == 0 || renderData.width != width || renderData.height != height || m_internalFormat != internalFormat) {
        releaseTexture();
        GLenum pixelFormat = (internalFormat == GL_RGBA16F) ? GL_RGBA : GL_RGBA;
        allocateTexture(width, height, internalFormat, pixelFormat);
        reallocated = true;
    }

    // Copy source texture to our owned texture using glCopyImageSubData
    glCopyImageSubData(
        srcTexId, GL_TEXTURE_2D, 0, 0, 0, 0,
        renderData.texId, GL_TEXTURE_2D, 0, 0, 0, 0,
        width, height, 1
    );

    m_hasTexture = true;
    if (reallocated) {
        sgct::Log::Info(std::format("TextureLayer '{}': Copied texture ({}x{}, fmt={}) from texture {}",
            title(), width, height, internalFormat, srcTexId));
    }
    return true;
}

bool TextureLayer::upload(const unsigned char* pixelData, int width, int height, int GLformat, GLenum internalFormat) {
    if (!pixelData || width <= 0 || height <= 0) {
        return false;
    }

    if (GLformat != GL_RGBA && GLformat != GL_BGRA) {
        return false;
    }

    // Allocate destination texture if needed
    if (renderData.texId == 0 || renderData.width != width || renderData.height != height || m_internalFormat != internalFormat) {
        releaseTexture();
        allocateTexture(width, height, internalFormat, GLformat);
    }

    glBindTexture(GL_TEXTURE_2D, renderData.texId);
    GLenum type = (internalFormat == GL_RGBA16F) ? GL_FLOAT : GL_UNSIGNED_BYTE;
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GLformat, type, pixelData);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_hasTexture = true;
    return true;
}

void TextureLayer::pointToTexture(GLuint texId, int width, int height) {
    // If we own a texture, release it first
    if (!m_pointingToExternal && renderData.texId > 0) {
        glDeleteTextures(1, &renderData.texId);
    }

    renderData.texId = texId;
    renderData.width = width;
    renderData.height = height;
    m_pointingToExternal = true;
    m_hasTexture = (texId > 0 && width > 0 && height > 0);
}

void TextureLayer::releaseTexture() {
    if (!m_pointingToExternal && renderData.texId > 0) {
        glDeleteTextures(1, &renderData.texId);
    }
    renderData.texId = 0;
    renderData.width = 0;
    renderData.height = 0;
    m_hasTexture = false;
    m_frozen = false;
    m_pointingToExternal = false;
    m_internalFormat = 0;
}

bool TextureLayer::isFrozen() const {
    return m_frozen;
}

void TextureLayer::setFrozen(bool frozen) {
    m_frozen = frozen;
}

bool TextureLayer::hasTexture() const {
    return m_hasTexture;
}

void TextureLayer::startFadeTo(float targetAlpha, int durationMs) {
    if (durationMs <= 0) {
        m_fading = false;
        setAlpha(targetAlpha);
        return;
    }

    m_fadeFrom = alpha();
    m_fadeTo = targetAlpha;
    m_fadeDurationSec = static_cast<float>(durationMs) / 1000.f;
    m_fadeStart = std::chrono::steady_clock::now();
    m_fading = true;
}

bool TextureLayer::tickFade() {
    if (!m_fading)
        return false;

    auto now = std::chrono::steady_clock::now();
    float elapsed = std::chrono::duration<float>(now - m_fadeStart).count();
    float t = (m_fadeDurationSec > 0.f) ? (elapsed / m_fadeDurationSec) : 1.f;

    if (t >= 1.f) {
        m_fading = false;
        setAlpha(m_fadeTo);
        return false;
    }

    float current = m_fadeFrom + (m_fadeTo - m_fadeFrom) * t;
    setAlpha(current);
    return true;
}

bool TextureLayer::isFading() const {
    return m_fading;
}

void TextureLayer::allocateTexture(int width, int height, GLenum internalFormat, GLenum pixelFormat) {
    GLuint texId = 0;
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Use the caller-specified internal format (e.g. GL_RGBA8 for NDI, GL_RGBA16F for streams)
    GLenum type = (internalFormat == GL_RGBA16F) ? GL_FLOAT : GL_UNSIGNED_BYTE;
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, pixelFormat, type, nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);

    renderData.texId = texId;
    renderData.width = width;
    renderData.height = height;
    m_internalFormat = internalFormat;
}
