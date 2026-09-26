/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "webrtc/webrtctypes.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace rtc {
class WebSocketServer;
class WebSocket;
class PeerConnection;
class DataChannel;
} // namespace rtc

/// Master-side one-to-many distribution hub for WebRTC layers.
///
/// The master pulls the WHEP stream exactly once (WebRtcSource) and fans the depacketized
/// frames out to every connected node over a unique, reliable + ordered data channel per
/// node, so no node ever fetches the same upstream address itself. Nodes reach us through
/// a small WebSocket signaling server (offer/answer + trickle ICE). Every node gets its own
/// PeerConnection with its own UDP socket inside the relay port range (no ICE UDP mux), so
/// a slow node cannot fill a send buffer shared with the others.
///
/// Wire protocol: the WebSocket carries JSON signaling messages; the data channel carries
/// one frame per message (see WebRtcRelayFrameType in webrtctypes.h).
///
/// Sending: a publish builds the frame once and only enqueues a shared reference into each
/// node's bounded queue; one sender thread per node pushes it into that node's SCTP stream.
/// SCTP/DTLS/UDP send work therefore runs in parallel for all nodes, so every node receives
/// a frame at the same time, and the WHEP media thread that publishes is never blocked. A
/// node whose backlog exceeds kWebRtcRelayMaxNodeBacklogBytes/Ms has its queue dropped and
/// resumes at the next keyframe (announced by kWebRtcRelayDiscontinuity).
///
/// Threading: registerFeed()/unregisterFeed()/updateFeedCodec() run on the main thread,
/// publishVideo()/publishAudio() run on the WHEP media threads and every libdatachannel
/// callback runs on its own thread. Hub state is guarded by m_mutex, each node queue by its
/// own mutex (always taken after m_mutex, never before).
class WebRtcHub {
public:
    static WebRtcHub &instance();

    /// Registers the media feed of one layer that also exists on nodes. \p requestKeyframe
    /// is invoked when a node joins or falls behind so it can (re)start decoding at an IDR
    /// frame (MediaMTX does not send one on connect); it must be cheap and thread-safe. The
    /// hub starts its signaling server when the first feed registers and stops it again
    /// when the last one unregisters. Must be paired with unregisterFeed().
    void registerFeed(const std::string &layerId, std::function<void()> requestKeyframe);

    /// Updates the negotiated video codec of a registered feed. Nodes that are already
    /// connected but still waiting for their metadata frame receive it as soon as this is
    /// called with a known codec (the master may re-negotiate mid-run on reconnect).
    void updateFeedCodec(const std::string &layerId, WebRtcVideoCodec codec);

    void unregisterFeed(const std::string &layerId);

    /// Fans one frame out to every node subscribed to \p layerId. Non-blocking and safe
    /// from any thread; a no-op when the feed is not registered or has no nodes. \p pts is
    /// the unwrapped RTP timestamp (90 kHz video, 48 kHz audio).
    void publishVideo(const std::string &layerId, const std::uint8_t *data, std::size_t size,
                      std::int64_t pts, bool keyframe);
    void publishAudio(const std::string &layerId, const std::uint8_t *data, std::size_t size,
                      std::int64_t pts);

private:
    WebRtcHub() = default;
    ~WebRtcHub();
    WebRtcHub(const WebRtcHub &) = delete;
    WebRtcHub &operator=(const WebRtcHub &) = delete;

    struct Feed {
        std::function<void()> requestKeyframe;
        std::atomic<WebRtcVideoCodec> codec{WebRtcVideoCodec::Unknown};
    };

    using Clock = std::chrono::steady_clock;

    struct QueuedFrame {
        std::shared_ptr<const std::vector<std::uint8_t>> bytes; // shared by all nodes
        Clock::time_point enqueued;
    };

    /// One connected node. The WebSocket is the signaling channel; the PeerConnection and
    /// DataChannel carry the media once ICE has completed.
    struct NodeConnection {
        std::string layerId; // empty until the node's hello message arrives
        std::shared_ptr<rtc::WebSocket> ws;
        std::shared_ptr<rtc::PeerConnection> pc;
        std::shared_ptr<rtc::DataChannel> dc; // set once the node's data channel opens
        bool metadataSent = false;            // guarded by m_mutex; media flows only after it

        // Outgoing queue, drained by the node's sender thread. Guarded by queueMutex.
        std::mutex queueMutex;
        std::condition_variable queueCv;
        std::deque<QueuedFrame> queue;
        std::size_t queuedBytes = 0;
        bool waitingForKeyframe = true; // skip video until a keyframe (join / after a drop)
        bool discontinuity = false;     // announce kWebRtcRelayDiscontinuity before resuming
        bool stopSender = false;
        std::thread sender; // started with the data channel; joined via m_retiredSenders
    };

    enum class EnqueueResult { Queued, Skipped, Overflow };

    /// A node that receives media for a feed, with its data channel captured under m_mutex.
    struct Target {
        std::shared_ptr<NodeConnection> conn;
        std::shared_ptr<rtc::DataChannel> dc;
    };

    void handleClient(std::shared_ptr<rtc::WebSocket> ws);
    void handleSignaling(const std::shared_ptr<NodeConnection> &conn, const std::string &text);
    void createPeerForNode(const std::shared_ptr<NodeConnection> &conn);
    void onNodeDataChannel(const std::shared_ptr<NodeConnection> &conn,
                           std::shared_ptr<rtc::DataChannel> dc);
    /// Queues the metadata frame for one node (caller holds no lock; takes m_mutex itself).
    bool sendMetadata(const std::shared_ptr<NodeConnection> &conn);
    void removeNode(const std::shared_ptr<NodeConnection> &conn);
    /// Closes every node connection and stops the signaling server. Used when the last feed
    /// unregisters (force = false) and in the destructor (force = true).
    void stopIfNoFeeds(bool force = false);
    /// The nodes of \p layerId that already received their metadata frame.
    std::vector<Target> targetsFor(const std::string &layerId);
    /// Queues one prebuilt frame for every target (non-blocking) and asks the feed for a
    /// keyframe when a node had to be resynchronised.
    void fanOut(const std::string &layerId, const std::vector<Target> &targets,
                const std::shared_ptr<const std::vector<std::uint8_t>> &frame, bool isVideo, bool keyframe);
    /// Applies the per-node backlog policy and queues \p frame. Takes conn.queueMutex.
    static EnqueueResult enqueue(NodeConnection &conn, rtc::DataChannel &dc,
                                 const std::shared_ptr<const std::vector<std::uint8_t>> &frame,
                                 bool isVideo, bool keyframe);
    static void senderLoop(std::shared_ptr<NodeConnection> conn, std::shared_ptr<rtc::DataChannel> dc);
    /// Joins sender threads of removed nodes. Never called from a libdatachannel callback
    /// (a sender may be inside a send on the same transport), only from the main thread.
    void joinRetiredSenders();

    // Guards m_feeds, m_nodes, m_server and m_retiredSenders. Never call into libdatachannel
    // while holding it for long: close() calls happen after the lock is released.
    std::mutex m_mutex;
    std::unordered_map<std::string, Feed> m_feeds;
    std::vector<std::shared_ptr<NodeConnection>> m_nodes;
    std::shared_ptr<rtc::WebSocketServer> m_server;
    std::vector<std::thread> m_retiredSenders;
};
