/*
 * SPDX-FileCopyrightText:
 * 2024-2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "imagelayer.h"

auto loadImageAsync = [](ImageLayer::ImageData &data) {
    data.threadDone = false;
    data.threadRunning = true;
    data.img.load(data.filename);

    if (data.abortRequested) {
        data.img = sgct::Image();
        data.threadDone = true;
        data.threadRunning = false;
        return;
    }

    data.imageDone = true;
    while (!data.uploadDone && !data.abortRequested) {
        std::this_thread::yield();
    }
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
        // Load background file
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
                } else {
                    sgct::Log::Warning(std::format("Image file extension is not supported: {}", filePath));
                }
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
            // Save dimensions before moving the image
            int imgWidth = imageData.img.size().x;
            int imgHeight = imageData.img.size().y;
            renderData.texId = sgct::TextureManager::instance().loadTexture(std::move(imageData.img));
            renderData.width = imgWidth;
            renderData.height = imgHeight;
            imageData.uploadDone = true;
        } else if (imageData.threadDone) {
            imageData.threadRunning = false;
            imageData.imageDone = false;
            imageData.uploadDone = false;
            joinThread();
        }
    }
}
