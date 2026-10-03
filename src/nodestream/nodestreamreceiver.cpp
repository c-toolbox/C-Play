/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nodestreamreceiver.h"
#include "lz4/lz4.h"

#include <sgct/log.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <format>

namespace {

constexpr int kReceiveBufferBytes = 64 * 1024 * 1024;
constexpr int kReceiveTimeoutMs = 100;
constexpr int64_t kStatsIntervalNs = 5'000'000'000;

int64_t nowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// True when frame id a comes after b, with wrap-around.
bool isNewer(uint32_t a, uint32_t b) {
    return static_cast<int32_t>(a - b) > 0;
}

} // namespace

NodeStreamReceiver::NodeStreamReceiver() = default;

NodeStreamReceiver::~NodeStreamReceiver() {
    stop();
}

bool NodeStreamReceiver::start(const std::string &group, uint16_t port, const std::string &interfaceAddress,
                               uint32_t streamId) {
    stop();

    m_endpoint = std::format("{}:{}", group, port);
    if (!m_socket.openReceiver(group, port, interfaceAddress, kReceiveBufferBytes, kReceiveTimeoutMs)) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastError = std::format("Could not join {}: {}", m_endpoint, m_socket.lastError());
        sgct::Log::Error(std::format("NodeStreamReceiver: {}", m_lastError));
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_streamId = streamId;
        m_hasSession = false;
        m_hasAcquired = false;
        m_hasWaited = false;
        m_hasPrevTarget = false;
        m_lastError.clear();
        for (Slot &slot : m_slots) {
            if (slot.state == Filling || slot.state == Complete)
                slot.state = Free;
        }
        m_statStartNs = nowNs();
    }

    sgct::Log::Info(std::format("NodeStreamReceiver: listening on {}{}, receive buffer {}", m_endpoint,
                                interfaceAddress.empty() ? std::string() : " via " + interfaceAddress,
                                m_socket.receiveBufferSize()));
    m_quit = false;
    m_running = true;
    m_thread = std::thread(&NodeStreamReceiver::threadMain, this);
    return true;
}

void NodeStreamReceiver::stop() {
    if (m_thread.joinable()) {
        m_quit = true;
        m_thread.join();
    }
    m_socket.close();
    m_running = false;
    m_quit = false;
}

bool NodeStreamReceiver::isRunning() const {
    return m_running;
}

std::string NodeStreamReceiver::lastError() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastError;
}

void NodeStreamReceiver::threadMain() {
    std::vector<uint8_t> buffer(nodestream::kMaxDatagram);
    m_decompressBuffer.resize(nodestream::kMaxDatagram);
    while (!m_quit) {
        const int received = m_socket.receive(buffer.data(), buffer.size());
        if (received > 0) {
            handlePacket(buffer.data(), static_cast<size_t>(received));
        } else if (received < 0) {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_lastError = std::format("Receive failed on {}: {}", m_endpoint, m_socket.lastError());
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        if (nowNs() - m_statStartNs >= kStatsIntervalNs)
            logStats();
    }
}

void NodeStreamReceiver::handlePacket(const uint8_t *data, size_t size) {
    nodestream::PacketHeader header{};
    bool valid = size > sizeof(header);
    size_t payloadSize = 0;
    bool lz4 = false;
    if (valid) {
        std::memcpy(&header, data, sizeof(header));
        payloadSize = size - sizeof(header);
        lz4 = (header.flags & nodestream::kFlagLz4) != 0;
        valid = header.magic == nodestream::kMagic && header.version == nodestream::kVersion
                && nodestream::isConcreteFormat(header.format) && header.width > 0 && header.height > 0
                && header.width <= nodestream::kMaxDimension && header.height <= nodestream::kMaxDimension
                && header.byteLength > 0 && header.byteLength <= nodestream::kMaxDatagram
                && (lz4 ? payloadSize <= static_cast<size_t>(LZ4_compressBound(static_cast<int>(header.byteLength)))
                        : payloadSize == header.byteLength);
    }
    if (valid) {
        const auto format = static_cast<nodestream::Format>(header.format);
        const uint64_t blockSize = nodestream::blockBytes(format);
        const uint64_t rowBytes = nodestream::blocksAcross(header.width) * blockSize;
        const uint64_t frameSize = nodestream::frameBytes(format, header.width, header.height);
        const uint64_t offset = header.byteOffset;
        const uint64_t length = header.byteLength;
        const bool wholeRows = offset % rowBytes == 0 && length % rowBytes == 0;
        const bool withinRow = offset / rowBytes == (offset + length - 1) / rowBytes;
        valid = offset % blockSize == 0 && length % blockSize == 0 && offset + length <= frameSize
                && (wholeRows || withinRow) && header.packetCount > 0 && header.packetIndex < header.packetCount
                && header.packetCount <= frameSize / blockSize;
    }
    const uint8_t *payload = data + sizeof(header);
    if (valid && lz4) {
        const int decompressed = LZ4_decompress_safe(reinterpret_cast<const char *>(payload),
                                                     reinterpret_cast<char *>(m_decompressBuffer.data()),
                                                     static_cast<int>(payloadSize), static_cast<int>(header.byteLength));
        valid = decompressed == static_cast<int>(header.byteLength);
        payload = m_decompressBuffer.data();
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    ++m_statPackets;
    if (!valid) {
        ++m_statInvalid;
        return;
    }
    if (header.streamId != m_streamId)
        return;

    Slot *slot = slotForPacket(header);
    if (!slot || slot->received[header.packetIndex])
        return;

    std::memcpy(slot->mapped + header.byteOffset, payload, header.byteLength);
    slot->received[header.packetIndex] = 1;
    slot->ranges.emplace_back(header.byteOffset, header.byteLength);
    if (++slot->receivedCount == slot->packetCount) {
        slot->state = Complete;
        slot->completeNs = nowNs();
        m_statSpreadMs += static_cast<double>(slot->completeNs - slot->firstPacketNs) / 1.0e6;
        ++m_statCompleted;
        m_cv.notify_all();
    }
}

NodeStreamReceiver::Slot *NodeStreamReceiver::slotForPacket(const nodestream::PacketHeader &header) {
    if (!m_hasSession || header.sessionId != m_sessionId) {
        m_hasSession = true;
        m_sessionId = header.sessionId;
        m_hasAcquired = false;
        for (Slot &slot : m_slots) {
            if (slot.state == Filling || slot.state == Complete)
                slot.state = Free;
        }
    }

    const auto format = static_cast<nodestream::Format>(header.format);
    for (Slot &slot : m_slots) {
        if (slot.frameId != header.frameId || slot.state == Free)
            continue;
        if (slot.state != Filling)
            return nullptr;
        if (slot.format != format || slot.width != header.width || slot.height != header.height
            || slot.packetCount != header.packetCount || slot.baseFrameId != header.baseFrameId)
            return nullptr;
        return &slot;
    }

    // Late packets of frames that have already been shown are useless.
    if (m_hasAcquired && !isNewer(header.frameId, m_lastAcquiredFrameId)) {
        ++m_statLatePackets;
        return nullptr;
    }

    const size_t frameSize = nodestream::frameBytes(format, header.width, header.height);
    Slot *chosen = nullptr;
    for (Slot &slot : m_slots) {
        if (slot.state == Free && slot.mapped && slot.capacity >= frameSize) {
            chosen = &slot;
            break;
        }
    }
    if (!chosen) {
        // Recycle the oldest pending frame, if it is older than this one.
        for (Slot &slot : m_slots) {
            if ((slot.state == Filling || slot.state == Complete) && slot.capacity >= frameSize
                && isNewer(header.frameId, slot.frameId) && (!chosen || isNewer(chosen->frameId, slot.frameId)))
                chosen = &slot;
        }
    }
    if (!chosen) {
        bool anyLargeEnough = false;
        for (const Slot &slot : m_slots)
            anyLargeEnough = anyLargeEnough || slot.capacity >= frameSize;
        if (!anyLargeEnough)
            m_requiredCapacity = std::max(m_requiredCapacity, frameSize);
        return nullptr;
    }

    if (chosen->state == Filling)
        ++m_statIncomplete;
    chosen->state = Filling;
    chosen->frameId = header.frameId;
    chosen->baseFrameId = header.baseFrameId;
    chosen->format = format;
    chosen->width = header.width;
    chosen->height = header.height;
    chosen->packetCount = header.packetCount;
    chosen->receivedCount = 0;
    chosen->firstPacketNs = nowNs();
    chosen->received.assign(header.packetCount, 0);
    chosen->ranges.clear();
    chosen->ranges.reserve(header.packetCount);
    return chosen;
}

void NodeStreamReceiver::releaseSlotGL(Slot &slot) {
    if (slot.fence) {
        glDeleteSync(slot.fence);
        slot.fence = nullptr;
    }
    if (slot.pbo) {
        if (slot.mapped) {
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, slot.pbo);
            glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        }
        glDeleteBuffers(1, &slot.pbo);
    }
    slot.pbo = 0;
    slot.mapped = nullptr;
    slot.capacity = 0;
    slot.state = Free;
}

void NodeStreamReceiver::reallocateSlot(Slot &slot, size_t capacity) {
    releaseSlotGL(slot);
    const GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
    glGenBuffers(1, &slot.pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, slot.pbo);
    glBufferStorage(GL_PIXEL_UNPACK_BUFFER, static_cast<GLsizeiptr>(capacity), nullptr, flags);
    slot.mapped = static_cast<uint8_t *>(glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, static_cast<GLsizeiptr>(capacity), flags));
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    if (!slot.mapped) {
        sgct::Log::Error("NodeStreamReceiver: could not map upload buffer");
        releaseSlotGL(slot);
        return;
    }
    slot.capacity = capacity;
}

void NodeStreamReceiver::serviceGL() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (Slot &slot : m_slots) {
        if (slot.state != Uploading || !slot.fence)
            continue;
        const GLenum result = glClientWaitSync(slot.fence, 0, 0);
        if (result == GL_ALREADY_SIGNALED || result == GL_CONDITION_SATISFIED) {
            glDeleteSync(slot.fence);
            slot.fence = nullptr;
            slot.state = Free;
        }
    }

    if (m_requiredCapacity > 0) {
        bool allLargeEnough = true;
        for (Slot &slot : m_slots) {
            if (slot.capacity >= m_requiredCapacity)
                continue;
            if (slot.state == Uploading) {
                allLargeEnough = false;
                continue;
            }
            reallocateSlot(slot, m_requiredCapacity);
            allLargeEnough = allLargeEnough && slot.capacity >= m_requiredCapacity;
        }
        if (allLargeEnough)
            m_requiredCapacity = 0;
    }
}

std::vector<int> NodeStreamReceiver::chainTo(int end, bool hasLastUploaded, uint32_t lastUploadedFrameId,
                                             bool textureExact) const {
    std::vector<int> chain;
    int index = end;
    while (true) {
        const Slot &slot = m_slots[index];
        chain.push_back(index);
        if (slot.baseFrameId == 0 || (hasLastUploaded && textureExact && slot.baseFrameId == lastUploadedFrameId))
            break;
        index = -1;
        for (int i = 0; i < kSlotCount; ++i) {
            if (m_slots[i].state == Complete && m_slots[i].frameId == slot.baseFrameId)
                index = i;
        }
        if (index < 0 || static_cast<int>(chain.size()) >= kSlotCount)
            return {};
    }
    std::reverse(chain.begin(), chain.end());
    return chain;
}

NodeStreamReceiver::Frame NodeStreamReceiver::takeSlot(int index, bool keepsExact) {
    Slot &slot = m_slots[index];
    Frame frame;
    frame.slot = index;
    frame.pbo = slot.pbo;
    frame.sessionId = m_sessionId;
    frame.frameId = slot.frameId;
    frame.baseFrameId = slot.baseFrameId;
    frame.keepsExact = keepsExact;
    frame.format = slot.format;
    frame.width = slot.width;
    frame.height = slot.height;
    frame.complete = slot.state == Complete;
    if (frame.complete) {
        const double slackMs = static_cast<double>(nowNs() - slot.completeNs) / 1.0e6;
        m_statMinSlackMs = m_statSlackCount == 0 ? slackMs : std::min(m_statMinSlackMs, slackMs);
        m_statSlackMs += slackMs;
        ++m_statSlackCount;
    } else {
        ++m_statPartialUploads;
        m_statMissingPackets += slot.packetCount - slot.receivedCount;
    }
    if (slot.baseFrameId != 0) {
        ++m_statDeltas;
        if (!keepsExact)
            ++m_statMissingBase;
    }
    // A delta's buffer only holds its own units, so it is uploaded range by range even when complete.
    if (!frame.complete || slot.baseFrameId != 0) {
        std::vector<std::pair<uint32_t, uint32_t>> ranges = slot.ranges;
        std::sort(ranges.begin(), ranges.end());
        for (const auto &range : ranges) {
            if (!frame.ranges.empty() && frame.ranges.back().first + frame.ranges.back().second == range.first)
                frame.ranges.back().second += range.second;
            else
                frame.ranges.push_back(range);
        }
    }
    slot.state = Uploading;
    if (!m_hasAcquired || isNewer(slot.frameId, m_lastAcquiredFrameId)) {
        m_hasAcquired = true;
        m_lastAcquiredFrameId = slot.frameId;
    }
    return frame;
}

void NodeStreamReceiver::freeOlderThan(uint32_t frameId) {
    for (Slot &slot : m_slots) {
        if ((slot.state == Filling || slot.state == Complete) && isNewer(frameId, slot.frameId)) {
            if (slot.state == Filling)
                ++m_statIncomplete;
            slot.state = Free;
        }
    }
}

std::vector<NodeStreamReceiver::Frame> NodeStreamReceiver::acquireFrameLocked(uint32_t sessionId, uint32_t targetFrameId,
                                                                              int maxWaitMs, bool hasLastUploaded,
                                                                              uint32_t lastUploadedFrameId, bool textureExact,
                                                                              bool allowPartial, bool allowMissingBase) {
    std::vector<Frame> frames;
    std::unique_lock<std::mutex> lock(m_mutex);

    if (m_hasPrevTarget && m_prevTargetSession == sessionId) {
        const int32_t step = static_cast<int32_t>(targetFrameId - m_prevTarget);
        if (step == 0)
            ++m_statTargetRepeats;
        else if (step > 1)
            m_statTargetSkips += static_cast<uint64_t>(step - 1);
    }
    m_hasPrevTarget = true;
    m_prevTargetSession = sessionId;
    m_prevTarget = targetFrameId;

    if (hasLastUploaded && !isNewer(targetFrameId, lastUploadedFrameId))
        return frames;

    auto findTarget = [&]() -> int {
        if (!m_hasSession || m_sessionId != sessionId)
            return -1;
        for (int i = 0; i < kSlotCount; ++i) {
            if ((m_slots[i].state == Filling || m_slots[i].state == Complete) && m_slots[i].frameId == targetFrameId)
                return i;
        }
        return -1;
    };
    auto targetComplete = [&]() {
        const int i = findTarget();
        return i >= 0 && m_slots[i].state == Complete;
    };

    // Wait once per target, so that a late frame does not stall every node frame until it arrives.
    const bool waitedBefore = m_hasWaited && m_waitedSession == sessionId && m_waitedTarget == targetFrameId;
    if (maxWaitMs > 0 && !waitedBefore && !targetComplete()) {
        m_hasWaited = true;
        m_waitedSession = sessionId;
        m_waitedTarget = targetFrameId;
        const int64_t waitStartNs = nowNs();
        m_cv.wait_for(lock, std::chrono::milliseconds(maxWaitMs), [&] { return m_quit || targetComplete(); });
        m_statWaitMs += static_cast<double>(nowNs() - waitStartNs) / 1.0e6;
        ++m_statWaits;
    }

    if (!m_hasSession || m_sessionId != sessionId)
        return frames;

    auto takeChain = [&](const std::vector<int> &chain) {
        for (const int index : chain)
            frames.push_back(takeSlot(index, true));
    };

    const int target = findTarget();
    if (target >= 0 && m_slots[target].state == Complete) {
        const std::vector<int> chain = chainTo(target, hasLastUploaded, lastUploadedFrameId, textureExact);
        if (!chain.empty() || allowMissingBase) {
            if (chain.empty())
                frames.push_back(takeSlot(target, false));
            else
                takeChain(chain);
            freeOlderThan(targetFrameId);
            return frames;
        }
        ++m_statBaseHolds;
    } else {
        ++m_statLateTargets;
    }

    // Fall back to the newest complete frame up to the target that can be shown.
    int best = -1;
    std::vector<int> bestChain;
    for (int i = 0; i < kSlotCount; ++i) {
        const Slot &slot = m_slots[i];
        if (i == target || slot.state != Complete || isNewer(slot.frameId, targetFrameId))
            continue;
        if (hasLastUploaded && !isNewer(slot.frameId, lastUploadedFrameId))
            continue;
        if (best >= 0 && !isNewer(slot.frameId, m_slots[best].frameId))
            continue;
        std::vector<int> chain = chainTo(i, hasLastUploaded, lastUploadedFrameId, textureExact);
        if (chain.empty() && !allowMissingBase)
            continue;
        best = i;
        bestChain = std::move(chain);
    }
    bool exact = hasLastUploaded && textureExact;
    uint32_t currentId = lastUploadedFrameId;
    if (best >= 0) {
        if (bestChain.empty())
            frames.push_back(takeSlot(best, false));
        else
            takeChain(bestChain);
        exact = frames.back().keepsExact;
        currentId = frames.back().frameId;
    }
    // Showing what arrived of the target on top mixes two frames in one image.
    if (target >= 0 && m_slots[target].state == Filling && allowPartial) {
        const uint32_t base = m_slots[target].baseFrameId;
        if (base == 0 || (exact && base == currentId) || allowMissingBase)
            frames.push_back(takeSlot(target, false));
    }
    if (frames.empty())
        ++m_statHeld;
    else
        freeOlderThan(frames.back().frameId);
    return frames;
}

std::vector<NodeStreamReceiver::Frame> NodeStreamReceiver::acquireNewest(bool hasLastUploaded, uint32_t lastUploadedFrameId,
                                                                         bool textureExact, bool allowPartial,
                                                                         bool allowMissingBase) {
    std::vector<Frame> frames;
    std::lock_guard<std::mutex> lock(m_mutex);

    int newestComplete = -1;
    int newestStarted = -1;
    int best = -1;
    std::vector<int> bestChain;
    for (int i = 0; i < kSlotCount; ++i) {
        const Slot &slot = m_slots[i];
        if (slot.state != Filling && slot.state != Complete)
            continue;
        if (hasLastUploaded && !isNewer(slot.frameId, lastUploadedFrameId))
            continue;
        if (newestStarted < 0 || isNewer(slot.frameId, m_slots[newestStarted].frameId))
            newestStarted = i;
        if (slot.state != Complete)
            continue;
        if (newestComplete < 0 || isNewer(slot.frameId, m_slots[newestComplete].frameId))
            newestComplete = i;
        if (best >= 0 && !isNewer(slot.frameId, m_slots[best].frameId))
            continue;
        std::vector<int> chain = chainTo(i, hasLastUploaded, lastUploadedFrameId, textureExact);
        if (chain.empty())
            continue;
        best = i;
        bestChain = std::move(chain);
    }

    if (best >= 0) {
        for (const int index : bestChain)
            frames.push_back(takeSlot(index, true));
    } else if (newestComplete >= 0) {
        if (allowMissingBase)
            frames.push_back(takeSlot(newestComplete, false));
        else
            ++m_statBaseHolds;
    } else if (newestStarted >= 0 && allowPartial) {
        // A frame that a newer frame has superseded will not receive more packets.
        int chosen = -1;
        for (int i = 0; i < kSlotCount; ++i) {
            const Slot &slot = m_slots[i];
            if (slot.state != Filling || i == newestStarted)
                continue;
            if (hasLastUploaded && !isNewer(slot.frameId, lastUploadedFrameId))
                continue;
            const bool baseOk = slot.baseFrameId == 0 || allowMissingBase
                                || (hasLastUploaded && textureExact && slot.baseFrameId == lastUploadedFrameId);
            if (baseOk && (chosen < 0 || isNewer(slot.frameId, m_slots[chosen].frameId)))
                chosen = i;
        }
        if (chosen >= 0)
            frames.push_back(takeSlot(chosen, false));
    }
    if (!frames.empty())
        freeOlderThan(frames.back().frameId);
    return frames;
}

void NodeStreamReceiver::finishUpload(const Frame &frame) {
    if (frame.slot < 0 || frame.slot >= kSlotCount)
        return;
    std::lock_guard<std::mutex> lock(m_mutex);
    Slot &slot = m_slots[frame.slot];
    if (slot.fence)
        glDeleteSync(slot.fence);
    slot.fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

void NodeStreamReceiver::logStats() {
    std::lock_guard<std::mutex> lock(m_mutex);
    const int64_t now = nowNs();
    const double seconds = static_cast<double>(now - m_statStartNs) / 1.0e9;
    if (m_statPackets > 0 && seconds > 0.0) {
        sgct::Log::Info(std::format(
            "NodeStreamReceiver {}: {:.0f} packets/s, {:.1f} complete fps, frame spread {:.2f} ms, slack {:.2f} ms (min {:.2f}), "
            "incomplete {} (missing packets {}, late packets {}), partial uploads {}, late targets {}, held {}, "
            "deltas {} (missing base {}, base holds {}), "
            "waits {} ({:.2f} ms avg), target repeats {}, target skips {}, invalid {}",
            m_endpoint, static_cast<double>(m_statPackets) / seconds, static_cast<double>(m_statCompleted) / seconds,
            m_statCompleted ? m_statSpreadMs / static_cast<double>(m_statCompleted) : 0.0,
            m_statSlackCount ? m_statSlackMs / static_cast<double>(m_statSlackCount) : 0.0,
            m_statSlackCount ? m_statMinSlackMs : 0.0, m_statIncomplete, m_statMissingPackets, m_statLatePackets,
            m_statPartialUploads, m_statLateTargets, m_statHeld, m_statDeltas, m_statMissingBase, m_statBaseHolds,
            m_statWaits,
            m_statWaits ? m_statWaitMs / static_cast<double>(m_statWaits) : 0.0, m_statTargetRepeats, m_statTargetSkips,
            m_statInvalid));
    }
    m_statStartNs = now;
    m_statPackets = 0;
    m_statInvalid = 0;
    m_statCompleted = 0;
    m_statSpreadMs = 0.0;
    m_statSlackMs = 0.0;
    m_statMinSlackMs = 0.0;
    m_statSlackCount = 0;
    m_statMissingPackets = 0;
    m_statLatePackets = 0;
    m_statLateTargets = 0;
    m_statHeld = 0;
    m_statDeltas = 0;
    m_statMissingBase = 0;
    m_statBaseHolds = 0;
    m_statWaits = 0;
    m_statWaitMs = 0.0;
    m_statTargetRepeats = 0;
    m_statTargetSkips = 0;
    m_statIncomplete = 0;
    m_statPartialUploads = 0;
}

void NodeStreamReceiver::cleanupGL() {
    stop();
    std::lock_guard<std::mutex> lock(m_mutex);
    for (Slot &slot : m_slots)
        releaseSlotGL(slot);
    m_requiredCapacity = 0;
}
