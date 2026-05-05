/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TEXTURELAYER_H
#define TEXTURELAYER_H

#include <layers/baselayer.h>
#include <sgct/opengl.h>
#include <string>
#include <chrono>

// A lightweight layer that owns a static OpenGL texture (a "frozen" snapshot).
// It can be used as a sublayer by any parent layer (e.g. NdiLayer) to hold
// a copy of a live capture frame at a specific point in time.
//
// The texture is created by copying pixel data from the parent's current
// frame via copyFromTexture() or upload().
// Once captured, the TextureLayer is fully self-contained and renders
// the static image until released.
//
// Supports timed fade transitions via startFadeTo(). The fade is driven
// by calling tickFade() every frame (typically from QROperationHandler).
class TextureLayer : public BaseLayer {
public:
    explicit TextureLayer(const std::string& name = "FrozenTexture");
    ~TextureLayer() override;

    void initialize() override;
    bool ready() const override;

    // Copy a snapshot from an existing GL texture into this layer's owned texture.
    // internalFormat must match the source texture's internal format (e.g. GL_RGBA8, GL_RGBA16F).
    bool copyFromTexture(GLuint srcTexId, int width, int height, GLenum internalFormat = GL_RGBA8);

    // Point to an external texture without copying. The TextureLayer does NOT own
    // the texture and will not delete it. Call releaseTexture() or pointToTexture(0,...)
    // to stop referencing the external texture.
    void pointToTexture(GLuint texId, int width, int height);

    // Upload a snapshot from raw RGBA/BGRA pixel data.
    bool upload(const unsigned char* pixelData, int width, int height, int GLformat, GLenum internalFormat = GL_RGBA8);

    // Release the owned texture and mark as not ready.
    void releaseTexture();

    // Whether this layer is in frozen (static snapshot) mode.
    bool isFrozen() const;

    // Set whether this layer is in frozen mode.
    void setFrozen(bool frozen);

    // Whether this layer holds a valid texture (independent of frozen state).
    bool hasTexture() const override;

    // ---- Fade support ----

    // Start a timed fade from the current alpha to targetAlpha over durationMs milliseconds.
    // If durationMs <= 0, the alpha is set immediately.
    void startFadeTo(float targetAlpha, int durationMs);

    // Advance the fade by the elapsed wall-clock time. Returns true if a fade is in progress.
    bool tickFade();

    // Whether a fade is currently in progress.
    bool isFading() const;

private:
    void allocateTexture(int width, int height, GLenum internalFormat, GLenum pixelFormat);

    bool m_frozen = false;
    bool m_hasTexture = false;
    bool m_pointingToExternal = false;
    GLenum m_internalFormat = 0;

    // Fade state
    bool m_fading = false;
    float m_fadeFrom = 0.f;
    float m_fadeTo = 0.f;
    float m_fadeDurationSec = 0.f;
    std::chrono::steady_clock::time_point m_fadeStart;
};

#endif // TEXTURELAYER_H
