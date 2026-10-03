/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef NODESTREAMRECEIVER_H
#define NODESTREAMRECEIVER_H

#include "nodestreamprotocol.h"
#include "nodestreamsocket.h"

#include <sgct/opengl.h>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

/**
 * Receives a block-compressed texture stream on a worker thread and assembles
 * the frames directly into persistently mapped pixel unpack buffers.
 *
 * All methods except start/stop/lastError must be called on the OpenGL thread.
 */
class NodeStreamReceiver {
public:
    struct Frame {
        int slot = -1;
        GLuint pbo = 0;
        uint32_t sessionId = 0;
        uint32_t frameId = 0;
        // 0 for a keyframe, otherwise the frame this delta applies on top of.
        uint32_t baseFrameId = 0;
        nodestream::Format format = nodestream::Format::BC1;
        int width = 0;
        int height = 0;
        bool complete = false;
        // True when the texture matches this frame exactly after the upload.
        bool keepsExact = false;
        // Sorted, merged byte ranges that were received (only for partial and delta frames).
        std::vector<std::pair<uint32_t, uint32_t>> ranges;
    };

    NodeStreamReceiver();
    ~NodeStreamReceiver();

    NodeStreamReceiver(const NodeStreamReceiver &) = delete;
    NodeStreamReceiver &operator=(const NodeStreamReceiver &) = delete;

    bool start(const std::string &group, uint16_t port, const std::string &interfaceAddress, uint32_t streamId);
    void stop();
    bool isRunning() const;
    std::string lastError() const;

    // Releases slots whose uploads have finished and (re)allocates buffers.
    void serviceGL();

    // Frame-locked selection: waits up to maxWaitMs (once per target) for the target
    // frame. Falls back to the newest complete frame up to the target. With
    // allowPartial, an incomplete target follows it for upload on top.
    // Delta frames are returned together with the frames they build on, in upload
    // order. A delta whose base is not available is held back unless allowMissingBase.
    std::vector<Frame> acquireFrameLocked(uint32_t sessionId, uint32_t targetFrameId, int maxWaitMs, bool hasLastUploaded,
                                          uint32_t lastUploadedFrameId, bool textureExact, bool allowPartial,
                                          bool allowMissingBase);
    // Newest complete frame or, with allowPartial, a partial frame that a newer frame has superseded.
    std::vector<Frame> acquireNewest(bool hasLastUploaded, uint32_t lastUploadedFrameId, bool textureExact,
                                     bool allowPartial, bool allowMissingBase);

    // Must be called after the upload commands for the frame have been issued.
    void finishUpload(const Frame &frame);

    void cleanupGL();

private:
    enum SlotState {
        Free = 0,
        Filling,
        Complete,
        Uploading
    };

    struct Slot {
        GLuint pbo = 0;
        GLsync fence = nullptr;
        uint8_t *mapped = nullptr;
        size_t capacity = 0;
        SlotState state = Free;
        uint32_t frameId = 0;
        uint32_t baseFrameId = 0;
        nodestream::Format format = nodestream::Format::BC1;
        int width = 0;
        int height = 0;
        uint32_t packetCount = 0;
        uint32_t receivedCount = 0;
        int64_t firstPacketNs = 0;
        int64_t completeNs = 0;
        std::vector<uint8_t> received;
        std::vector<std::pair<uint32_t, uint32_t>> ranges;
    };

    static constexpr int kSlotCount = 4;

    void threadMain();
    void handlePacket(const uint8_t *data, size_t size);
    Slot *slotForPacket(const nodestream::PacketHeader &header);
    // Complete frames to upload in order so that the texture ends up exactly at slot end, or empty.
    std::vector<int> chainTo(int end, bool hasLastUploaded, uint32_t lastUploadedFrameId, bool textureExact) const;
    Frame takeSlot(int index, bool keepsExact);
    void freeOlderThan(uint32_t frameId);
    void reallocateSlot(Slot &slot, size_t capacity);
    void releaseSlotGL(Slot &slot);
    void logStats();

    NodeStreamSocket m_socket;
    std::thread m_thread;
    std::atomic_bool m_quit = false;
    std::atomic_bool m_running = false;
    uint32_t m_streamId = 0;
    std::string m_endpoint;
    // Receive thread only. LZ4 reads back its output, so it must not decompress into the mapped buffers.
    std::vector<uint8_t> m_decompressBuffer;

    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    Slot m_slots[kSlotCount];
    bool m_hasSession = false;
    uint32_t m_sessionId = 0;
    bool m_hasAcquired = false;
    uint32_t m_lastAcquiredFrameId = 0;
    size_t m_requiredCapacity = 0;
    std::string m_lastError;

    bool m_hasWaited = false;
    uint32_t m_waitedSession = 0;
    uint32_t m_waitedTarget = 0;
    bool m_hasPrevTarget = false;
    uint32_t m_prevTargetSession = 0;
    uint32_t m_prevTarget = 0;

    // Stats, guarded by m_mutex.
    uint64_t m_statPackets = 0;
    uint64_t m_statInvalid = 0;
    uint64_t m_statCompleted = 0;
    uint64_t m_statIncomplete = 0;
    uint64_t m_statPartialUploads = 0;
    uint64_t m_statMissingPackets = 0;
    uint64_t m_statLatePackets = 0;
    uint64_t m_statLateTargets = 0;
    uint64_t m_statHeld = 0;
    uint64_t m_statDeltas = 0;
    uint64_t m_statBaseHolds = 0;
    uint64_t m_statMissingBase = 0;
    uint64_t m_statWaits = 0;
    uint64_t m_statTargetRepeats = 0;
    uint64_t m_statTargetSkips = 0;
    uint64_t m_statSlackCount = 0;
    double m_statSpreadMs = 0.0;
    double m_statSlackMs = 0.0;
    double m_statMinSlackMs = 0.0;
    double m_statWaitMs = 0.0;
    int64_t m_statStartNs = 0;
};

#endif // NODESTREAMRECEIVER_H
