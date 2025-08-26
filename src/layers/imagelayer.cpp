/*
 * SPDX-FileCopyrightText:
 * 2024-2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "imagelayer.h"

auto loadImageAsync = [](ImageLayer::ImageData &data) {
    data.threadDone = false;
    data.threadRunning = true;
    data.img.load(data.filename);
    data.imageDone = true;
    while (!data.uploadDone) {
    }
    data.img = sgct::Image();
    data.threadDone = true;
};

ImageLayer::ImageLayer(std::string identifier) {
    imageData.identifier = identifier;
    imageData.filename = "";
    setType(BaseLayer::LayerType::IMAGE);
}

ImageLayer::~ImageLayer() {
    if (imageData.trd) {
        imageData.uploadDone = true;
        while (!imageData.threadDone) {
        }
        imageData.trd->join();
        imageData.trd = nullptr;
    }

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
    handleAsyncImageUpload();

    if (!imageData.threadRunning && forceUpdate) {
        if (filename.empty()) {
            sgct::Log::Info("Clearing background");
            imageData.filename = "";
        } else {
            if (fileIsImage(filename)) {
                // Load background file
                imageData.filename = filename;
                sgct::Log::Info(std::format("Loading new {} image asynchronously: {}", imageData.identifier, imageData.filename));
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
                std::filesystem::path bgPathExt = bgPath.extension();
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

void ImageLayer::handleAsyncImageUpload() {
    if (imageData.threadRunning) {
        if (imageData.imageDone && !imageData.uploadDone) {
            renderData.texId = sgct::TextureManager::instance().loadTexture(std::move(imageData.img));
            renderData.width = imageData.img.size().x;
            renderData.height = imageData.img.size().y;
            imageData.uploadDone = true;
        } else if (imageData.threadDone) {
            imageData.threadRunning = false;
            imageData.imageDone = false;
            imageData.uploadDone = false;
            imageData.trd->join();
            imageData.trd = nullptr;
        }
    }
}
