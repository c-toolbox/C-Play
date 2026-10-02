/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nodestreamsender.h"
#include "layers/baselayer.h"

#include <sgct/log.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <format>
#include <random>

namespace {

// Small, so that a full buffer cannot hide pacing that is faster than the link.
constexpr int kSendBufferBytes = 1024 * 1024;
// Pacing granularity: the shared send budget is reserved in chunks of this size.
constexpr size_t kPacingChunkBytes = 64 * 1024;
constexpr int kAutoAlphaHoldFrames = 60;
constexpr int64_t kStatsIntervalNs = 5'000'000'000;
// An unchanged frame is still resent this often, for nodes that joined late or lost packets.
constexpr int64_t kDuplicateRefreshNs = 500'000'000;
constexpr int kAutoRatePercent = 85;
constexpr int kFallbackLinkMbps = 1000;

int64_t nowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

uint32_t newSessionId() {
    std::random_device rd;
    uint32_t id = rd() ^ static_cast<uint32_t>(nowNs());
    return id == 0 ? 1 : id;
}

uint64_t hashFrame(const uint8_t *data, size_t size, uint64_t seed) {
    uint64_t h = seed ^ (size * 0x9E3779B97F4A7C15ULL);
    size_t i = 0;
    for (; i + 8 <= size; i += 8) {
        uint64_t word;
        std::memcpy(&word, data + i, 8);
        h = (h ^ word) * 0xFF51AFD7ED558CCDULL;
        h ^= h >> 32;
    }
    for (; i < size; ++i)
        h = (h ^ data[i]) * 0x100000001B3ULL;
    return h ^ (h >> 29);
}

// Shared by all senders so that the combined rate stays below the link rate.
class SendPacer {
public:
    static SendPacer &instance() {
        static SendPacer pacer;
        return pacer;
    }

    // Waits for the start of the reserved send window and returns its end.
    int64_t reserve(size_t bytes, int rateMbps) {
        const int64_t duration = static_cast<int64_t>(bytes) * 8000 / std::max(rateMbps, 1);
        const int64_t now = nowNs();
        int64_t start;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            start = std::max(now, m_nextFreeNs);
            m_nextFreeNs = start + duration;
        }
        NodeStreamSocket::waitUntilNs(start);
        return start + duration;
    }

private:
    std::mutex m_mutex;
    int64_t m_nextFreeNs = 0;
};

} // namespace

NodeStreamSender::NodeStreamSender() = default;

NodeStreamSender::~NodeStreamSender() {
    stopThread();
}

NodeStreamSource NodeStreamSender::sourceFromLayer(BaseLayer *layer) {
    NodeStreamSource source;
    source.textureId = [layer]() -> unsigned int { return layer->hasTexture() ? layer->textureId() : 0; };
    source.width = [layer]() { return layer->width(); };
    source.height = [layer]() { return layer->height(); };
    return source;
}

void NodeStreamSender::setSource(const NodeStreamSource &source) {
    m_source = source;
}

bool NodeStreamSender::isSending() const {
    return m_sending;
}

uint32_t NodeStreamSender::sessionId() const {
    return m_sessionId;
}

uint32_t NodeStreamSender::targetFrameId(int64_t guardNs) {
    const int64_t now = nowNs();
    std::lock_guard<std::mutex> lock(m_sentMutex);
    for (int i = 1; i <= m_sentCount; ++i) {
        const SentFrame &sent = m_sent[(m_sentNext - i + kSentHistory) % kSentHistory];
        if (sent.wireDoneNs + guardNs > now)
            continue;
        if (!m_hasTarget || static_cast<int32_t>(sent.frameId - m_target) > 0) {
            m_hasTarget = true;
            m_target = sent.frameId;
        }
        break;
    }
    return m_target;
}

nodestream::Format NodeStreamSender::activeFormat() const {
    return static_cast<nodestream::Format>(m_activeFormat.load());
}

void NodeStreamSender::stopThread() {
    if (m_thread.joinable()) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_quit = true;
        }
        m_cv.notify_all();
        m_thread.join();
    }
    m_quit = false;
    m_sending = false;
}

bool NodeStreamSender::restart(const NodeStreamConfig &config) {
    stopThread();
    m_socket.close();
    for (Slot &slot : m_slots) {
        if (slot.fence) {
            glDeleteSync(slot.fence);
            slot.fence = nullptr;
        }
        slot.state = Free;
    }

    m_config = config;
    m_config.maxDatagram = std::clamp(m_config.maxDatagram, static_cast<int>(sizeof(nodestream::PacketHeader)) + 16,
                                      static_cast<int>(nodestream::kMaxDatagram));
    m_configured = true;
    m_sessionId = newSessionId();
    m_nextFrameId = 0;
    m_nextSequence = 0;
    m_lastCaptureNs = 0;
    m_hasLastHash = false;
    m_lastSendNs = 0;
    {
        std::lock_guard<std::mutex> lock(m_sentMutex);
        m_sentCount = 0;
        m_sentNext = 0;
        m_hasTarget = false;
        m_target = 0;
    }

    const uint64_t linkMbps = NodeStreamSocket::linkSpeedMbps(m_config.interfaceAddress);
    if (m_config.rateMbps > 0) {
        m_rateMbps = m_config.rateMbps;
    } else {
        m_rateMbps = static_cast<int>((linkMbps > 0 ? linkMbps : kFallbackLinkMbps) * kAutoRatePercent / 100);
    }

    m_socketOk = m_socket.openSender(m_config.group, m_config.port, m_config.interfaceAddress, m_config.ttl,
                                     m_config.loopback, kSendBufferBytes);
    if (!m_socketOk) {
        sgct::Log::Error(std::format("NodeStreamSender: could not open {}:{}: {}", m_config.group, m_config.port,
                                     m_socket.lastError()));
        return false;
    }

    sgct::Log::Info(std::format("NodeStreamSender: streaming to {}:{} (session {:08x}), pacing {} Mbps{}, link {}, send buffer {}",
                                m_config.group, m_config.port, m_sessionId.load(), m_rateMbps,
                                m_config.rateMbps > 0 ? "" : " (auto)",
                                linkMbps > 0 ? std::format("{} Mbps", linkMbps) : std::string("unknown"),
                                m_socket.sendBufferSize()));

    if (!m_waitContextTried) {
        m_waitContextTried = true;
        if (!m_waitContext.create())
            sgct::Log::Warning("NodeStreamSender: no shared OpenGL context, encoded frames wait for the next capture");
    }
    m_workerWaits = m_waitContext.isValid();

    m_statStartNs = nowNs();
    m_thread = std::thread(&NodeStreamSender::threadMain, this);
    return true;
}

void NodeStreamSender::releaseSlot(Slot &slot) {
    if (slot.fence) {
        glDeleteSync(slot.fence);
        slot.fence = nullptr;
    }
    if (slot.buffer) {
        if (slot.mapped) {
            glBindBuffer(GL_COPY_WRITE_BUFFER, slot.buffer);
            glUnmapBuffer(GL_COPY_WRITE_BUFFER);
            glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
        }
        glDeleteBuffers(1, &slot.buffer);
    }
    slot.buffer = 0;
    slot.mapped = nullptr;
    slot.capacity = 0;
    slot.state = Free;
}

bool NodeStreamSender::ensureSlotCapacity(Slot &slot, size_t size) {
    if (slot.buffer && slot.capacity >= size)
        return true;
    releaseSlot(slot);

    const GLbitfield flags = GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
    glGenBuffers(1, &slot.buffer);
    glBindBuffer(GL_COPY_WRITE_BUFFER, slot.buffer);
    glBufferStorage(GL_COPY_WRITE_BUFFER, static_cast<GLsizeiptr>(size), nullptr, flags | GL_CLIENT_STORAGE_BIT);
    slot.mapped = static_cast<uint8_t *>(glMapBufferRange(GL_COPY_WRITE_BUFFER, 0, static_cast<GLsizeiptr>(size), flags));
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    if (!slot.mapped) {
        sgct::Log::Error("NodeStreamSender: could not map staging buffer");
        releaseSlot(slot);
        return false;
    }
    slot.capacity = size;
    return true;
}

void NodeStreamSender::markEncoded(Slot &slot) {
    uint32_t alphaFlag = 0;
    std::memcpy(&alphaFlag, slot.mapped + NodeStreamEncoder::alphaFlagOffset(slot.format, slot.width, slot.height),
                sizeof(alphaFlag));
    constexpr uint8_t bc1 = static_cast<uint8_t>(nodestream::Format::BC1);
    constexpr uint8_t bc3 = static_cast<uint8_t>(nodestream::Format::BC3);
    if (alphaFlag) {
        m_autoFormat = bc3;
        m_framesWithoutAlpha = 0;
    } else if (m_autoFormat == bc3 && ++m_framesWithoutAlpha >= kAutoAlphaHoldFrames) {
        m_autoFormat = bc1;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    slot.state = Ready;
}

void NodeStreamSender::pollEncodedSlots() {
    bool queued = false;
    for (Slot &slot : m_slots) {
        if (slot.state != Encoding || !slot.fence)
            continue;
        const GLenum result = glClientWaitSync(slot.fence, 0, 0);
        if (result != GL_ALREADY_SIGNALED && result != GL_CONDITION_SATISFIED)
            continue;
        glDeleteSync(slot.fence);
        slot.fence = nullptr;
        markEncoded(slot);
        queued = true;
    }
    if (queued)
        m_cv.notify_one();
}

void NodeStreamSender::waitEncoded(Slot &slot) {
    constexpr GLuint64 kWaitTimeoutNs = 100'000'000;
    GLenum result = GL_TIMEOUT_EXPIRED;
    while (result == GL_TIMEOUT_EXPIRED && !m_quit)
        result = glClientWaitSync(slot.fence, 0, kWaitTimeoutNs);
    if (result != GL_ALREADY_SIGNALED && result != GL_CONDITION_SATISFIED) {
        if (result == GL_WAIT_FAILED) {
            glDeleteSync(slot.fence);
            slot.fence = nullptr;
            std::lock_guard<std::mutex> lock(m_mutex);
            slot.state = Free;
        }
        return;
    }
    glDeleteSync(slot.fence);
    slot.fence = nullptr;
    markEncoded(slot);
}

bool NodeStreamSender::captureAndSend(const NodeStreamConfig &config) {
    if (!m_source.valid())
        return false;

    if (!m_configured || !(config == m_config)) {
        if (!restart(config))
            return false;
    }
    if (!m_socketOk)
        return false;

    if (!m_workerWaits)
        pollEncodedSlots();
    m_gpuTimeMs = static_cast<float>(m_encoder.lastGpuTimeMs());

    const GLuint texture = m_source.textureId();
    const int width = m_source.width();
    const int height = m_source.height();
    if (texture == 0 || width <= 0 || height <= 0 || width > nodestream::kMaxDimension || height > nodestream::kMaxDimension)
        return false;

    const int64_t now = nowNs();
    if (m_config.maxFps > 0 && m_lastCaptureNs != 0 && now - m_lastCaptureNs < 1'000'000'000LL / m_config.maxFps)
        return true;

    Slot *slot = nullptr;
    for (Slot &s : m_slots) {
        if (s.state == Free) {
            slot = &s;
            break;
        }
    }
    if (!slot) {
        ++m_framesDropped;
        return false;
    }

    const nodestream::Format format = m_config.format == nodestream::Format::Auto
                                          ? static_cast<nodestream::Format>(m_autoFormat.load())
                                          : m_config.format;
    if (!ensureSlotCapacity(*slot, NodeStreamEncoder::outputSize(format, width, height)))
        return false;
    if (!m_encoder.encode(texture, width, height, format, slot->buffer))
        return false;

    slot->fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    glFlush();
    slot->sequence = ++m_nextSequence;
    slot->format = format;
    slot->width = width;
    slot->height = height;
    slot->size = nodestream::frameBytes(format, width, height);
    slot->timestampNs = static_cast<uint64_t>(now);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        slot->state = Encoding;
    }
    if (m_workerWaits)
        m_cv.notify_one();
    m_lastCaptureNs = now;
    m_activeFormat = static_cast<uint8_t>(format);
    return true;
}

void NodeStreamSender::threadMain() {
    if (m_workerWaits && !m_waitContext.makeCurrent()) {
        sgct::Log::Warning("NodeStreamSender: could not make the shared OpenGL context current");
        m_workerWaits = false;
    }

    while (true) {
        Slot *slot = nullptr;
        Slot *encoding = nullptr;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this] {
                if (m_quit)
                    return true;
                for (const Slot &s : m_slots) {
                    if (s.state == Ready || (m_workerWaits && s.state == Encoding && s.fence))
                        return true;
                }
                return false;
            });
            if (m_quit)
                break;

            if (m_workerWaits) {
                for (Slot &s : m_slots) {
                    if (s.state == Encoding && s.fence
                        && (!encoding || static_cast<int32_t>(encoding->sequence - s.sequence) > 0))
                        encoding = &s;
                }
            }
        }
        if (encoding) {
            waitEncoded(*encoding);
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            // Send the newest ready frame only; older ones are stale.
            for (Slot &s : m_slots) {
                if (s.state != Ready)
                    continue;
                if (!slot || static_cast<int32_t>(s.sequence - slot->sequence) > 0) {
                    if (slot) {
                        slot->state = Free;
                        ++m_framesDropped;
                    }
                    slot = &s;
                } else {
                    s.state = Free;
                    ++m_framesDropped;
                }
            }
            if (!slot)
                continue;
            slot->state = Sending;
        }

        const int64_t now = nowNs();
        const uint64_t hash = hashFrame(slot->mapped, slot->size,
                                        (static_cast<uint64_t>(slot->format) << 32) ^ (static_cast<uint64_t>(slot->width) << 16)
                                            ^ static_cast<uint64_t>(slot->height));
        if (m_hasLastHash && hash == m_lastHash && now - m_lastSendNs < kDuplicateRefreshNs) {
            ++m_statDuplicates;
        } else {
            if (++m_nextFrameId == 0)
                m_nextFrameId = 1;
            const int64_t wireDoneNs = sendFrame(*slot, m_nextFrameId);
            m_hasLastHash = true;
            m_lastHash = hash;
            m_lastSendNs = now;
            {
                std::lock_guard<std::mutex> lock(m_sentMutex);
                m_sent[m_sentNext] = {m_nextFrameId, wireDoneNs};
                m_sentNext = (m_sentNext + 1) % kSentHistory;
                m_sentCount = std::min(m_sentCount + 1, kSentHistory);
            }
            const double latencyMs = static_cast<double>(wireDoneNs - static_cast<int64_t>(slot->timestampNs)) / 1.0e6;
            m_statLatencyMs += latencyMs;
            m_statMaxLatencyMs = std::max(m_statMaxLatencyMs, latencyMs);
            m_sending = true;
        }
        slot->state = Free;

        if (nowNs() - m_statStartNs >= kStatsIntervalNs)
            logStats();
    }

    if (m_workerWaits)
        m_waitContext.doneCurrent();
}

int64_t NodeStreamSender::sendFrame(const Slot &slot, uint32_t frameId) {
    const int64_t startNs = nowNs();
    const uint32_t blockSize = nodestream::blockBytes(slot.format);
    const uint32_t rowBytes = nodestream::blocksAcross(slot.width) * blockSize;
    const uint32_t rows = nodestream::blocksAcross(slot.height);
    const uint32_t maxPayload = (static_cast<uint32_t>(m_config.maxDatagram) - sizeof(nodestream::PacketHeader)) / blockSize * blockSize;

    // Whole block rows per packet when a row fits, otherwise row segments.
    uint32_t rowsPerPacket = 0;
    uint32_t segmentsPerRow = 1;
    uint32_t packetCount = 0;
    if (rowBytes <= maxPayload) {
        rowsPerPacket = maxPayload / rowBytes;
        packetCount = (rows + rowsPerPacket - 1) / rowsPerPacket;
    } else {
        segmentsPerRow = (rowBytes + maxPayload - 1) / maxPayload;
        packetCount = rows * segmentsPerRow;
    }

    nodestream::PacketHeader header{};
    header.magic = nodestream::kMagic;
    header.version = nodestream::kVersion;
    header.format = static_cast<uint8_t>(slot.format);
    header.flags = 0;
    header.streamId = m_config.streamId;
    header.sessionId = m_sessionId;
    header.frameId = frameId;
    header.width = static_cast<uint16_t>(slot.width);
    header.height = static_cast<uint16_t>(slot.height);
    header.packetCount = packetCount;
    header.timestampNs = slot.timestampNs;

    const uint64_t totalBytes = slot.size + static_cast<uint64_t>(packetCount) * sizeof(header);
    uint64_t reservedBytes = 0;
    uint64_t sentBytes = 0;
    int64_t wireDoneNs = 0;
    for (uint32_t p = 0; p < packetCount; ++p) {
        uint32_t offset;
        uint32_t length;
        if (rowsPerPacket > 0) {
            const uint32_t row = p * rowsPerPacket;
            offset = row * rowBytes;
            length = std::min(rowsPerPacket, rows - row) * rowBytes;
        } else {
            const uint32_t row = p / segmentsPerRow;
            const uint32_t segment = p % segmentsPerRow;
            offset = row * rowBytes + segment * maxPayload;
            length = std::min(maxPayload, rowBytes - segment * maxPayload);
        }
        header.packetIndex = p;
        header.byteOffset = offset;
        header.byteLength = length;

        if (sentBytes >= reservedBytes) {
            const uint64_t chunk = std::min<uint64_t>(kPacingChunkBytes, totalBytes - reservedBytes);
            wireDoneNs = SendPacer::instance().reserve(static_cast<size_t>(chunk), m_rateMbps);
            reservedBytes += chunk;
        }
        sentBytes += length + sizeof(header);

        if (!m_socket.send(&header, sizeof(header), slot.mapped + offset, length))
            ++m_statSendErrors;
        m_statBytes += length + sizeof(header);

        if (m_quit)
            break;
    }

    ++m_statFrames;
    const int64_t endNs = nowNs();
    m_statSendMs += static_cast<double>(endNs - startNs) / 1.0e6;
    return std::max(wireDoneNs, endNs);
}

void NodeStreamSender::logStats() {
    const int64_t now = nowNs();
    const double seconds = static_cast<double>(now - m_statStartNs) / 1.0e9;
    if (seconds <= 0.0)
        return;
    const char *formatName = activeFormat() == nodestream::Format::BC1   ? "BC1"
                             : activeFormat() == nodestream::Format::BC3 ? "BC3"
                                                                         : "BC7";
    sgct::Log::Info(std::format(
        "NodeStreamSender {}:{} ({}): {:.1f} fps, {:.0f} Mbps, send {:.2f} ms/frame, capture to wire {:.2f} ms (max {:.2f}), "
        "gpu {:.2f} ms, duplicates {}, dropped {}, send errors {}",
        m_config.group, m_config.port, formatName, static_cast<double>(m_statFrames) / seconds,
        static_cast<double>(m_statBytes) * 8.0 / seconds / 1.0e6,
        m_statFrames ? m_statSendMs / static_cast<double>(m_statFrames) : 0.0,
        m_statFrames ? m_statLatencyMs / static_cast<double>(m_statFrames) : 0.0, m_statMaxLatencyMs, m_gpuTimeMs.load(),
        m_statDuplicates, m_framesDropped.exchange(0), m_statSendErrors));
    m_statStartNs = now;
    m_statFrames = 0;
    m_statDuplicates = 0;
    m_statBytes = 0;
    m_statSendErrors = 0;
    m_statSendMs = 0.0;
    m_statLatencyMs = 0.0;
    m_statMaxLatencyMs = 0.0;
}

void NodeStreamSender::cleanupGL() {
    stopThread();
    m_socket.close();
    for (Slot &slot : m_slots)
        releaseSlot(slot);
    m_encoder.cleanupGL();
    m_waitContext.destroy();
    m_waitContextTried = false;
    m_workerWaits = false;
    m_configured = false;
    m_socketOk = false;
}
