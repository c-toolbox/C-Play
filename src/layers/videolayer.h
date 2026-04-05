/*
 * SPDX-FileCopyrightText:
 * 2024-2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef VIDEOLAYER_H
#define VIDEOLAYER_H

#include <layers/mpvlayer.h>

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
    void updateFrame();
    virtual bool ready() const;

    void updateFbo();

private:
    void checkNeededMpvFboResize();
    void createMpvFBO(int width, int height);
    void generateTexture(unsigned int &id, int width, int height);

    // Ping-pong resources (master only)
    unsigned int m_pingPongFboId = 0;
    unsigned int m_pingPongTexId = 0;
    bool m_pingPongCreated = false;
    bool m_renderingToPingPong = false; // true => MPV renders to ping-pong FBO, display reads primary; false => vice-versa
};

#endif // VIDEOLAYER_H