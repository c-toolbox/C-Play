/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef NODESTREAMSENDER_H
#define NODESTREAMSENDER_H

#include "nodestreamencoder.h"
#include "nodestreamglcontext.h"
#include "nodestreamprotocol.h"
#include "nodestreamsocket.h"

#include <sgct/opengl.h>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

class BaseLayer;

struct NodeStreamSource {
    std::function<unsigned int()> textureId;
    std::function<int()> width;
    std::function<int()> height;

    bool valid() const {
        return textureId && width && height;
    }
};

struct NodeStreamConfig {
    std::string group = nodestream::kDefaultGroup;
    uint16_t port = nodestream::kDefaultPort;
    std::string interfaceAddress;
    int ttl = 1;
    bool loopback = false;
    int maxDatagram = 1472;
    // Combined send rate of all senders, 0 uses 85% of the interface link speed.
    int rateMbps = 0;
    nodestream::Format format = nodestream::Format::Auto;
    int maxFps = 0;
    uint32_t streamId = 0;

    bool operator==(const NodeStreamConfig &) const = default;
};

/**
 * Compresses a layer texture on the GPU and multicasts the blocks to the nodes.
 *
 * captureAndSend and cleanupGL must be called on the render thread with the
 * source's OpenGL context current. The packets are sent from a worker thread
 * straight out of persistently mapped staging buffers.
 */
class NodeStreamSender {
public:
    NodeStreamSender();
    ~NodeStreamSender();

    NodeStreamSender(const NodeStreamSender &) = delete;
    NodeStreamSender &operator=(const NodeStreamSender &) = delete;

    static NodeStreamSource sourceFromLayer(BaseLayer *layer);

    void setSource(const NodeStreamSource &source);

    // Encodes the current texture and queues it for sending. Reopens the
    // socket and starts a new session when the config changed.
    bool captureAndSend(const NodeStreamConfig &config);

    // Stops the worker thread and releases all OpenGL resources.
    void cleanupGL();

    bool isSending() const;
    uint32_t sessionId() const;
    // Newest frame that has been on the wire for at least guardNs. Never moves
    // backwards within a session.
    uint32_t targetFrameId(int64_t guardNs);
    nodestream::Format activeFormat() const;

private:
    enum SlotState : int {
        Free = 0,
        Encoding,
        Ready,
        Sending
    };

    struct Slot {
        GLuint buffer = 0;
        uint8_t *mapped = nullptr;
        size_t capacity = 0;
        GLsync fence = nullptr;
        std::atomic<int> state = Free;
        // Capture order; the wire frame id is assigned when the frame is sent.
        uint32_t sequence = 0;
        nodestream::Format format = nodestream::Format::BC1;
        int width = 0;
        int height = 0;
        size_t size = 0;
        uint64_t timestampNs = 0;
    };

    struct SentFrame {
        uint32_t frameId = 0;
        int64_t wireDoneNs = 0;
    };

    static constexpr int kSlotCount = 3;
    static constexpr int kSentHistory = 16;

    bool restart(const NodeStreamConfig &config);
    void stopThread();
    void pollEncodedSlots();
    // Waits for the encode fence on the worker thread, with the shared context current.
    void waitEncoded(Slot &slot);
    void markEncoded(Slot &slot);
    bool ensureSlotCapacity(Slot &slot, size_t size);
    void releaseSlot(Slot &slot);
    void threadMain();
    // Returns the estimated time the last packet left the network interface.
    int64_t sendFrame(const Slot &slot, uint32_t frameId);
    void logStats();

    NodeStreamSource m_source;
    NodeStreamConfig m_config;
    int m_rateMbps = 0;
    bool m_configured = false;
    bool m_socketOk = false;

    NodeStreamEncoder m_encoder;
    NodeStreamSocket m_socket;
    Slot m_slots[kSlotCount];

    NodeStreamSharedContext m_waitContext;
    bool m_waitContextTried = false;
    // When set, the worker thread waits on the encode fences instead of the render thread polling them.
    std::atomic_bool m_workerWaits = false;

    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic_bool m_quit = false;

    uint32_t m_nextSequence = 0;
    int64_t m_lastCaptureNs = 0;
    std::atomic<uint8_t> m_autoFormat = static_cast<uint8_t>(nodestream::Format::BC1);
    int m_framesWithoutAlpha = 0;

    // Send thread only.
    uint32_t m_nextFrameId = 0;
    bool m_hasLastHash = false;
    uint64_t m_lastHash = 0;
    int64_t m_lastSendNs = 0;

    std::atomic<uint32_t> m_sessionId = 0;
    std::atomic_bool m_sending = false;
    std::atomic<uint8_t> m_activeFormat = static_cast<uint8_t>(nodestream::Format::BC1);

    std::mutex m_sentMutex;
    SentFrame m_sent[kSentHistory];
    int m_sentCount = 0;
    int m_sentNext = 0;
    bool m_hasTarget = false;
    uint32_t m_target = 0;

    // Stats, logged periodically from the worker thread.
    std::atomic<uint64_t> m_framesDropped = 0;
    std::atomic<float> m_gpuTimeMs = -1.0f;
    uint64_t m_statFrames = 0;
    uint64_t m_statDuplicates = 0;
    uint64_t m_statBytes = 0;
    uint64_t m_statSendErrors = 0;
    double m_statSendMs = 0.0;
    double m_statLatencyMs = 0.0;
    double m_statMaxLatencyMs = 0.0;
    int64_t m_statStartNs = 0;
};

#endif // NODESTREAMSENDER_H
