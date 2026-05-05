/*
 * SPDX-FileCopyrightText:
 * 2024-2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef IMAGELAYER_H
#define IMAGELAYER_H

#include <layers/baselayer.h>
#include <sgct/sgct.h>
#include <vector>
#include <memory>

#ifdef SAIL_SUPPORT
namespace sail { class image; }
#endif

class ImageLayer : public BaseLayer {
public:
    struct ImageData {
        std::string filename = "";
        std::string identifier = "";
        sgct::Image img;
#ifdef SAIL_SUPPORT
        // When SAIL is used, the decoded image is kept alive until GPU upload completes
        std::shared_ptr<sail::image> sailImage;
        int sailWidth = 0;
        int sailHeight = 0;
        unsigned int sailGLFormat = 0x1908; // GL_RGBA
        unsigned int sailGLInternalFormat = 0x8058; // GL_RGBA8
        bool usedSail = false;
#endif
        std::unique_ptr<std::thread> trd;
        std::atomic_bool threadRunning = false;
        std::atomic_bool imageDone = false;
        std::atomic_bool uploadDone = false;
        std::atomic_bool threadDone = false;
        std::atomic_bool abortRequested = false;
    };

    ImageLayer(std::string identifier);
    ~ImageLayer();

    void initialize();
    void update(bool updateRendering = true);
    void updateFrame();
    bool ready() const;
    bool hasTexture() const override;

    bool processImageUpload(std::string filename, bool forceUpdate);
    std::string loadedFile();

    bool fileIsImage(std::string &filePath);

private:
    ImageData imageData;

    void joinThread();
    void handleAsyncImageUpload();
};

#endif // IMAGELAYER_H