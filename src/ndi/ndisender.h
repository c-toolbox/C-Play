/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef NDISENDER_H
#define NDISENDER_H

#include <sgct/opengl.h>
#include <atomic>
#include <functional>
#include <string>

#ifdef NDI_SUPPORT
#include <ndi/ofxNDI/ofxNDIsend.h>
#endif

class MpvObject;
class BaseLayer;

/**
 * Describes the object that provides the pixels to send over NDI.
 *
 * The source is intentionally kept as a set of small callbacks instead of a
 * class hierarchy, so that any object exposing an OpenGL texture (MpvObject /
 * MpvView FBO, LayerQtItem / BaseLayer texture, ...) can be adapted without
 * NdiSender having to know about it.
 */
struct NdiSenderSource {
    // Returns the OpenGL texture to read from, or 0 when not (yet) available.
    std::function<unsigned int()> textureId;
    // Native resolution of the texture.
    std::function<int()> width;
    std::function<int()> height;
    // True when the texture rows are stored bottom-up compared to what NDI
    // expects, so that the image has to be flipped vertically while sending.
    std::function<bool()> invertY;
    // Optional human readable name of the source, used for logging.
    std::string name;

    bool valid() const {
        return textureId && width && height;
    }
};

/**
 * Publishes the contents of a single source object as an NDI source at the
 * source's native resolution.
 *
 * All methods that touch OpenGL (captureAndSend and cleanupGL) must be called
 * on a thread with the source's OpenGL context current.
 */
class NdiSender {
public:
    NdiSender();
    ~NdiSender();

    // True when the application was built with NDI support.
    static bool isSupported();

    // Adapters for the supported source objects.
    static NdiSenderSource sourceFromMpvObject(MpvObject *mpv);
    static NdiSenderSource sourceFromLayer(BaseLayer *layer);

    void setSource(const NdiSenderSource &source);
    const NdiSenderSource &source() const;

    // Requests that a sender is created. The actual NDI sender is created
    // lazily in captureAndSend, once the source resolution is known.
    bool start(const std::string &senderName);
    void stop();

    bool isEnabled() const;
    bool isSending() const;

    const std::string &senderName() const;
    int width() const;
    int height() const;

    // Reads back the source texture and sends one video frame.
    // Must be called with the OpenGL context of the source current.
    bool captureAndSend();

    // Releases the OpenGL resources. Must be called with a current context.
    void cleanupGL();

private:
    bool createOrUpdateSender(int width, int height);
    void releaseSender();
    // Asynchronously reads the texture into the PBO ring and returns the
    // pixels captured a couple of frames ago, or nullptr when not ready yet.
    unsigned char *readPixels(unsigned int textureId, int width, int height, bool invertY);
    void releasePbos();

    // TODO: audio support. ofxNDIsend already provides SetAudio /
    // SetAudioData / SetAudioSampleRate etc. A future sendAudio() can feed
    // those from the source without changing the video path above.

#ifdef NDI_SUPPORT
    ofxNDIsend m_sender;
#endif

    NdiSenderSource m_source;
    std::string m_senderName;

    // Set from the GUI thread, read from the render thread.
    std::atomic_bool m_enabled = false;
    bool m_senderCreated = false;
    int m_width = 0;
    int m_height = 0;

    // PBOs used for asynchronous pixel readback, mirroring NdiLayer.
    GLuint m_pbo[3] = {0, 0, 0};
    int PboIndex = 0;
    int NextPboIndex = 0;
    // Number of frames pushed into the ring, used to skip the initial
    // incomplete reads.
    int m_framesCaptured = 0;

    // CPU side copy of the mapped PBO, handed to ofxNDIsend.
    unsigned char *m_frameBuffer = nullptr;
    size_t m_frameBufferSize = 0;
};

#endif // NDISENDER_H
