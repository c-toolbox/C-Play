/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nodestreamlayer.h"
#include "nodestreamsocket.h"

#include <sgct/opengl.h>
#include <sgct/sgct.h>
#include <algorithm>
#include <chrono>
#include <format>

namespace {

// Not part of the core profile headers.
constexpr GLenum kCompressedRgbS3tcDxt1 = 0x83F0;
constexpr GLenum kCompressedRgbaS3tcDxt5 = 0x83F3;

constexpr int64_t kRetryStartNs = 2'000'000'000;

GLenum glFormat(nodestream::Format format) {
    switch (format) {
    case nodestream::Format::BC1:
        return kCompressedRgbS3tcDxt1;
    case nodestream::Format::BC3:
        return kCompressedRgbaS3tcDxt5;
    default:
        return GL_COMPRESSED_RGBA_BPTC_UNORM;
    }
}

int64_t nowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::string nodeInterfaceAddress() {
    static const std::string address = [] {
        const std::string resolved = NodeStreamSocket::resolveIPv4(sgct::Engine::instance().thisNode().address());
        return NodeStreamSocket::isLoopbackAddress(resolved) ? std::string() : resolved;
    }();
    return address;
}

} // namespace

NodeStreamLayer::NodeStreamLayer() {
    setType(BaseLayer::LayerType::NODESTREAM);
}

NodeStreamLayer::~NodeStreamLayer() {
    cleanup();
}

void NodeStreamLayer::cleanup() {
    if (m_receiver) {
        m_receiver->cleanupGL();
        m_receiver.reset();
    }
    if (m_texture) {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
    }
    renderData.texId = 0;
    renderData.width = 0;
    renderData.height = 0;
    m_texWidth = 0;
    m_texHeight = 0;
    m_hasFrame = false;
    m_hasLastUploaded = false;
    m_activeGroup.clear();
    m_activePort = 0;
}

void NodeStreamLayer::initialize() {
    m_hasInitialized = true;
}

void NodeStreamLayer::ensureReceiver() {
    if (m_group.empty() || m_port <= 0 || m_port > 65535)
        return;

    const bool changed = m_group != m_activeGroup || m_port != m_activePort || m_streamId != m_activeStreamId;
    if (!m_receiver)
        m_receiver = std::make_unique<NodeStreamReceiver>();
    if (!changed && m_receiver->isRunning())
        return;

    const int64_t now = nowNs();
    if (!changed && now - m_lastStartAttemptNs < kRetryStartNs)
        return;
    m_lastStartAttemptNs = now;

    m_activeGroup = m_group;
    m_activePort = m_port;
    m_activeStreamId = m_streamId;
    m_receiver->start(m_group, static_cast<uint16_t>(m_port), nodeInterfaceAddress(), m_streamId);
}

void NodeStreamLayer::update(bool) {
    ensureReceiver();
    if (!m_receiver || !m_receiver->isRunning())
        return;

    m_receiver->serviceGL();

    std::vector<NodeStreamReceiver::Frame> frames;
    const bool hasLast = m_hasLastUploaded && m_lastUploadedSession == m_sessionId;
    if (m_syncMode == static_cast<uint8_t>(nodestream::SyncMode::FrameLocked)) {
        if (m_masterSending)
            frames = m_receiver->acquireFrameLocked(m_sessionId, m_targetFrameId, m_maxWaitMs, hasLast, m_lastUploadedFrameId);
    } else {
        frames = m_receiver->acquireNewest(hasLast, m_lastUploadedFrameId);
    }

    for (const NodeStreamReceiver::Frame &frame : frames) {
        upload(frame);
        m_receiver->finishUpload(frame);
        m_hasLastUploaded = true;
        m_lastUploadedSession = frame.sessionId;
        m_lastUploadedFrameId = frame.frameId;
    }
}

bool NodeStreamLayer::upload(const NodeStreamReceiver::Frame &frame) {
    const GLenum internalFormat = glFormat(frame.format);
    const bool resized = frame.width != m_texWidth || frame.height != m_texHeight;
    if (!m_texture || resized || frame.format != m_texFormat) {
        // A partial frame on a fresh texture would show uninitialized blocks.
        if (!frame.complete)
            return false;
        if (m_texture)
            glDeleteTextures(1, &m_texture);
        glGenTextures(1, &m_texture);
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glTexStorage2D(GL_TEXTURE_2D, 1, internalFormat, frame.width, frame.height);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        m_texFormat = frame.format;
        m_texWidth = frame.width;
        m_texHeight = frame.height;
    } else {
        glBindTexture(GL_TEXTURE_2D, m_texture);
    }

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, frame.pbo);
    if (frame.complete) {
        glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame.width, frame.height, internalFormat,
                                  static_cast<GLsizei>(nodestream::frameBytes(frame.format, frame.width, frame.height)), nullptr);
    } else {
        const uint32_t blockSize = nodestream::blockBytes(frame.format);
        const uint32_t rowBytes = nodestream::blocksAcross(frame.width) * blockSize;
        auto uploadRect = [&](uint32_t offset, uint32_t length, int x, int y, int w, int h) {
            w = std::min(w, frame.width - x);
            h = std::min(h, frame.height - y);
            if (w > 0 && h > 0)
                glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, internalFormat, static_cast<GLsizei>(length),
                                          reinterpret_cast<const void *>(static_cast<uintptr_t>(offset)));
        };
        // Each merged range is split into a leading row segment, whole rows and a trailing row segment.
        for (const auto &[rangeOffset, rangeLength] : frame.ranges) {
            uint32_t offset = rangeOffset;
            uint32_t end = rangeOffset + rangeLength;
            if (offset % rowBytes != 0) {
                const uint32_t rowEnd = std::min(end, (offset / rowBytes + 1) * rowBytes);
                const uint32_t blocks = (rowEnd - offset) / blockSize;
                uploadRect(offset, rowEnd - offset, static_cast<int>((offset % rowBytes) / blockSize) * 4,
                           static_cast<int>(offset / rowBytes) * 4, static_cast<int>(blocks) * 4, 4);
                offset = rowEnd;
            }
            const uint32_t wholeRows = (end - offset) / rowBytes;
            if (wholeRows > 0) {
                uploadRect(offset, wholeRows * rowBytes, 0, static_cast<int>(offset / rowBytes) * 4, frame.width,
                           static_cast<int>(wholeRows) * 4);
                offset += wholeRows * rowBytes;
            }
            if (offset < end) {
                uploadRect(offset, end - offset, 0, static_cast<int>(offset / rowBytes) * 4,
                           static_cast<int>((end - offset) / blockSize) * 4, 4);
            }
        }
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    renderData.texId = m_texture;
    renderData.width = m_texWidth;
    renderData.height = m_texHeight;
    if (!m_hasFrame || resized) {
        m_hasFrame = true;
        if (gridMode() == BaseLayer::GridMode::Plane)
            updatePlane();
    }
    return true;
}

bool NodeStreamLayer::ready() const {
    return m_texture != 0 && m_hasFrame;
}

bool NodeStreamLayer::hasTexture() const {
    return ready();
}

void NodeStreamLayer::collectLoadStatus() {
    const std::string error = m_receiver ? m_receiver->lastError() : std::string();
    if (error.empty())
        clearLoadError();
    else
        setLoadError(std::format("{}:{}", m_group, m_port), error);
}

void NodeStreamLayer::decodeTypeCore(const std::vector<std::byte> &data, unsigned int &pos) {
    sgct::deserializeObject(data, pos, m_group);
    sgct::deserializeObject(data, pos, m_port);
    sgct::deserializeObject(data, pos, m_syncMode);
    sgct::deserializeObject(data, pos, m_maxWaitMs);
    sgct::deserializeObject(data, pos, m_streamId);
}

void NodeStreamLayer::decodeTypeAlways(const std::vector<std::byte> &data, unsigned int &pos) {
    sgct::deserializeObject(data, pos, m_sessionId);
    sgct::deserializeObject(data, pos, m_targetFrameId);
    sgct::deserializeObject(data, pos, m_masterSending);
}

void NodeStreamLayer::decodeTypeProperties(const std::vector<std::byte> &, unsigned int &) {
}
