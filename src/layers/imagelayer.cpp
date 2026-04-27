/*
 * SPDX-FileCopyrightText:
 * 2024-2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "imagelayer.h"
#include <sgct/opengl.h>

#ifdef SAIL_SUPPORT
#include <sail-c++/sail-c++.h>
#endif

auto loadImageAsync = [](ImageLayer::ImageData &data) {
    data.threadDone = false;
    data.threadRunning = true;

#ifdef SAIL_SUPPORT
    data.usedSail = false;
    // Try SAIL first for broader format support
    try {
        sail::image_input input(data.filename);
        sail::image sailImg = input.next_frame();

        // Convert to RGBA for consistent GL upload
        sailImg.convert(SAIL_PIXEL_FORMAT_BPP32_RGBA);

        data.sailWidth = sailImg.width();
        data.sailHeight = sailImg.height();
        data.sailChannels = 4; // RGBA

        size_t dataSize = static_cast<size_t>(data.sailWidth) * data.sailHeight * data.sailChannels;
        const unsigned char* pixels = reinterpret_cast<const unsigned char*>(sailImg.pixels());

        // Flip vertically to match OpenGL convention (bottom-to-top)
        data.sailPixels.resize(dataSize);
        size_t rowBytes = static_cast<size_t>(data.sailWidth) * data.sailChannels;
        for (int y = 0; y < data.sailHeight; y++) {
            const unsigned char* srcRow = pixels + static_cast<size_t>(y) * sailImg.bytes_per_line();
            unsigned char* dstRow = data.sailPixels.data() + static_cast<size_t>(data.sailHeight - 1 - y) * rowBytes;
            std::memcpy(dstRow, srcRow, rowBytes);
        }

        data.usedSail = true;
    } catch (...) {
        // SAIL failed, fall back to sgct::Image (stb_image)
        data.usedSail = false;
        data.sailPixels.clear();
        try {
            data.img.load(data.filename);
        } catch (...) {
            data.img = sgct::Image();
            data.threadDone = true;
            data.threadRunning = false;
            return;
        }
    }
#else
    data.img.load(data.filename);
#endif

    if (data.abortRequested) {
#ifdef SAIL_SUPPORT
        data.sailPixels.clear();
#endif
        data.img = sgct::Image();
        data.threadDone = true;
        data.threadRunning = false;
        return;
    }

    data.imageDone = true;
    while (!data.uploadDone && !data.abortRequested) {
        std::this_thread::yield();
    }
#ifdef SAIL_SUPPORT
    data.sailPixels.clear();
    data.sailPixels.shrink_to_fit();
#endif
    data.img = sgct::Image();
    data.threadDone = true;
    data.threadRunning = false;
};

ImageLayer::ImageLayer(std::string identifier) {
    imageData.identifier = identifier;
    imageData.filename = "";
    setType(BaseLayer::LayerType::IMAGE);
}

ImageLayer::~ImageLayer() {
    imageData.abortRequested = true;
    imageData.uploadDone = true;
    joinThread();

    if (renderData.texId > 0)
        sgct::TextureManager::instance().removeTexture(renderData.texId);
}

void ImageLayer::initialize() {
    m_hasInitialized = true;
}

void ImageLayer::update(bool updateRendering) {
    if(updateRendering || !ready())
        processImageUpload(filepath(), imageData.filename != filepath());
}

bool ImageLayer::ready() const {
    return !imageData.filename.empty() && imageData.threadDone;
}

bool ImageLayer::hasTexture() const {
    return true;
}

bool ImageLayer::processImageUpload(std::string filename, bool forceUpdate) {
    std::lock_guard<std::mutex> lock(m_updateMutex);
    handleAsyncImageUpload();

    if (!imageData.threadRunning && forceUpdate) {
        if (filename.empty()) {
            sgct::Log::Info("Clearing background");
            imageData.filename = "";
        } else {
            if (fileIsImage(filename)) {
                // Join any previous thread before starting a new one
                joinThread();

                // Store filename before launching thread to avoid race on the string
                imageData.filename = filename;
                imageData.abortRequested = false;
                imageData.imageDone = false;
                imageData.uploadDone = false;
                imageData.threadDone = false;
                sgct::Log::Info(std::format("Loading new {} image asynchronously: {}", imageData.identifier, imageData.filename));
                imageData.threadRunning = true;
                imageData.trd = std::make_unique<std::thread>(loadImageAsync, std::ref(imageData));

                return true;
            } else {
                imageData.filename = "";
            }
        }
    }

    return false;
}

std::string ImageLayer::loadedFile() {
    return imageData.filename;
}

bool ImageLayer::fileIsImage(std::string &filePath) {
    if (!filePath.empty()) {
        if (std::filesystem::exists(filePath)) {
            std::filesystem::path bgPath = std::filesystem::path(filePath);
            if (bgPath.has_extension()) {
                std::string bgPathExt = bgPath.extension().generic_string();
                std::transform(bgPathExt.begin(), bgPathExt.end(), bgPathExt.begin(),
                    [](unsigned char c) { return std::tolower(c); });
                if (bgPathExt == ".png" ||
                    bgPathExt == ".jpg" ||
                    bgPathExt == ".jpeg" ||
                    bgPathExt == ".tga") {
                    return true;
                }
#ifdef SAIL_SUPPORT
                // Query SAIL to check if the extension is supported
                // Extension without the leading dot
                std::string extNoDot = bgPathExt.substr(1);
                sail::codec_info ci = sail::codec_info::from_extension(extNoDot);
                if (ci.is_valid()) {
                    return true;
                }
#endif
                sgct::Log::Warning(std::format("Image file extension is not supported: {}", filePath));
            } else {
                sgct::Log::Warning(std::format("Image file has no extension: {}", filePath));
            }
        } else {
            sgct::Log::Warning(std::format("Could not find image file: {}", filePath));
        }
    } else {
        sgct::Log::Warning(std::format("Image file is empty: {}", filePath));
    }

    return false;
}

void ImageLayer::joinThread() {
    if (imageData.trd) {
        if (imageData.trd->joinable()) {
            imageData.trd->join();
        }
        imageData.trd.reset();
    }
}

void ImageLayer::handleAsyncImageUpload() {
    if (imageData.threadRunning) {
        if (imageData.imageDone && !imageData.uploadDone) {
#ifdef SAIL_SUPPORT
            if (imageData.usedSail) {
                // Upload SAIL-decoded pixels directly to OpenGL
                GLuint texId = 0;
                glGenTextures(1, &texId);
                glBindTexture(GL_TEXTURE_2D, texId);
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glTexImage2D(
                    GL_TEXTURE_2D, 0, GL_RGBA8,
                    imageData.sailWidth, imageData.sailHeight, 0,
                    GL_RGBA, GL_UNSIGNED_BYTE,
                    imageData.sailPixels.data()
                );
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glBindTexture(GL_TEXTURE_2D, 0);

                renderData.texId = texId;
                renderData.width = imageData.sailWidth;
                renderData.height = imageData.sailHeight;
                imageData.uploadDone = true;
            } else
#endif
            {
                // Save dimensions before moving the image
                int imgWidth = imageData.img.size().x;
                int imgHeight = imageData.img.size().y;
                renderData.texId = sgct::TextureManager::instance().loadTexture(std::move(imageData.img));
                renderData.width = imgWidth;
                renderData.height = imgHeight;
                imageData.uploadDone = true;
            }
        } else if (imageData.threadDone) {
            imageData.threadRunning = false;
            imageData.imageDone = false;
            imageData.uploadDone = false;
            joinThread();
        }
    }
}
