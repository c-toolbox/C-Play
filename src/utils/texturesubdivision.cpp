/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "texturesubdivision.h"
#include <utils/dividetexturehandler.h>
#include <utils/qrcommandprocessor.h>
#include <utils/qroperationconfig.h>
#include <utils/qroperationhandler.h>
#include <layers/baselayer.h>
#include <layers/texturelayer.h>
#include <sgct/opengl.h>
#include <sgct/sgct.h>

TextureSubdivision::TextureSubdivision(BaseLayer* owner) : m_owner(owner) {
    m_qrProcessor = std::make_unique<QRCommandProcessor>();
    m_qrProcessor->setCommandCallback([this](const QRCommand& cmd) {
        onQRCommand(cmd);
    });
    m_qrOpHandler = std::make_unique<QROperationHandler>();
}

TextureSubdivision::~TextureSubdivision() {
    releaseGL();
}

bool TextureSubdivision::qrEnabled() const {
    // A decoded value is applied on the render thread; report it already so an
    // encode in between (e.g. a copied layer) does not sync the old one.
    if (m_qrDecodePending)
        return m_qrEnabledDecoded;
    return m_qrProcessor->isEnabled();
}

void TextureSubdivision::setQrEnabled(bool enabled) {
    m_qrDecodePending = false;
    m_qrProcessor->setEnabled(enabled);
    if (!enabled) {
        m_qrProcessor->clearQueue();
        m_qrOpHandler->clearAll();
    }
}

int TextureSubdivision::mode() const {
    return m_mode;
}

void TextureSubdivision::setMode(int mode) {
    if (m_mode == mode)
        return;

    m_mode = mode;

    if (mode == ImPres) {
        setQrEnabled(true);
        if (m_divideTexHandler)
            m_divideTexHandler->clearAll();
    } else if (mode == Division) {
        setQrEnabled(false);
        applyDivision();
    } else {
        setQrEnabled(false);
        if (m_divideTexHandler)
            m_divideTexHandler->clearAll();
    }
}

int TextureSubdivision::grid() const {
    return m_grid;
}

void TextureSubdivision::setGrid(int grid) {
    if (m_grid == grid)
        return;

    m_grid = grid;

    if (m_mode == Division)
        applyDivision();
}

void TextureSubdivision::gridToColsRows(int grid, int& cols, int& rows) {
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

int TextureSubdivision::mergeColumns() const {
    return m_mergeCols;
}

int TextureSubdivision::mergeRows() const {
    return m_mergeRows;
}

bool TextureSubdivision::setMerge(int cols, int rows) {
    int gridCols = 1, gridRows = 1;
    gridToColsRows(m_grid, gridCols, gridRows);
    const bool custom = cols == 0 && rows == 0;
    if (!custom && !isValidMerge(cols, rows, gridCols * gridRows))
        return false;

    m_mergeCols = cols;
    m_mergeRows = rows;
    applyMerge();
    return true;
}

bool TextureSubdivision::isValidMerge(int cols, int rows, int cellCount) {
    if (cols < 1 || rows < 1 || cellCount < 1 || cols > cellCount)
        return false;
    // Filled row by row: the rows needed for cellCount cells, at most one of them partial.
    return rows == (cellCount + cols - 1) / cols;
}

std::vector<std::pair<int, int>> TextureSubdivision::validMerges(int cellCount) {
    std::vector<std::pair<int, int>> merges;
    for (int cols = 1; cols <= cellCount; ++cols)
        merges.emplace_back(cols, (cellCount + cols - 1) / cols);
    return merges;
}

void TextureSubdivision::applyMerge() {
    if (m_mode != Division || !m_divideTexHandler || m_mergeCols < 1 || m_mergeRows < 1)
        return;
    // Only the master turns a moved cell into a move of the layout; the nodes follow the
    // synced parent (and cells).
    m_divideTexHandler->applyMerge(m_owner, m_mergeCols, m_mergeRows, m_owner->isMaster());
}

void TextureSubdivision::applyDivision() {
    if (!m_divideTexHandler)
        m_divideTexHandler = std::make_unique<DivideTextureHandler>();
    int cols = 1, rows = 1;
    gridToColsRows(m_grid, cols, rows);
    m_divideTexHandler->setDivision(cols, rows, m_owner);

    const bool custom = m_mergeCols == 0 && m_mergeRows == 0;
    if (!custom && !isValidMerge(m_mergeCols, m_mergeRows, cols * rows)) {
        m_mergeCols = cols;
        m_mergeRows = rows;
    }
    applyMerge();
}

bool TextureSubdivision::loadQROperationConfig(const std::string& filePath) {
    return m_qrOpHandler->loadConfig(filePath);
}

const QROperationConfig* TextureSubdivision::qrOperationConfig() const {
    return m_qrOpHandler->config();
}

std::string TextureSubdivision::activePlaneName() const {
    return m_qrOpHandler->activePlaneName();
}

bool TextureSubdivision::hasSubLayers() const {
    if (m_mode == Division)
        return m_divideTexHandler && m_divideTexHandler->hasSubLayers();
    return qrEnabled() && m_qrOpHandler->hasSubLayers();
}

std::vector<std::shared_ptr<BaseLayer>>& TextureSubdivision::subLayers() const {
    if (m_mode == Division && m_divideTexHandler)
        return m_divideTexHandler->subLayers();
    return m_qrOpHandler->subLayers();
}

bool TextureSubdivision::isControlFrame(unsigned char* pixels, unsigned int width, unsigned int height,
                                        int glFormat, int rowStride, int pixelStride) {
    applyDecoded();
    if (!m_qrProcessor->isEnabled() || !pixels || width == 0 || height == 0)
        return false;
    return m_qrProcessor->processFrame(pixels, width, height, glFormat, rowStride, pixelStride);
}

unsigned int TextureSubdivision::filterControlFrame(unsigned int texId, int width, int height) {
    applyDecoded();
    if (!m_qrProcessor->isEnabled() || texId == 0 || width <= 0 || height <= 0)
        return texId;
    // No new frame since the last call returned the clean copy.
    if (texId == m_backupTex)
        return texId;

    m_readback.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    glBindTexture(GL_TEXTURE_2D, texId);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_readback.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    // Queued commands run once the first clean frame arrives, against that frame.
    m_liveTex = texId;
    m_liveWidth = width;
    m_liveHeight = height;
    const bool controlFrame = m_qrProcessor->processFrame(m_readback.data(),
        static_cast<unsigned int>(width), static_cast<unsigned int>(height), GL_RGBA);

    if (!controlFrame) {
        copyToBackup(texId, width, height);
        return texId;
    }
    // Show the last clean frame instead; without one (the first frame ever was a
    // control frame) there is nothing else to show.
    if (m_backupTex > 0 && m_backupWidth == width && m_backupHeight == height)
        return m_backupTex;
    return texId;
}

void TextureSubdivision::copyToBackup(unsigned int texId, int width, int height) {
    GLint format = 0;
    glBindTexture(GL_TEXTURE_2D, texId);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &format);
    glBindTexture(GL_TEXTURE_2D, 0);
    // glTexStorage2D needs a sized format; glCopyImageSubData a matching one.
    if (format == GL_RGBA)
        format = GL_RGBA8;
    else if (format == GL_RGB)
        format = GL_RGB8;

    if (m_backupTex == 0 || m_backupWidth != width || m_backupHeight != height || m_backupFormat != format) {
        if (m_backupTex > 0)
            glDeleteTextures(1, &m_backupTex);

        glGenTextures(1, &m_backupTex);
        glBindTexture(GL_TEXTURE_2D, m_backupTex);
        glTexStorage2D(GL_TEXTURE_2D, 1, static_cast<GLenum>(format), width, height);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        m_backupWidth = width;
        m_backupHeight = height;
        m_backupFormat = format;
    }

    glCopyImageSubData(
        texId, GL_TEXTURE_2D, 0, 0, 0, 0,
        m_backupTex, GL_TEXTURE_2D, 0, 0, 0, 0,
        width, height, 1);
}

void TextureSubdivision::updateSubLayers(unsigned int texId, int width, int height) {
    applyDecoded();
    if (texId == 0 || width <= 0 || height <= 0)
        return;

    m_liveTex = texId;
    m_liveWidth = width;
    m_liveHeight = height;

    if (m_qrProcessor->isEnabled() && m_qrOpHandler->isActive())
        m_qrOpHandler->updateActiveSubLayer(m_owner, texId, width, height);

    if (m_mode == Division && m_divideTexHandler && m_divideTexHandler->isActive()) {
        m_divideTexHandler->updateSubLayers(m_owner, texId, width, height);
        // The cell size follows the texture, and the parent may have moved meanwhile.
        applyMerge();
    }
}

void TextureSubdivision::releaseGL() {
    if (m_backupTex > 0) {
        glDeleteTextures(1, &m_backupTex);
        m_backupTex = 0;
    }
    m_backupWidth = 0;
    m_backupHeight = 0;
    m_backupFormat = 0;
    m_readback.clear();
    m_readback.shrink_to_fit();
}

void TextureSubdivision::applyDecoded() {
    if (m_qrDecodePending)
        setQrEnabled(m_qrEnabledDecoded);
}

void TextureSubdivision::onQRCommand(const QRCommand& command) {
    m_qrOpHandler->handleCommand(command, m_owner, m_liveTex, m_liveWidth, m_liveHeight);
}

void TextureSubdivision::encodeAlways(std::vector<std::byte>& data) const {
    int count = 0;
    if (m_mode == Division && m_divideTexHandler && m_divideTexHandler->hasSubLayers())
        count = static_cast<int>(m_divideTexHandler->subLayers().size());
    sgct::serializeObject(data, count);
    for (int i = 0; i < count; ++i) {
        auto& sub = m_divideTexHandler->subLayers()[i];
        sub->encodeBaseAlways(data);
        sub->encodeTypeAlways(data);
    }
}

void TextureSubdivision::decodeAlways(const std::vector<std::byte>& data, unsigned int& pos) {
    int count = 0;
    sgct::deserializeObject(data, pos, count);
    if (count <= 0)
        return;

    // Cells this machine does not have (yet) are decoded into a scratch layer, which keeps
    // the read position in step with whatever the sublayers encode.
    std::unique_ptr<TextureLayer> scratch;
    for (int i = 0; i < count; ++i) {
        BaseLayer* target = nullptr;
        if (m_mode == Division && m_divideTexHandler) {
            auto& subs = m_divideTexHandler->subLayers();
            if (i < static_cast<int>(subs.size()))
                target = subs[i].get();
        }
        if (!target) {
            if (!scratch)
                scratch = std::make_unique<TextureLayer>();
            target = scratch.get();
        }
        target->decodeBaseAlways(data, pos);
        target->decodeTypeAlways(data, pos);
    }
}

void TextureSubdivision::encodeProperties(std::vector<std::byte>& data) const {
    sgct::serializeObject(data, qrEnabled());
    sgct::serializeObject(data, m_mode);
    sgct::serializeObject(data, m_grid);
    sgct::serializeObject(data, m_mergeCols);
    sgct::serializeObject(data, m_mergeRows);

    int count = 0;
    if (m_mode == Division && m_divideTexHandler && m_divideTexHandler->hasSubLayers())
        count = static_cast<int>(m_divideTexHandler->subLayers().size());
    sgct::serializeObject(data, count);
    for (int i = 0; i < count; ++i) {
        auto& sub = m_divideTexHandler->subLayers()[i];
        sub->encodeBaseProperties(data);
        sub->encodeTypeProperties(data);
    }
}

void TextureSubdivision::decodeProperties(const std::vector<std::byte>& data, unsigned int& pos) {
    sgct::deserializeObject(data, pos, m_qrEnabledDecoded);
    m_qrDecodePending = true;
    sgct::deserializeObject(data, pos, m_mode);
    sgct::deserializeObject(data, pos, m_grid);
    sgct::deserializeObject(data, pos, m_mergeCols);
    sgct::deserializeObject(data, pos, m_mergeRows);

    // Create the cells right away, also when the master has none to send, so the
    // division shows on this machine as soon as frames arrive.
    if (m_mode == Division)
        applyDivision();

    int count = 0;
    sgct::deserializeObject(data, pos, count);
    std::unique_ptr<TextureLayer> scratch;
    for (int i = 0; i < count; ++i) {
        BaseLayer* target = nullptr;
        if (m_mode == Division && m_divideTexHandler) {
            auto& subs = m_divideTexHandler->subLayers();
            if (i < static_cast<int>(subs.size()))
                target = subs[i].get();
        }
        if (!target) {
            if (!scratch)
                scratch = std::make_unique<TextureLayer>();
            target = scratch.get();
        }
        target->decodeBaseProperties(data, pos);
        target->decodeTypeProperties(data, pos);
    }
}

void TextureSubdivision::encodeDefaults(std::vector<std::byte>& data, bool properties) {
    const int count = 0;
    if (!properties) {
        sgct::serializeObject(data, count);
        return;
    }
    const bool qr = false;
    const int mode = None;
    const int grid = 0;
    const int mergeCols = 1;
    const int mergeRows = 1;
    sgct::serializeObject(data, qr);
    sgct::serializeObject(data, mode);
    sgct::serializeObject(data, grid);
    sgct::serializeObject(data, mergeCols);
    sgct::serializeObject(data, mergeRows);
    sgct::serializeObject(data, count);
}
