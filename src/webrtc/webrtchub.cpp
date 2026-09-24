/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "webrtc/webrtchub.h"

#include <rtc/rtc.hpp>
#include <sgct/log.h>

#include <QJsonDocument>
#include <QJsonObject>

#include <cstring>
#include <algorithm>

namespace {

void writeBigEndian64(std::uint8_t *out, std::uint64_t value) {
    for (int i = 7; i >= 0; --i) {
        out[i] = static_cast<std::uint8_t>(value & 0xFF);
        value >>= 8;
    }
}

std::string signalingMessage(const QJsonObject &object) {
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact)).toStdString();
}

} // namespace

WebRtcHub &WebRtcHub::instance() {
    static WebRtcHub hub;
    return hub;
}

WebRtcHub::~WebRtcHub() {
    stopIfNoFeeds(true);
}

void WebRtcHub::registerFeed(const std::string &layerId, std::function<void()> requestKeyframe) {
    bool startedNow = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto &feed = m_feeds[layerId];
        feed.requestKeyframe = std::move(requestKeyframe);
        feed.codec.store(WebRtcVideoCodec::Unknown, std::memory_order_relaxed);

        if (!m_server) {
            try {
                rtc::WebSocketServerConfiguration serverConfig;
                serverConfig.port = kWebRtcHubPort;
                m_server = std::make_shared<rtc::WebSocketServer>(serverConfig);
                m_server->onClient([this](std::shared_ptr<rtc::WebSocket> ws) {
                    handleClient(std::move(ws));
                });
                startedNow = true;
            } catch (const std::exception &error) {
                m_server.reset();
                sgct::Log::Error("WebRtcHub: could not start the signaling server on port "
                                 + std::to_string(kWebRtcHubPort) + ": " + error.what() + "\n");
            }
        }
    }

    if (startedNow) {
        sgct::Log::Info("WebRtcHub: signaling server listening on port "
                        + std::to_string(kWebRtcHubPort) + "\n");
    }
}

void WebRtcHub::updateFeedCodec(const std::string &layerId, WebRtcVideoCodec codec) {
    if (codec == WebRtcVideoCodec::Unknown) {
        return;
    }

    std::vector<std::shared_ptr<NodeConnection>> waiting;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_feeds.find(layerId);
        if (it == m_feeds.end()) {
            return;
        }
        it->second.codec.store(codec, std::memory_order_relaxed);

        // Nodes that joined before the master finished negotiating are waiting for their
        // metadata frame; deliver it now.
        for (const auto &conn : m_nodes) {
            if (!conn->layerId.empty() && conn->dc && !conn->metadataSent) {
                waiting.push_back(conn);
            }
        }
    }

    for (auto &conn : waiting) {
        sendMetadata(conn);
    }
}

void WebRtcHub::unregisterFeed(const std::string &layerId) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_feeds.erase(layerId);
    }
    stopIfNoFeeds();
}

void WebRtcHub::stopIfNoFeeds(bool force) {
    std::vector<std::shared_ptr<NodeConnection>> nodes;
    std::shared_ptr<rtc::WebSocketServer> server;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!force && !m_feeds.empty()) {
            return;
        }
        nodes = m_nodes;
        m_nodes.clear();
        // Move the server out: destroying it joins its accept thread, which may be inside
        // handleClient() waiting for this very mutex.
        server = std::move(m_server);
    }

    for (auto &conn : nodes) {
        removeNode(conn);
    }
    // The WebSocketServer destructor stops the accept thread; done after releasing m_mutex.
}

void WebRtcHub::publishVideo(const std::string &layerId, const std::uint8_t *data, std::size_t size,
                             std::uint32_t rtpTimestamp) {
    if (!data || size == 0) {
        return;
    }

    // Frame: [type][u64 BE timestamp][Annex-B access unit]. Built once and shared by all nodes.
    std::vector<std::uint8_t> frame(1 + 8 + size);
    frame[0] = kWebRtcRelayVideo;
    writeBigEndian64(frame.data() + 1, rtpTimestamp);
    std::memcpy(frame.data() + 9, data, size);

    fanOut(layerId, frame);
}

void WebRtcHub::publishAudio(const std::string &layerId, const std::uint8_t *data, std::size_t size,
                             std::uint32_t rtpTimestamp) {
    if (!data || size == 0) {
        return;
    }

    // Frame: [type][u64 BE timestamp][depacketized Opus payload].
    std::vector<std::uint8_t> frame(1 + 8 + size);
    frame[0] = kWebRtcRelayAudio;
    writeBigEndian64(frame.data() + 1, rtpTimestamp);
    std::memcpy(frame.data() + 9, data, size);

    fanOut(layerId, frame);
}

void WebRtcHub::fanOut(const std::string &layerId, const std::vector<std::uint8_t> &frame) {
    // Collect the targets under the lock, then send without holding it: sendBuffer() only
    // enqueues into each node's SCTP stream and must not serialize against signaling.
    std::vector<std::shared_ptr<rtc::DataChannel>> targets;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto &conn : m_nodes) {
            if (!conn->layerId.empty() && conn->layerId == layerId && conn->dc) {
                targets.push_back(conn->dc);
            }
        }
    }

    for (auto &dc : targets) {
        dc->sendBuffer(frame);
    }
}

void WebRtcHub::handleClient(std::shared_ptr<rtc::WebSocket> ws) {
    if (!ws) {
        return;
    }

    auto conn = std::make_shared<NodeConnection>();
    conn->ws = ws;

    // Register the connection before wiring callbacks so no message can race ahead of it.
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_nodes.push_back(conn);
    }

    ws->onMessage([this, conn](rtc::message_variant message) {
        if (const auto *text = std::get_if<rtc::string>(&message)) {
            handleSignaling(conn, *text);
        }
        // Binary signaling is not used; ignore.
    });

    ws->onClosed([this, conn] { removeNode(conn); });
    ws->onError([conn](std::string error) {
        sgct::Log::Warning("WebRtcHub: signaling error from a node: " + error + "\n");
    });
}

void WebRtcHub::handleSignaling(const std::shared_ptr<NodeConnection> &conn, const std::string &text) {
    QJsonParseError parseError{};
    const QJsonObject object =
        QJsonDocument::fromJson(QString::fromStdString(text).toUtf8(), &parseError).object();
    if (parseError.error != QJsonParseError::NoError) {
        return;
    }

    auto send = [this, conn](const QJsonObject &message) {
        std::shared_ptr<rtc::WebSocket> ws;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            ws = conn->ws;
        }
        if (ws && ws->isOpen()) {
            ws->send(signalingMessage(message));
        }
    };

    const QString type = object.value(QStringLiteral("type")).toString();

    if (type == QStringLiteral("hello")) {
        const std::string layerId = object.value(QStringLiteral("layerId")).toString().toStdString();

        bool knownFeed = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            knownFeed = m_feeds.count(layerId) > 0 && conn->pc == nullptr;
        }
        if (!knownFeed) {
            sgct::Log::Warning("WebRtcHub: rejecting a node with unknown layer '" + layerId + "'\n");
            QJsonObject error;
            error.insert(QStringLiteral("type"), QStringLiteral("error"));
            error.insert(QStringLiteral("message"),
                         QString::fromLatin1(layerId.empty() ? "missing layer id" : "unknown layer"));
            send(error);
            removeNode(conn);
            return;
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            conn->layerId = layerId;
        }
        sgct::Log::Info("WebRtcHub: a node connected for layer " + layerId + "\n");
        createPeerForNode(conn);
        QJsonObject ready;
        ready.insert(QStringLiteral("type"), QStringLiteral("ready"));
        send(ready);
    } else if (type == QStringLiteral("offer")) {
        std::shared_ptr<rtc::PeerConnection> pc;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            pc = conn->pc;
        }
        if (!pc) {
            return; // offer before hello; the node will retry on reconnect
        }
        try {
            pc->setRemoteDescription(
                rtc::Description(object.value(QStringLiteral("sdp")).toString().toStdString(),
                                 rtc::Description::Type::Offer));
            // Auto-negotiation is disabled on the hub's peer connections, so generate the answer
            // explicitly; onLocalDescription() sends it to the node.
            pc->setLocalDescription(rtc::Description::Type::Answer);
        } catch (const std::exception &error) {
            sgct::Log::Error(std::string("WebRtcHub: could not apply the node offer: ") + error.what() + "\n");
            QJsonObject badOffer;
            badOffer.insert(QStringLiteral("type"), QStringLiteral("error"));
            badOffer.insert(QStringLiteral("message"), QStringLiteral("bad offer"));
            send(badOffer);
            removeNode(conn);
        }
    } else if (type == QStringLiteral("candidate")) {
        std::shared_ptr<rtc::PeerConnection> pc;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            pc = conn->pc;
        }
        if (!pc) {
            return;
        }
        try {
            pc->addRemoteCandidate(
                rtc::Candidate(object.value(QStringLiteral("candidate")).toString().toStdString()));
        } catch (const std::exception &error) {
            sgct::Log::Warning(std::string("WebRtcHub: ignoring a malformed candidate: ") + error.what() + "\n");
        }
    } else if (type == QStringLiteral("keyframe")) {
        // A node's decoder is starved: ask the master's WHEP session for an IDR.
        std::function<void()> requestKeyframe;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_feeds.find(conn->layerId);
            if (it != m_feeds.end()) {
                requestKeyframe = it->second.requestKeyframe;
            }
        }
        if (requestKeyframe) {
            requestKeyframe();
        }
    } else if (type == QStringLiteral("error")) {
        sgct::Log::Warning("WebRtcHub: a node reported an error: "
                           + object.value(QStringLiteral("message")).toString().toStdString() + "\n");
    }
}

void WebRtcHub::createPeerForNode(const std::shared_ptr<NodeConnection> &conn) {
    rtc::Configuration config;
    // All nodes share one UDP socket/port on the master (libjuice MUX mode); each node still
    // gets its own PeerConnection and DataChannel.
    config.enableIceUdpMux = true;
    config.maxMessageSize = kWebRtcRelayMaxMessageSize;
    // The hub answers explicitly in the "offer" handler below. Auto-negotiation would also make
    // libdatachannel generate surprise local offers (e.g. when a remote data channel appears)
    // that our onLocalDescription handler would mislabel as an answer to the node.
    config.disableAutoNegotiation = true;

    std::shared_ptr<rtc::PeerConnection> pc;
    try {
        pc = std::make_shared<rtc::PeerConnection>(config);
    } catch (const std::exception &error) {
        sgct::Log::Error(std::string("WebRtcHub: could not create the peer connection: ") + error.what() + "\n");
        removeNode(conn);
        return;
    }

    pc->onLocalDescription([this, conn](const rtc::Description &description) {
        // The master only ever produces an answer (the node is the offerer).
        QJsonObject message;
        message.insert(QStringLiteral("type"), QStringLiteral("answer"));
        message.insert(QStringLiteral("sdp"), QString::fromStdString(std::string(description)));
        std::shared_ptr<rtc::WebSocket> ws;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            ws = conn->ws;
        }
        if (ws && ws->isOpen()) {
            ws->send(signalingMessage(message));
        }
    });

    pc->onLocalCandidate([this, conn](const rtc::Candidate &candidate) {
        QJsonObject message;
        message.insert(QStringLiteral("type"), QStringLiteral("candidate"));
        message.insert(QStringLiteral("candidate"), QString::fromStdString(candidate.candidate()));
        std::shared_ptr<rtc::WebSocket> ws;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            ws = conn->ws;
        }
        if (ws && ws->isOpen()) {
            ws->send(signalingMessage(message));
        }
    });

    pc->onDataChannel([this, conn](std::shared_ptr<rtc::DataChannel> dc) {
        onNodeDataChannel(conn, std::move(dc));
    });

    pc->onStateChange([this, conn](rtc::PeerConnection::State state) {
        if (state == rtc::PeerConnection::State::Failed || state == rtc::PeerConnection::State::Closed) {
            removeNode(conn);
        }
    });

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        conn->pc = pc;
    }
}

void WebRtcHub::onNodeDataChannel(const std::shared_ptr<NodeConnection> &conn,
                                  std::shared_ptr<rtc::DataChannel> dc) {
    if (!dc || dc->label() != kWebRtcRelayDataChannelLabel) {
        return; // unexpected channel; ignore it
    }

    bool sendNow = false;
    WebRtcVideoCodec codec = WebRtcVideoCodec::Unknown;
    std::function<void()> requestKeyframe;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        conn->dc = dc;
        auto it = m_feeds.find(conn->layerId);
        if (it != m_feeds.end()) {
            codec = it->second.codec.load(std::memory_order_relaxed);
            requestKeyframe = it->second.requestKeyframe;
            sendNow = codec != WebRtcVideoCodec::Unknown;
        }
    }

    // Ask the master's WHEP session for an IDR so this node can start decoding immediately.
    if (requestKeyframe) {
        requestKeyframe();
    }

    if (!sendNow) {
        return; // updateFeedCodec() will deliver the metadata once negotiation completes
    }
    sendMetadata(conn);
}

bool WebRtcHub::sendMetadata(const std::shared_ptr<NodeConnection> &conn) {
    std::shared_ptr<rtc::DataChannel> dc;
    WebRtcVideoCodec codec = WebRtcVideoCodec::Unknown;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (conn->metadataSent || !conn->dc) {
            return false;
        }
        auto it = m_feeds.find(conn->layerId);
        if (it == m_feeds.end()) {
            return false;
        }
        codec = it->second.codec.load(std::memory_order_relaxed);
        if (codec == WebRtcVideoCodec::Unknown) {
            return false;
        }
        conn->metadataSent = true;
        dc = conn->dc;
    }

    // Frame: [type][1 byte codec]. The node starts decoding only after receiving it.
    const std::vector<std::uint8_t> frame{static_cast<std::uint8_t>(kWebRtcRelayMetadata),
                                          static_cast<std::uint8_t>(codec)};
    dc->sendBuffer(frame);
    return true;
}

void WebRtcHub::removeNode(const std::shared_ptr<NodeConnection> &conn) {
    std::shared_ptr<rtc::PeerConnection> pc;
    std::shared_ptr<rtc::WebSocket> ws;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = std::find_if(m_nodes.begin(), m_nodes.end(),
                               [&](const std::shared_ptr<NodeConnection> &candidate) {
                                   return candidate.get() == conn.get();
                               });
        if (it != m_nodes.end()) {
            m_nodes.erase(it);
        }
        // Take the members out so no other thread can observe a half-closed connection.
        pc = std::move(conn->pc);
        ws = std::move(conn->ws);
    }

    if (pc) {
        try {
            pc->close();
        } catch (const std::exception &error) {
            sgct::Log::Warning(std::string("WebRtcHub: error while closing a node peer connection: ")
                               + error.what() + "\n");
        }
    }
    if (ws && !ws->isClosed()) {
        try {
            ws->forceClose();
        } catch (const std::exception &error) {
            sgct::Log::Warning(std::string("WebRtcHub: error while closing a node WebSocket: ")
                               + error.what() + "\n");
        }
    }
}
