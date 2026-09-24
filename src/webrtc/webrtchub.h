/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "webrtc/webrtctypes.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
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
/// a small WebSocket signaling server (offer/answer + trickle ICE); all peer connections
/// run with enableIceUdpMux = true, so libjuice multiplexes every node's UDP traffic onto
/// a single master socket/port and each node still gets its own PeerConnection.
///
/// Wire protocol: the WebSocket carries JSON signaling messages; the data channel carries
/// one frame per message (see WebRtcRelayFrameType in webrtctypes.h).
///
/// Threading: registerFeed()/unregisterFeed()/updateFeedCodec() run on the main thread,
/// publishVideo()/publishAudio() run on the WHEP media threads and every libdatachannel
/// callback runs on its own thread. All state is guarded by m_mutex; publishes are
/// non-blocking (they only enqueue into each node's SCTP stream).
class WebRtcHub {
public:
    static WebRtcHub &instance();

    /// Registers the media feed of one layer that also exists on nodes. \p requestKeyframe
    /// is invoked when a node joins so it can start decoding at an IDR frame (MediaMTX does
    /// not send one on connect). The hub starts its signaling server when the first feed
    /// registers and stops it again when the last one unregisters. Must be paired with
    /// unregisterFeed().
    void registerFeed(const std::string &layerId, std::function<void()> requestKeyframe);

    /// Updates the negotiated video codec of a registered feed. Nodes that are already
    /// connected but still waiting for their metadata frame receive it as soon as this is
    /// called with a known codec (the master may re-negotiate mid-run on reconnect).
    void updateFeedCodec(const std::string &layerId, WebRtcVideoCodec codec);

    void unregisterFeed(const std::string &layerId);

    /// Fans one frame out to every node subscribed to \p layerId. Non-blocking and safe
    /// from any thread; a no-op when the feed is not registered or has no nodes.
    void publishVideo(const std::string &layerId, const std::uint8_t *data, std::size_t size,
                      std::uint32_t rtpTimestamp);
    void publishAudio(const std::string &layerId, const std::uint8_t *data, std::size_t size,
                      std::uint32_t rtpTimestamp);

private:
    WebRtcHub() = default;
    ~WebRtcHub();
    WebRtcHub(const WebRtcHub &) = delete;
    WebRtcHub &operator=(const WebRtcHub &) = delete;

    struct Feed {
        std::function<void()> requestKeyframe;
        std::atomic<WebRtcVideoCodec> codec{WebRtcVideoCodec::Unknown};
    };

    /// One connected node. The WebSocket is the signaling channel; the PeerConnection and
    /// DataChannel carry the media once ICE has completed.
    struct NodeConnection {
        std::string layerId; // empty until the node's hello message arrives
        std::shared_ptr<rtc::WebSocket> ws;
        std::shared_ptr<rtc::PeerConnection> pc;
        std::shared_ptr<rtc::DataChannel> dc; // set once the node's data channel opens
        bool metadataSent = false;            // guarded by m_mutex
    };

    void handleClient(std::shared_ptr<rtc::WebSocket> ws);
    void handleSignaling(const std::shared_ptr<NodeConnection> &conn, const std::string &text);
    void createPeerForNode(const std::shared_ptr<NodeConnection> &conn);
    void onNodeDataChannel(const std::shared_ptr<NodeConnection> &conn,
                           std::shared_ptr<rtc::DataChannel> dc);
    /// Sends the metadata frame to one node (caller holds no lock; takes m_mutex itself).
    bool sendMetadata(const std::shared_ptr<NodeConnection> &conn);
    void removeNode(const std::shared_ptr<NodeConnection> &conn);
    /// Closes every node connection and stops the signaling server. Used when the last feed
    /// unregisters (force = false) and in the destructor (force = true).
    void stopIfNoFeeds(bool force = false);
    /// Sends one prebuilt frame to every data channel of \p layerId (non-blocking).
    void fanOut(const std::string &layerId, const std::vector<std::uint8_t> &frame);

    // Guards m_feeds, m_nodes and m_server. Never call into libdatachannel while holding
    // it for long: close() calls happen after the lock is released.
    std::mutex m_mutex;
    std::unordered_map<std::string, Feed> m_feeds;
    std::vector<std::shared_ptr<NodeConnection>> m_nodes;
    std::shared_ptr<rtc::WebSocketServer> m_server;
};
