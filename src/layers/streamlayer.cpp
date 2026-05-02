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
#include <utils/dividetexturehandler.h>
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
    delete m_divideTexHandler;

    if (m_readbackBuffer) {
        free(m_readbackBuffer);
        m_readbackBuffer = nullptr;
    }

    if (m_backupTexId > 0) {
        glDeleteTextures(1, &m_backupTexId);
        m_backupTexId = 0;
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

    // After VideoLayer::updateFrame(), renderData.texId holds the just-rendered
    // frame (either the primary or ping-pong texture). We save this as the
    // "current render texture" before potentially swapping to the backup.
    unsigned int currentRenderTexId = renderData.texId;

    // Scan for QR codes in the rendered frame (two-phase scheme)
    if (isQRCodeDetectionEnabled() && currentRenderTexId > 0 && renderData.width > 0 && renderData.height > 0) {
        bool codesFound = FindCodes(renderData.texId,
            static_cast<unsigned int>(renderData.width),
            static_cast<unsigned int>(renderData.height));

        if (!codesFound) {
            // Clean frame: copy current texture to the backup so we have a
            // known-good frame to show when a QR code appears later.
            copyToBackupTexture(currentRenderTexId, renderData.width, renderData.height);
            // Display the current render texture (already set by VideoLayer)
        } else {
            // QR code detected in this frame (control frame).
            // Switch renderData.texId to the backup texture so the control
            // frame is not displayed. The rendered texture stays untouched
            // in its FBO for the next frame's ping-pong cycle.
            if (m_backupTexId > 0
                && m_backupTexWidth == renderData.width
                && m_backupTexHeight == renderData.height) {
                renderData.texId = m_backupTexId;
            }
            // If no backup exists yet we have no choice but to show the
            // current frame (first frame ever was a QR code).
        }

        // Update the active sublayer with the displayed frame
        if (m_qrOpHandler && m_qrOpHandler->isActive()) {
            m_qrOpHandler->updateActiveSubLayer(this, renderData.texId, renderData.width, renderData.height);
        }
    }

    // Update divide texture sublayers if division mode is active
    if (m_textureDivisionMode == 2 && m_divideTexHandler && m_divideTexHandler->isActive()
        && renderData.texId > 0 && renderData.width > 0 && renderData.height > 0) {
        m_divideTexHandler->updateSubLayers(this, renderData.texId, renderData.width, renderData.height);
    }
}

bool StreamLayer::ready() const {
    return !m_data.loadedFile.empty();
}

static void streamGridIndexToColsRows(int grid, int& cols, int& rows) {
    switch (grid) {
    case 1: cols = 1; rows = 2; break;
    case 2: cols = 2; rows = 1; break;
    case 3: cols = 2; rows = 2; break;
    case 4: cols = 2; rows = 3; break;
    case 5: cols = 3; rows = 2; break;
    case 6: cols = 3; rows = 3; break;
    default: cols = 1; rows = 1; break;
    }
}

void StreamLayer::encodeTypeProperties(std::vector<std::byte>& data) {
    MpvLayer::encodeTypeProperties(data);
    sgct::serializeObject(data, isQRCodeDetectionEnabled());
    sgct::serializeObject(data, m_textureDivisionMode);
    sgct::serializeObject(data, m_textureDivisionGrid);

    // Serialize sublayer properties for division mode
    if (m_textureDivisionMode == 2 && m_divideTexHandler) {
        int numSubs = m_divideTexHandler->cellCount();
        sgct::serializeObject(data, numSubs);
        // Sublayers may not exist yet (lazy creation), but we still encode
        // the ones that do exist with their user-modified properties.
        bool hasSubs = m_divideTexHandler->hasSubLayers();
        sgct::serializeObject(data, hasSubs);
        if (hasSubs) {
            auto& subs = m_divideTexHandler->subLayers();
            int actualCount = static_cast<int>(subs.size());
            sgct::serializeObject(data, actualCount);
            for (int i = 0; i < actualCount; ++i) {
                auto& sub = subs[i];
                if (sub) {
                    sub->encodeBaseAlways(data);
                    sub->encodeBaseProperties(data);
                }
            }
        }
    }
}

void StreamLayer::decodeTypeProperties(const std::vector<std::byte>& data, unsigned int& pos) {
    MpvLayer::decodeTypeProperties(data, pos);
    sgct::deserializeObject(data, pos, m_qrCodeDetectionEnabled_Dec);

    int divMode = 0;
    sgct::deserializeObject(data, pos, divMode);
    int divGrid = 0;
    sgct::deserializeObject(data, pos, divGrid);

    m_textureDivisionMode = divMode;
    m_textureDivisionGrid = divGrid;

    // Deserialize sublayer properties for division mode
    if (m_textureDivisionMode == 2) {
        int numSubs = 0;
        sgct::deserializeObject(data, pos, numSubs);
        bool hasSubs = false;
        sgct::deserializeObject(data, pos, hasSubs);
        if (hasSubs) {
            int actualCount = 0;
            sgct::deserializeObject(data, pos, actualCount);

            // Ensure DivideTextureHandler exists and has proper division set
            if (!m_divideTexHandler) {
                m_divideTexHandler = new DivideTextureHandler();
            }
            int cols = 1, rows = 1;
            streamGridIndexToColsRows(m_textureDivisionGrid, cols, rows);
            m_divideTexHandler->setDivision(cols, rows, this);

            // If sublayers exist, decode into them; otherwise just advance pos
            if (m_divideTexHandler->hasSubLayers()) {
                auto& subs = m_divideTexHandler->subLayers();
                for (int i = 0; i < actualCount; ++i) {
                    if (i < static_cast<int>(subs.size()) && subs[i]) {
                        subs[i]->decodeBaseAlways(data, pos);
                        subs[i]->decodeBaseProperties(data, pos);
                    } else {
                        // Skip data for this sublayer
                        BaseLayer::RenderParams tmpRender;
                        BaseLayer::PlaneParams tmpPlane;
                        // decodeBaseAlways: shouldUpdate, shouldPreLoad, alpha
                        bool tmpBool; float tmpFloat;
                        sgct::deserializeObject(data, pos, tmpBool);
                        sgct::deserializeObject(data, pos, tmpBool);
                        sgct::deserializeObject(data, pos, tmpFloat);
                        // decodeBaseProperties: gridMode, stereoMode, roiEnabled, [roi], [plane or rotate]
                        uint8_t gm, sm;
                        sgct::deserializeObject(data, pos, gm);
                        sgct::deserializeObject(data, pos, sm);
                        bool roiEn;
                        sgct::deserializeObject(data, pos, roiEn);
                        if (roiEn) {
                            float rx, ry, rz, rw;
                            sgct::deserializeObject(data, pos, rx);
                            sgct::deserializeObject(data, pos, ry);
                            sgct::deserializeObject(data, pos, rz);
                            sgct::deserializeObject(data, pos, rw);
                        }
                        if (gm == BaseLayer::GridMode::Plane) {
                            double d; float f; uint8_t u;
                            sgct::deserializeObject(data, pos, d); // azimuth
                            sgct::deserializeObject(data, pos, d); // elevation
                            sgct::deserializeObject(data, pos, d); // roll
                            sgct::deserializeObject(data, pos, d); // distance
                            sgct::deserializeObject(data, pos, d); // horizontal
                            sgct::deserializeObject(data, pos, d); // vertical
                            sgct::deserializeObject(data, pos, f); // specifiedSize.x
                            sgct::deserializeObject(data, pos, f); // specifiedSize.y
                            sgct::deserializeObject(data, pos, u); // aspectRatio
                        } else {
                            float rx, ry, rz;
                            sgct::deserializeObject(data, pos, rx);
                            sgct::deserializeObject(data, pos, ry);
                            sgct::deserializeObject(data, pos, rz);
                        }
                    }
                }
            } else {
                // Sublayers don't exist yet on this node, skip their data
                for (int i = 0; i < actualCount; ++i) {
                    bool tmpBool; float tmpFloat;
                    sgct::deserializeObject(data, pos, tmpBool);
                    sgct::deserializeObject(data, pos, tmpBool);
                    sgct::deserializeObject(data, pos, tmpFloat);
                    uint8_t gm, sm;
                    sgct::deserializeObject(data, pos, gm);
                    sgct::deserializeObject(data, pos, sm);
                    bool roiEn;
                    sgct::deserializeObject(data, pos, roiEn);
                    if (roiEn) {
                        float rx, ry, rz, rw;
                        sgct::deserializeObject(data, pos, rx);
                        sgct::deserializeObject(data, pos, ry);
                        sgct::deserializeObject(data, pos, rz);
                        sgct::deserializeObject(data, pos, rw);
                    }
                    if (gm == BaseLayer::GridMode::Plane) {
                        double d; float f; uint8_t u;
                        sgct::deserializeObject(data, pos, d);
                        sgct::deserializeObject(data, pos, d);
                        sgct::deserializeObject(data, pos, d);
                        sgct::deserializeObject(data, pos, d);
                        sgct::deserializeObject(data, pos, d);
                        sgct::deserializeObject(data, pos, d);
                        sgct::deserializeObject(data, pos, f);
                        sgct::deserializeObject(data, pos, f);
                        sgct::deserializeObject(data, pos, u);
                    } else {
                        float rx, ry, rz;
                        sgct::deserializeObject(data, pos, rx);
                        sgct::deserializeObject(data, pos, ry);
                        sgct::deserializeObject(data, pos, rz);
                    }
                }
            }
        }
    }

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

int StreamLayer::textureDivisionMode() const {
    return m_textureDivisionMode;
}

void StreamLayer::setTextureDivisionMode(int mode) {
    if (m_textureDivisionMode == mode)
        return;

    m_textureDivisionMode = mode;

    if (mode == 1) {
        // ImPres/QR mode
        setQRCodeDetectionEnabled(true);
        if (m_divideTexHandler) {
            m_divideTexHandler->clearAll();
        }
    } else if (mode == 2) {
        // Division mode
        setQRCodeDetectionEnabled(false);
        if (!m_divideTexHandler) {
            m_divideTexHandler = new DivideTextureHandler();
        }
        int cols = 1, rows = 1;
        streamGridIndexToColsRows(m_textureDivisionGrid, cols, rows);
        m_divideTexHandler->setDivision(cols, rows, this);
    } else {
        // None
        setQRCodeDetectionEnabled(false);
        if (m_divideTexHandler) {
            m_divideTexHandler->clearAll();
        }
    }

    if (isMaster())
        setNeedSync();
}

int StreamLayer::textureDivisionGrid() const {
    return m_textureDivisionGrid;
}

void StreamLayer::setTextureDivisionGrid(int grid) {
    if (m_textureDivisionGrid == grid)
        return;

    m_textureDivisionGrid = grid;

    if (m_textureDivisionMode == 2) {
        if (!m_divideTexHandler) {
            m_divideTexHandler = new DivideTextureHandler();
        }
        int cols = 1, rows = 1;
        streamGridIndexToColsRows(grid, cols, rows);
        m_divideTexHandler->setDivision(cols, rows, this);
    }

    if (isMaster())
        setNeedSync();
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

void StreamLayer::copyToBackupTexture(unsigned int srcTexId, int width, int height) {
    if (srcTexId == 0 || width <= 0 || height <= 0) {
        return;
    }

    // (Re-)create the backup texture if the dimensions changed
    if (m_backupTexId == 0 || m_backupTexWidth != width || m_backupTexHeight != height) {
        if (m_backupTexId > 0) {
            glDeleteTextures(1, &m_backupTexId);
        }

        glGenTextures(1, &m_backupTexId);
        glBindTexture(GL_TEXTURE_2D, m_backupTexId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        m_backupTexWidth = width;
        m_backupTexHeight = height;
    }

    // GPU-to-GPU copy of the clean frame into the backup texture
    glCopyImageSubData(
        srcTexId, GL_TEXTURE_2D, 0, 0, 0, 0,
        m_backupTexId, GL_TEXTURE_2D, 0, 0, 0, 0,
        width, height, 1);
}

void StreamLayer::onQRCommand(const QRCommand& command) {
    if (m_qrOpHandler) {
        m_qrOpHandler->handleCommand(command, this, renderData.texId, renderData.width, renderData.height);
    }
}

bool StreamLayer::hasSubLayers() const {
    if (m_textureDivisionMode == 2 && m_divideTexHandler)
        return m_divideTexHandler->hasSubLayers();
    return m_qrOpHandler ? m_qrOpHandler->hasSubLayers() : false;
}

std::vector<std::shared_ptr<BaseLayer>>& StreamLayer::getSubLayers() const {
    if (m_textureDivisionMode == 2 && m_divideTexHandler && m_divideTexHandler->hasSubLayers())
        return m_divideTexHandler->subLayers();
    if (m_qrOpHandler) {
        return m_qrOpHandler->subLayers();
    }
    static std::vector<std::shared_ptr<BaseLayer>> empty;
    return empty;
}
