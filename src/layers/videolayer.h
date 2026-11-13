/*
 * SPDX-FileCopyrightText:
 * 2024-2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef VIDEOLAYER_H
#define VIDEOLAYER_H

#include <layers/mpvlayer.h>
#include <mutex>
#include <vector>

class VideoLayer : public MpvLayer {
public:
    VideoLayer(gl_adress_func_v1 opa,
             bool allowDirectRendering = false,
             bool loggingOn = false,
             std::string logLevel = "info",
             MpvLayer::onFileLoadedCallback flc = nullptr);
    ~VideoLayer();

    virtual void initialize();
    void initializeGL();
    void cleanup();
    void update(bool updateRendering = true) override;
    void updateFrame();
    virtual bool ready() const;

    unsigned int textureInternalFormat() const override;
    unsigned int acquireTextureForRead() override;
    void releaseTextureForRead(unsigned int texId) override;

    void updateFbo();

private:
    // Index of texId among the primary (0) and ping-pong (1) textures, or -1. Needs m_readMutex.
    int bufferIndex(unsigned int texId) const;
    // Render thread: makes this context's GPU wait for all reads of the buffer. Needs m_readMutex.
    void waitForBufferReads(int index);
    void deleteReadFences();

    void applyPendingVideoSize();
    void checkNeededMpvFboResize();
    void createMpvFBO(int width, int height);
    void generateTexture(unsigned int &id, int width, int height);

    // Primary FBO texture (always tracks the texture attached to m_data.fboId)
    unsigned int m_primaryTexId = 0;

    // Ping-pong resources (master only)
    unsigned int m_pingPongFboId = 0;
    unsigned int m_pingPongTexId = 0;
    bool m_pingPongCreated = false;
    bool m_renderingToPingPong = false; // true => MPV renders to ping-pong FBO, display reads primary; false => vice-versa
    bool m_redrawNeeded = true; // The display buffer lacks the current frame, so render even without a new one

    // Reads of the buffers from other windows' contexts (LayerView), so mpv never renders into a
    // buffer that is still being sampled: a held buffer is skipped, and the GPU waits for the
    // fences of finished reads before writing.
    std::mutex m_readMutex;
    int m_readHolds[2] = {0, 0};
    std::vector<struct __GLsync *> m_readFences[2];
};

#endif // VIDEOLAYER_H