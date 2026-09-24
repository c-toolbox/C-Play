/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "webrtc/webrtcrelayclient.h"

#include <rtc/rtc.hpp>
#include <sgct/log.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

using namespace Qt::Literals::StringLiterals;

namespace {

std::uint64_t readBigEndian64(const std::uint8_t *in) {
    std::uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value = (value << 8) | in[i];
    }
    return value;
}

std::string signalingMessage(const QJsonObject &object) {
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact)).toStdString();
}

/// Give up when the hub does not deliver its metadata frame (the negotiated codec) within
/// this window; the layer's reconnect machinery then tries again.
constexpr int kMetadataTimeoutMs = 10000;

} // namespace

WebRtcRelayClient::WebRtcRelayClient(std::string masterAddress, std::uint16_t hubPort, std::string layerId,
                                     QObject *parent)
    : WebRtcMediaSource(parent),
      m_masterAddress(std::move(masterAddress)),
      m_hubPort(hubPort),
      m_layerId(std::move(layerId)) {
}

WebRtcRelayClient::~WebRtcRelayClient() {
    stop();
}

void WebRtcRelayClient::setState(WebRtcStreamState state) {
    const WebRtcStreamState previous = m_state.exchange(state, std::memory_order_relaxed);
    if (previous != state) {
        Q_EMIT stateChanged(state);
    }
}

void WebRtcRelayClient::fail(const QString &reason) {
    // Only the first failure per session is reported; stop() resets the state to Idle.
    const WebRtcStreamState previous = m_state.exchange(WebRtcStreamState::Failed, std::memory_order_relaxed);
    if (previous == WebRtcStreamState::Failed || previous == WebRtcStreamState::Idle
        || previous == WebRtcStreamState::Stopping) {
        return;
    }
    Q_EMIT errorOccurred(reason);
    Q_EMIT stateChanged(WebRtcStreamState::Failed);
}

void WebRtcRelayClient::start() {
    stop(); // idempotent cleanup of any previous session

    webRtcEnsureLibDataChannelLogger();

    if (m_masterAddress.empty()) {
        fail(u"No master address in the cluster config; cannot join the WebRTC relay"_s);
        return;
    }

    m_iceStarted.store(false, std::memory_order_relaxed);
    auto session = std::make_shared<std::atomic<bool>>(true);
    m_session.store(session);

    setState(WebRtcStreamState::Connecting);
    sgct::Log::Info("WebRTC relay: connecting to hub " + m_masterAddress + ":" + std::to_string(m_hubPort)
                    + " for layer " + m_layerId + "\n");

    rtc::WebSocketConfiguration wsConfig;
    // Fail fast when the master is unreachable instead of hanging in Connecting.
    wsConfig.connectionTimeout = std::chrono::seconds(5);

    auto ws = std::make_shared<rtc::WebSocket>(wsConfig);
    m_ws.store(ws);

    const std::string url = "ws://" + m_masterAddress + ":" + std::to_string(m_hubPort) + "/cplay";

    // The callbacks capture plain copies of the objects (and the session flag), so stop() can
    // close and reset everything from the main thread without racing them.
    ws->onOpen([this, session, ws] {
        if (!*session) {
            return;
        }
        QJsonObject hello;
        hello.insert(QStringLiteral("type"), QStringLiteral("hello"));
        hello.insert(QStringLiteral("layerId"), QString::fromStdString(m_layerId));
        ws->send(signalingMessage(hello));
    });

    ws->onMessage([this, session](rtc::message_variant message) {
        if (!*session) {
            return;
        }
        if (const auto *text = std::get_if<rtc::string>(&message)) {
            onSignalingMessage(*text);
        }
    });

    ws->onClosed([this, session] {
        if (*session) {
            fail(u"The signaling connection to the hub was closed"_s);
        }
    });

    ws->onError([this, session](std::string error) {
        if (!*session) {
            return;
        }
        sgct::Log::Warning("WebRTC relay: signaling error: " + error + "\n");
        fail(QString::fromStdString(error));
    });

    try {
        ws->open(url);
    } catch (const std::exception &error) {
        fail(u"Could not open the hub WebSocket: %1"_s.arg(QString::fromUtf8(error.what())));
        return;
    }

    // If the hub does not send its metadata frame in time, give up and let the layer retry.
    QTimer::singleShot(kMetadataTimeoutMs, this, [this, session] {
        if (*session && m_state.load(std::memory_order_relaxed) != WebRtcStreamState::Running) {
            fail(u"Timed out waiting for the hub metadata"_s);
        }
    });
}

void WebRtcRelayClient::stop() {
    auto session = m_session.exchange(nullptr);
    if (session) {
        *session = false; // no new callback work from here on
    }

    auto pc = m_pc.exchange(nullptr);
    auto ws = m_ws.exchange(nullptr);
    // Declared after pc/ws, so at scope exit the channel is destroyed first while the peer
    // connection (and its SCTP transport) are still alive for a clean closeStream().
    auto dc = m_dc.exchange(nullptr);
    if (!pc && !ws && !dc) {
        return; // nothing was running
    }

    setState(WebRtcStreamState::Stopping);

    if (pc) {
        try {
            pc->close();
        } catch (const std::exception &error) {
            sgct::Log::Warning(std::string("WebRTC relay: error while closing the peer connection: ")
                               + error.what() + "\n");
        }
    }
    if (ws && !ws->isClosed()) {
        try {
            ws->forceClose();
        } catch (const std::exception &error) {
            sgct::Log::Warning(std::string("WebRTC relay: error while closing the WebSocket: ")
                               + error.what() + "\n");
        }
    }

    setState(WebRtcStreamState::Idle);
}

void WebRtcRelayClient::beginIce() {
    // "ready" may arrive more than once; only the first one starts ICE for this session.
    if (m_iceStarted.exchange(true, std::memory_order_relaxed)) {
        return;
    }

    rtc::Configuration config;
    // Share one UDP socket across this node's peer connections (harmless with a single layer,
    // required to mirror the master side).
    config.enableIceUdpMux = true;
    config.maxMessageSize = kWebRtcRelayMaxMessageSize;
    // With auto-negotiation enabled, createDataChannel() below would generate the offer itself,
    // before our onLocalDescription handler is installed (the callback fires on a worker thread),
    // so the hub never receives it. Negotiate explicitly instead: we call setLocalDescription()
    // ourselves once every handler is in place.
    config.disableAutoNegotiation = true;

    std::shared_ptr<rtc::PeerConnection> pc;
    try {
        pc = std::make_shared<rtc::PeerConnection>(config);
    } catch (const std::exception &error) {
        fail(u"Could not create the peer connection: %1"_s.arg(QString::fromUtf8(error.what())));
        return;
    }
    m_pc.store(pc);

    auto session = m_session.load();
    auto ws = m_ws.load();

    // The offerer must create its data channel before generating the SDP offer, so that SCTP
    // gets a negotiated m-section (libdatachannel throws "No DataChannel or Track to negotiate"
    // otherwise). The default Reliability{} is reliable + ordered, which the framing in
    // webrtctypes.h relies on. The master receives the channel through its onDataChannel()
    // callback when it applies our offer.
    auto dc = pc->createDataChannel(kWebRtcRelayDataChannelLabel);
    // libdatachannel only holds a weak reference to this channel; storing it in m_dc keeps it
    // alive until stop(). Without that, the channel would die at the end of beginIce() and the
    // SCTP layer would silently never assign/open it (no data channel open on either side).
    m_dc.store(dc);
    dc->onMessage([this, session](rtc::message_variant message) {
        if (!session || !*session) {
            return;
        }
        if (const auto *data = std::get_if<rtc::binary>(&message)) {
            onDataChannelMessage(*data);
        }
    });

    pc->onLocalDescription([session, ws](const rtc::Description &description) {
        if (!session || !*session || !ws || !ws->isOpen()) {
            return;
        }
        QJsonObject offer;
        offer.insert(QStringLiteral("type"), QStringLiteral("offer"));
        offer.insert(QStringLiteral("sdp"), QString::fromStdString(std::string(description)));
        ws->send(signalingMessage(offer));
    });

    pc->onLocalCandidate([session, ws](const rtc::Candidate &candidate) {
        if (!session || !*session || !ws || !ws->isOpen()) {
            return;
        }
        QJsonObject candidateMessage;
        candidateMessage.insert(QStringLiteral("type"), QStringLiteral("candidate"));
        candidateMessage.insert(QStringLiteral("candidate"), QString::fromStdString(candidate.candidate()));
        ws->send(signalingMessage(candidateMessage));
    });

    pc->onStateChange([this, session](rtc::PeerConnection::State state) {
        if (!session || !*session) {
            return;
        }
        switch (state) {
        case rtc::PeerConnection::State::Failed:
            fail(u"The relay peer connection failed"_s);
            break;
        case rtc::PeerConnection::State::Closed:
            // stop() closes the peer itself; only report it when the master went away.
            if (*session) {
                fail(u"The relay peer connection was closed by the master"_s);
            }
            break;
        default:
            break;
        }
    });

    try {
        pc->setLocalDescription(rtc::Description::Type::Offer);
    } catch (const std::exception &error) {
        fail(u"Could not create the SDP offer: %1"_s.arg(QString::fromUtf8(error.what())));
    }
}

void WebRtcRelayClient::onSignalingMessage(const std::string &text) {
    QJsonParseError parseError{};
    const QJsonObject object =
        QJsonDocument::fromJson(QString::fromStdString(text).toUtf8(), &parseError).object();
    if (parseError.error != QJsonParseError::NoError) {
        return;
    }

    const QString type = object.value(QStringLiteral("type")).toString();
    auto pc = m_pc.load();

    if (type == QStringLiteral("ready")) {
        beginIce();
    } else if (type == QStringLiteral("answer")) {
        if (!pc) {
            return;
        }
        try {
            pc->setRemoteDescription(
                rtc::Description(object.value(QStringLiteral("sdp")).toString().toStdString(),
                                 rtc::Description::Type::Answer));
        } catch (const std::exception &error) {
            fail(u"Could not apply the SDP answer: %1"_s.arg(QString::fromUtf8(error.what())));
        }
    } else if (type == QStringLiteral("candidate")) {
        if (!pc) {
            return;
        }
        try {
            pc->addRemoteCandidate(rtc::Candidate(object.value(QStringLiteral("candidate")).toString().toStdString()));
        } catch (const std::exception &error) {
            sgct::Log::Warning(std::string("WebRTC relay: ignoring a malformed candidate: ") + error.what() + "\n");
        }
    } else if (type == QStringLiteral("error")) {
        fail(u"The hub rejected the connection: %1"_s.arg(object.value(QStringLiteral("message")).toString()));
    }
}

void WebRtcRelayClient::onDataChannelMessage(const rtc::binary &message) {
    if (message.empty()) {
        return;
    }
    const auto *bytes = reinterpret_cast<const std::uint8_t *>(message.data());

    switch (static_cast<WebRtcRelayFrameType>(bytes[0])) {
    case kWebRtcRelayMetadata: {
        if (message.size() < 2) {
            break;
        }
        const auto codec = static_cast<WebRtcVideoCodec>(bytes[1]);
        if (codec == WebRtcVideoCodec::Unknown) {
            fail(u"The hub has not negotiated a video codec yet"_s);
            return;
        }
        sgct::Log::Info(std::string("WebRTC relay: ") + (codec == WebRtcVideoCodec::H265 ? "H.265" : "H.264")
                        + " stream ready\n");
        Q_EMIT videoCodecNegotiated(codec);
        setState(WebRtcStreamState::Running);
        break;
    }
    case kWebRtcRelayVideo: {
        // [type][u64 BE timestamp][Annex-B access unit]
        if (message.size() < 9) {
            break;
        }
        const std::uint64_t timestamp = readBigEndian64(bytes + 1);
        if (m_onVideo && message.size() > 9) {
            m_onVideo(bytes + 9, message.size() - 9, static_cast<quint32>(timestamp));
        }
        break;
    }
    case kWebRtcRelayAudio: {
        // [type][u64 BE timestamp][depacketized Opus payload]
        if (message.size() < 9) {
            break;
        }
        const std::uint64_t timestamp = readBigEndian64(bytes + 1);
        Q_EMIT audioFrameReceived(
            QByteArray(reinterpret_cast<const char *>(bytes + 9), static_cast<int>(message.size() - 9)),
            static_cast<quint32>(timestamp));
        break;
    }
    default:
        break; // unknown frame type (newer master); skip it
    }
}

void WebRtcRelayClient::requestKeyframe() {
    auto ws = m_ws.load();
    if (!ws || !ws->isOpen()) {
        return;
    }
    QJsonObject message;
    message.insert(QStringLiteral("type"), QStringLiteral("keyframe"));
    ws->send(signalingMessage(message));
}

