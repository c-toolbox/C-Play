/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ndisender.h"

#include <layers/baselayer.h>
#include <mpvobject.h>

#ifdef NDI_SUPPORT
#include <ndi/ofxNDI/ofxNDIutils.h>
#include <sgct/log.h>
#include <format>
#endif

NdiSender::NdiSender() {}

NdiSender::~NdiSender() {
    // OpenGL resources cannot be released here since there is no guarantee of
    // a current context. cleanupGL() must have been called before.
    releaseSender();
    delete[] m_frameBuffer;
    m_frameBuffer = nullptr;
    m_frameBufferSize = 0;
}

bool NdiSender::isSupported() {
#ifdef NDI_SUPPORT
    return true;
#else
    return false;
#endif
}

NdiSenderSource NdiSender::sourceFromMpvObject(MpvObject *mpv) {
    NdiSenderSource source;
    if (!mpv)
        return source;

    source.name = "MpvObject";
    source.textureId = [mpv]() -> unsigned int { return mpv->fboTextureId(); };
    source.width = [mpv]() -> int { return mpv->fboWidth(); };
    source.height = [mpv]() -> int { return mpv->fboHeight(); };
    // The mpv FBO is already stored in the orientation NDI expects.
    source.invertY = []() -> bool { return false; };
    return source;
}

NdiSenderSource NdiSender::sourceFromLayer(BaseLayer *layer) {
    NdiSenderSource source;
    if (!layer)
        return source;

    source.name = layer->title();
    source.textureId = [layer]() -> unsigned int {
        return layer->hasTexture() ? layer->textureId() : 0u;
    };
    source.width = [layer]() -> int { return layer->width(); };
    source.height = [layer]() -> int { return layer->height(); };
    // Layer textures are bottom-up, but a layer that is already flagged flipY
    // stores its rows the other way around, so the two cancel out.
    source.invertY = [layer]() -> bool { return !layer->flipY(); };
    return source;
}

void NdiSender::setSource(const NdiSenderSource &source) {
    m_source = source;
    // Force the sender to be re-created with the new source resolution.
    m_width = 0;
    m_height = 0;
    m_framesCaptured = 0;
}

const NdiSenderSource &NdiSender::source() const {
    return m_source;
}

bool NdiSender::start(const std::string &senderName) {
    if (!isSupported())
        return false;

    // The source must be in place before the render thread observes m_enabled.
    m_senderName = senderName;
    m_enabled = true;
    return true;
}

void NdiSender::stop() {
    // Only flag the intent here. The NDI sender and the OpenGL resources are
    // torn down in cleanupGL(), which must run on the render thread.
    m_enabled = false;
}

bool NdiSender::isEnabled() const {
    return m_enabled;
}

bool NdiSender::isSending() const {
    return m_enabled && m_senderCreated;
}

const std::string &NdiSender::senderName() const {
    return m_senderName;
}

int NdiSender::width() const {
    return m_width;
}

int NdiSender::height() const {
    return m_height;
}

#ifdef NDI_SUPPORT

bool NdiSender::createOrUpdateSender(int width, int height) {
    if (width <= 0 || height <= 0)
        return false;

    if (m_senderCreated) {
        if (m_width == width && m_height == height)
            return true;

        // Resolution changed, tell NDI about the new dimensions.
        if (!m_sender.UpdateSender(static_cast<unsigned int>(width),
                                   static_cast<unsigned int>(height))) {
            sgct::Log::Error(std::format("NdiSender Error: could not update sender to {}x{}", width, height));
            return false;
        }

        m_width = width;
        m_height = height;
        releasePbos();
        m_framesCaptured = 0;
        return true;
    }

    m_sender.SetFormat(NDIlib_FourCC_video_type_RGBA);
    // Asynchronous sending keeps the render thread free, the PBO ring already
    // guarantees that the buffer handed over stays valid for several frames.
    m_sender.SetAsync(true);

    if (!m_sender.CreateSender(m_senderName.c_str(),
                               static_cast<unsigned int>(width),
                               static_cast<unsigned int>(height))) {
        sgct::Log::Error(std::format("NdiSender Error: could not create sender \"{}\"", m_senderName));
        return false;
    }

    m_senderCreated = true;
    m_width = width;
    m_height = height;
    m_framesCaptured = 0;

    sgct::Log::Info(std::format("NdiSender: sending \"{}\" at {}x{}", m_senderName, width, height));

    return true;
}

void NdiSender::releaseSender() {
    if (m_senderCreated) {
        m_sender.ReleaseSender();
        m_senderCreated = false;
    }
    m_width = 0;
    m_height = 0;
}

void NdiSender::releasePbos() {
    if (m_pbo[0]) {
        glDeleteBuffers(3, m_pbo);
        m_pbo[0] = m_pbo[1] = m_pbo[2] = 0;
    }
    PboIndex = NextPboIndex = 0;
}

// Streaming texture pixel readback, the inverse of NdiLayer::LoadTexturePixels.
// The read into the PBO is asynchronous, the buffer mapped is the one filled a
// couple of frames earlier, so the GPU is never stalled.
unsigned char *NdiSender::readPixels(unsigned int textureId, int width, int height, bool invertY) {
    const size_t dataSize = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;

    if (!m_pbo[0]) {
        glGenBuffers(3, m_pbo);
        PboIndex = NextPboIndex = 0;
        for (int i = 0; i < 3; i++) {
            glBindBuffer(GL_PIXEL_PACK_BUFFER, m_pbo[i]);
            glBufferData(GL_PIXEL_PACK_BUFFER, dataSize, 0, GL_STREAM_READ);
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    }

    if (m_frameBufferSize != dataSize) {
        delete[] m_frameBuffer;
        m_frameBuffer = new unsigned char[dataSize];
        m_frameBufferSize = dataSize;
    }

    PboIndex = (PboIndex + 1) % 3;
    NextPboIndex = (PboIndex + 1) % 3;

    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    // Start an asynchronous read of the current texture into the current PBO.
    glBindBuffer(GL_PIXEL_PACK_BUFFER, m_pbo[PboIndex]);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_framesCaptured++;

    // The oldest PBO holds a completed transfer, map that one.
    glBindBuffer(GL_PIXEL_PACK_BUFFER, m_pbo[NextPboIndex]);
    unsigned char *pboMemory = static_cast<unsigned char *>(glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY));

    unsigned char *result = nullptr;
    if (pboMemory) {
        // The ring has to be filled before the trailing buffer holds a
        // complete frame, otherwise the first frames would be garbage.
        if (m_framesCaptured > 2) {
            // The flip is applied here, once. Both this copy and
            // ofxNDIsend::SendImage can invert, so SendImage is always called
            // with bInvert=false to avoid cancelling this one out.
            ofxNDIutils::CopyImage(pboMemory, m_frameBuffer, width, height, invertY);
            result = m_frameBuffer;
        }
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    }

    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    return result;
}

bool NdiSender::captureAndSend() {
    if (!m_enabled || !m_source.valid())
        return false;

    const unsigned int textureId = m_source.textureId();
    const int width = m_source.width();
    const int height = m_source.height();

    if (textureId == 0 || width <= 0 || height <= 0)
        return false;

    if (!createOrUpdateSender(width, height))
        return false;

    // Textures read back with glGetTexImage are bottom-up. A source can opt out
    // when it already stores its rows top-down.
    const bool invertY = m_source.invertY ? m_source.invertY() : true;

    unsigned char *pixels = readPixels(textureId, width, height, invertY);
    if (!pixels)
        return false;

    // readPixels has already applied the vertical flip, so no second invert here.
    return m_sender.SendImage(pixels,
                              static_cast<unsigned int>(width),
                              static_cast<unsigned int>(height),
                              /*bSwapRB=*/false,
                              /*bInvert=*/false);
}

void NdiSender::cleanupGL() {
    releasePbos();
    releaseSender();
    m_framesCaptured = 0;
}

#else // !NDI_SUPPORT

bool NdiSender::createOrUpdateSender(int, int) {
    return false;
}

void NdiSender::releaseSender() {
    m_senderCreated = false;
    m_width = 0;
    m_height = 0;
}

void NdiSender::releasePbos() {
}

unsigned char *NdiSender::readPixels(unsigned int, int, int, bool) {
    return nullptr;
}

bool NdiSender::captureAndSend() {
    return false;
}

void NdiSender::cleanupGL() {
}

#endif // NDI_SUPPORT
