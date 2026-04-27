/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "omtlayer.h"
#include <sgct/sgct.h>

OmtFinder* OmtFinder::_instance = nullptr;

OmtFinder::OmtFinder() {
}

OmtFinder::~OmtFinder() {
}

OmtFinder& OmtFinder::instance() {
    if (!_instance) {
        _instance = new OmtFinder();
    }
    return *_instance;
}

std::vector<std::string> OmtFinder::getSendersList() {
    std::vector<std::string> sendersList;
    int count = 0;
    char** addresses = omt_discovery_getaddresses(&count);
    if (addresses && count > 0) {
        for (int i = 0; i < count; i++) {
            if (addresses[i]) {
                sendersList.push_back(addresses[i]);
            }
        }
    }
    return sendersList;
}

bool OmtFinder::senderExists(const std::string& senderName) {
    std::vector<std::string> senders = getSendersList();
    for (const auto& s : senders) {
        if (s == senderName) {
            return true;
        }
    }
    return false;
}

std::string OmtFinder::getOMTVersionString() {
    return "1.0";
}

OmtLayer::OmtLayer() {
    setType(BaseLayer::LayerType::OMT);
}

OmtLayer::~OmtLayer() {
    if (m_receiver) {
        omt_receive_destroy(m_receiver);
        m_receiver = nullptr;
    }

    if (renderData.texId > 0) {
        glDeleteTextures(1, &renderData.texId);
    }
}

void OmtLayer::initialize() {
    m_hasInitialized = true;
}

void OmtLayer::update(bool updateRendering) {
    // Check if sender exists
    if (!OmtFinder::instance().senderExists(filepath())) {
        m_isReady = false;
        if (m_receiver) {
            omt_receive_destroy(m_receiver);
            m_receiver = nullptr;
        }
        return;
    }

    // Create receiver if needed
    if (!m_receiver) {
        m_receiver = omt_receive_create(
            filepath().c_str(),
            OMTFrameType_Video,
            OMTPreferredVideoFormat_BGRA,
            OMTReceiveFlags_None
        );
        if (!m_receiver) {
            sgct::Log::Error("OmtLayer Error: Failed to create OMT receiver.\n");
            m_isReady = false;
            return;
        }
    }

    if (m_receiver) {
        m_isReady = true;
    }

    // Receive a video frame
    if (updateRendering) {
        updateFrame();
    }
}

void OmtLayer::updateFrame() {
    if (!m_receiver) {
        return;
    }

    OMTMediaFrame* frame = omt_receive(m_receiver, OMTFrameType_Video, 0);
    if (!frame || frame->Type != OMTFrameType_Video || !frame->Data || frame->DataLength <= 0) {
        // No frame available yet, but receiver is created
        return;
    }

    unsigned int width = static_cast<unsigned int>(frame->Width);
    unsigned int height = static_cast<unsigned int>(frame->Height);

    // Check for changed sender dimensions
    if (width != static_cast<unsigned int>(renderData.width) ||
        height != static_cast<unsigned int>(renderData.height)) {
        if (renderData.width > 0) {
            glDeleteTextures(1, &renderData.texId);
        }
        GenerateTexture(renderData.texId, width, height);
        renderData.width = static_cast<int>(width);
        renderData.height = static_cast<int>(height);
    }

    // Upload pixel data to texture
    glBindTexture(GL_TEXTURE_2D, renderData.texId);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
        GL_BGRA, GL_UNSIGNED_BYTE, frame->Data);
}

bool OmtLayer::ready() const {
    return m_isReady;
}

bool OmtLayer::hasTexture() const {
    return true;
}

void OmtLayer::GenerateTexture(unsigned int& id, int width, int height) {
    glGenTextures(1, &id);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);

    // Disable mipmaps
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}
