/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nodestreamsender.h"
#include "layers/baselayer.h"

#define LZ4_STATIC_LINKING_ONLY
#include "lz4/lz4.h"

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
// Above this fraction of changed units a keyframe is sent instead of a delta frame.
constexpr double kDeltaMaxDirtyFraction = 0.6;
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
    m_hasSent = false;
    m_lastKeyframeNs = 0;
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
        const nodestream::UnitLayout layout =
            nodestream::makeUnitLayout(slot->format, slot->width, slot->height, m_config.maxDatagram);
        const uint64_t layoutKey = (static_cast<uint64_t>(slot->format) << 32)
                                   ^ (static_cast<uint64_t>(slot->width) << 16) ^ static_cast<uint64_t>(slot->height);
        m_newUnitHashes.resize(layout.unitCount);
        for (uint32_t unit = 0; unit < layout.unitCount; ++unit) {
            uint32_t offset;
            uint32_t length;
            nodestream::unitRange(layout, unit, offset, length);
            m_newUnitHashes[unit] = hashFrame(slot->mapped + offset, length, layoutKey);
        }
        const bool sameLayout = m_hasSent && layoutKey == m_unitLayoutKey;
        m_dirtyUnits.clear();
        if (sameLayout) {
            for (uint32_t unit = 0; unit < layout.unitCount; ++unit) {
                if (m_newUnitHashes[unit] != m_unitHashes[unit])
                    m_dirtyUnits.push_back(unit);
            }
        }
        const int64_t keyframeIntervalNs = static_cast<int64_t>(std::max(m_config.keyframeIntervalMs, 1)) * 1'000'000;
        const bool keyframeDue = !sameLayout || now - m_lastKeyframeNs >= keyframeIntervalNs;
        if (!keyframeDue && m_dirtyUnits.empty()) {
            ++m_statDuplicates;
        } else {
            const bool keyframe = keyframeDue || !m_config.deltaFrames
                                  || static_cast<double>(m_dirtyUnits.size()) > kDeltaMaxDirtyFraction * layout.unitCount;
            const uint32_t baseFrameId = keyframe ? 0 : m_nextFrameId;
            if (++m_nextFrameId == 0)
                m_nextFrameId = 1;
            const int64_t wireDoneNs = sendFrame(*slot, layout, m_nextFrameId, baseFrameId, keyframe ? nullptr : &m_dirtyUnits);
            if (keyframe)
                m_lastKeyframeNs = now;
            m_hasSent = true;
            m_unitLayoutKey = layoutKey;
            m_unitHashes.swap(m_newUnitHashes);
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

int64_t NodeStreamSender::sendFrame(const Slot &slot, const nodestream::UnitLayout &layout, uint32_t frameId,
                                    uint32_t baseFrameId, const std::vector<uint32_t> *units) {
    const int64_t startNs = nowNs();
    const uint32_t packetCount = units ? static_cast<uint32_t>(units->size()) : layout.unitCount;

    nodestream::PacketHeader header{};
    header.magic = nodestream::kMagic;
    header.version = nodestream::kVersion;
    header.format = static_cast<uint8_t>(slot.format);
    header.streamId = m_config.streamId;
    header.sessionId = m_sessionId;
    header.frameId = frameId;
    header.baseFrameId = baseFrameId;
    header.width = static_cast<uint16_t>(slot.width);
    header.height = static_cast<uint16_t>(slot.height);
    header.packetCount = packetCount;
    header.timestampNs = slot.timestampNs;

    if (m_config.lz4 && !m_lz4State) {
        m_lz4State = std::make_unique<LZ4_stream_u>();
        LZ4_initStream(m_lz4State.get(), sizeof(LZ4_stream_u));
    }
    if (m_config.lz4)
        m_lz4Buffer.resize(static_cast<size_t>(LZ4_compressBound(static_cast<int>(layout.maxPayload))));

    uint64_t unsentBound = static_cast<uint64_t>(packetCount) * sizeof(header);
    if (units) {
        for (const uint32_t unit : *units) {
            uint32_t offset;
            uint32_t length;
            nodestream::unitRange(layout, unit, offset, length);
            unsentBound += length;
        }
    } else {
        unsentBound += slot.size;
    }

    uint64_t reservedBytes = 0;
    uint64_t sentBytes = 0;
    int64_t wireDoneNs = 0;
    int64_t compressNs = 0;
    for (uint32_t p = 0; p < packetCount; ++p) {
        uint32_t offset;
        uint32_t length;
        nodestream::unitRange(layout, units ? (*units)[p] : p, offset, length);
        header.packetIndex = p;
        header.byteOffset = offset;
        header.byteLength = length;
        header.flags = 0;

        const uint8_t *payload = slot.mapped + offset;
        uint32_t payloadSize = length;
        if (m_config.lz4) {
            const int64_t compressStartNs = nowNs();
            const int compressed = LZ4_compress_fast_extState_fastReset(
                m_lz4State.get(), reinterpret_cast<const char *>(payload), m_lz4Buffer.data(), static_cast<int>(length),
                static_cast<int>(m_lz4Buffer.size()), 1);
            compressNs += nowNs() - compressStartNs;
            if (compressed > 0 && static_cast<uint32_t>(compressed) < length) {
                payload = reinterpret_cast<const uint8_t *>(m_lz4Buffer.data());
                payloadSize = static_cast<uint32_t>(compressed);
                header.flags = nodestream::kFlagLz4;
            }
        }

        // Reserved on the bytes actually sent; the last chunk may cover a little more than needed.
        const uint64_t packetBytes = payloadSize + sizeof(header);
        if (sentBytes + packetBytes > reservedBytes) {
            const uint64_t chunk = std::max<uint64_t>(packetBytes, std::min<uint64_t>(kPacingChunkBytes, unsentBound));
            wireDoneNs = SendPacer::instance().reserve(static_cast<size_t>(chunk), m_rateMbps);
            reservedBytes += chunk;
        }
        sentBytes += packetBytes;
        unsentBound -= length + sizeof(header);

        if (!m_socket.send(&header, sizeof(header), payload, payloadSize))
            ++m_statSendErrors;
        m_statBytes += packetBytes;
        m_statRawBytes += length + sizeof(header);

        if (m_quit)
            break;
    }

    ++m_statFrames;
    if (!units)
        ++m_statKeyframes;
    m_statUnitsSent += packetCount;
    m_statUnitsTotal += layout.unitCount;
    m_statCompressMs += static_cast<double>(compressNs) / 1.0e6;
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
        "gpu {:.2f} ms, keyframes {}, deltas {} ({:.0f}% of units sent), wire/raw {:.0f}% (lz4 {:.2f} ms/frame), "
        "duplicates {}, dropped {}, send errors {}",
        m_config.group, m_config.port, formatName, static_cast<double>(m_statFrames) / seconds,
        static_cast<double>(m_statBytes) * 8.0 / seconds / 1.0e6,
        m_statFrames ? m_statSendMs / static_cast<double>(m_statFrames) : 0.0,
        m_statFrames ? m_statLatencyMs / static_cast<double>(m_statFrames) : 0.0, m_statMaxLatencyMs, m_gpuTimeMs.load(),
        m_statKeyframes, m_statFrames - m_statKeyframes,
        m_statUnitsTotal ? 100.0 * static_cast<double>(m_statUnitsSent) / static_cast<double>(m_statUnitsTotal) : 0.0,
        m_statRawBytes ? 100.0 * static_cast<double>(m_statBytes) / static_cast<double>(m_statRawBytes) : 100.0,
        m_statFrames ? m_statCompressMs / static_cast<double>(m_statFrames) : 0.0, m_statDuplicates,
        m_framesDropped.exchange(0), m_statSendErrors));
    m_statStartNs = now;
    m_statFrames = 0;
    m_statKeyframes = 0;
    m_statUnitsSent = 0;
    m_statUnitsTotal = 0;
    m_statDuplicates = 0;
    m_statBytes = 0;
    m_statRawBytes = 0;
    m_statSendErrors = 0;
    m_statSendMs = 0.0;
    m_statCompressMs = 0.0;
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
