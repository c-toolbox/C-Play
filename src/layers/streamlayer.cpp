/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "streamlayer.h"
#include <utils/qrcommandprocessor.h>
#include <utils/qroperationhandler.h>
#include <utils/qroperationconfig.h>
#include <sgct/opengl.h>
#include <sgct/sgct.h>

StreamLayer::StreamLayer(gl_adress_func_v1 opa,
    bool allowDirectRendering,
    bool loggingOn,
    std::string logLevel,
    MpvLayer::onFileLoadedCallback flc) : VideoLayer(opa, allowDirectRendering, loggingOn, logLevel, flc) {
    m_data.isStream = true;
    setType(BaseLayer::LayerType::STREAM);

    m_qrProcessor = new QRCommandProcessor();
    m_qrProcessor->setCommandCallback([this](const QRCommand& cmd) {
        onQRCommand(cmd);
    });

    m_qrOpHandler = new QROperationHandler();
}

StreamLayer::~StreamLayer() {
    delete m_qrProcessor;
    delete m_qrOpHandler;

    if (m_readbackBuffer) {
        free(m_readbackBuffer);
        m_readbackBuffer = nullptr;
    }
}

void StreamLayer::initialize() {
    VideoLayer::initialize();
}

void StreamLayer::updateFrame() {
    if (m_typePropertiesDecoded) {
        m_typePropertiesDecoded = false;
        setQRCodeDetectionEnabled(m_qrCodeDetectionEnabled_Dec);
    }

    VideoLayer::updateFrame();

    // Scan for QR codes in the rendered frame (two-phase scheme)
    if (renderData.texId > 0 && renderData.width > 0 && renderData.height > 0) {
        FindCodes(renderData.texId, static_cast<unsigned int>(renderData.width), static_cast<unsigned int>(renderData.height));
    }

    // Update the active sublayer with the new frame
    if (m_qrOpHandler && m_qrOpHandler->isActive()) {
        m_qrOpHandler->updateActiveSubLayer(this, renderData.texId, renderData.width, renderData.height);
    }
}

bool StreamLayer::ready() const {
    return !m_data.loadedFile.empty();
}

void StreamLayer::encodeTypeProperties(std::vector<std::byte>& data) {
    MpvLayer::encodeTypeProperties(data);
    sgct::serializeObject(data, isQRCodeDetectionEnabled());
}

void StreamLayer::decodeTypeProperties(const std::vector<std::byte>& data, unsigned int& pos) {
    MpvLayer::decodeTypeProperties(data, pos);
    sgct::deserializeObject(data, pos, m_qrCodeDetectionEnabled_Dec);
    m_typePropertiesDecoded = true;
}

bool StreamLayer::isQRCodeDetectionEnabled() const {
    if (m_qrProcessor) {
        return m_qrProcessor->isEnabled();
    }
    return false;
}

void StreamLayer::setQRCodeDetectionEnabled(bool enabled) {
    if (m_qrProcessor) {
        m_qrProcessor->setEnabled(enabled);
    }
    if (!enabled && m_qrOpHandler) {
        m_qrOpHandler->clearAll();
    }

    if (isMaster())
        setNeedSync();
}

bool StreamLayer::loadQROperationConfig(const std::string& filePath) {
    if (m_qrOpHandler) {
        return m_qrOpHandler->loadConfig(filePath);
    }
    return false;
}

const QROperationConfig* StreamLayer::qrOperationConfig() const {
    if (m_qrOpHandler) {
        return m_qrOpHandler->config();
    }
    return nullptr;
}

std::string StreamLayer::activePlaneName() const {
    if (m_qrOpHandler) {
        return m_qrOpHandler->activePlaneName();
    }
    return std::string();
}

bool StreamLayer::FindCodes(unsigned int texId, unsigned int width, unsigned int height) {
    if (!m_qrProcessor || !m_qrProcessor->isEnabled()) {
        return true;
    }

    if (texId == 0 || width == 0 || height == 0) {
        return true;
    }

    // Ensure the readback buffer is large enough
    size_t requiredSize = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    if (m_readbackBufferSize != requiredSize) {
        if (m_readbackBuffer) {
            free(m_readbackBuffer);
        }
        m_readbackBuffer = static_cast<unsigned char*>(malloc(requiredSize));
        m_readbackBufferSize = requiredSize;
        if (!m_readbackBuffer) {
            sgct::Log::Error("StreamLayer: Failed to allocate readback buffer");
            m_readbackBufferSize = 0;
            return true;
        }
    }

    // Read back pixels from the texture
    glBindTexture(GL_TEXTURE_2D, texId);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_readbackBuffer);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Process frame through two-phase QR command scheme
    return m_qrProcessor->processFrame(m_readbackBuffer, width, height, GL_RGBA);
}

void StreamLayer::onQRCommand(const QRCommand& command) {
    if (m_qrOpHandler) {
        m_qrOpHandler->handleCommand(command, this, renderData.texId, renderData.width, renderData.height);
    }
}

bool StreamLayer::hasSubLayers() const {
    return m_qrOpHandler ? m_qrOpHandler->hasSubLayers() : false;
}

std::vector<std::shared_ptr<BaseLayer>>& StreamLayer::getSubLayers() const {
    if (m_qrOpHandler) {
        return m_qrOpHandler->subLayers();
    }
    static std::vector<std::shared_ptr<BaseLayer>> empty;
    return empty;
}
