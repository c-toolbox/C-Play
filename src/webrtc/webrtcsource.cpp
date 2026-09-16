/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "webrtc/webrtcsource.h"

#include "webrtc/opusaudiodepacketizer.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QRegularExpression>
#include <QStringList>

#include <rtc/rtc.hpp>
#include <sgct/log.h>

using namespace Qt::Literals::StringLiterals;

namespace {

constexpr int kVideoPayloadTypeH264 = 106;
constexpr int kVideoPayloadTypeH265 = 103;
constexpr int kAudioPayloadTypeOpus = 111;

/// Routes libdatachannel's own ICE/DTLS diagnostics into the C-Play log. Without it a
/// handshake that dies after the SDP exchange fails completely silently.
void ensureWebRtcLogger()
{
    static const bool initialized = [] {
        rtc::InitLogger(rtc::LogLevel::Warning, [](rtc::LogLevel level, std::string message) {
            const std::string line = "libdatachannel: " + message + "\n";
            if (level <= rtc::LogLevel::Error) {
                sgct::Log::Error(line);
            } else if (level == rtc::LogLevel::Warning) {
                sgct::Log::Warning(line);
            } else {
                sgct::Log::Info(line);
            }
        });
        return true;
    }();
    (void)initialized;
}

const char *stateName(WebRtcStreamState state)
{
    switch (state) {
    case WebRtcStreamState::Idle: return "Idle";
    case WebRtcStreamState::Connecting: return "Connecting";
    case WebRtcStreamState::Running: return "Running";
    case WebRtcStreamState::Failed: return "Failed";
    case WebRtcStreamState::Stopping: return "Stopping";
    }
    return "Unknown";
}

/// The state and codec enums travel through queued signal/slot connections, so
/// they must be known to the meta type system.
struct WebRtcMetaTypes {
    WebRtcMetaTypes()
    {
        qRegisterMetaType<WebRtcStreamState>("WebRtcStreamState");
        qRegisterMetaType<WebRtcVideoCodec>("WebRtcVideoCodec");
    }
};
const WebRtcMetaTypes webRtcMetaTypes;

/// MediaMTX answers with exactly one video codec; find out which so the right
/// depacketizer can be installed before media starts flowing.
WebRtcVideoCodec videoCodecFromAnswer(const QString &sdp)
{
    static const QRegularExpression rtpMap(
        uR"(^a=rtpmap:(\d+)\s+([A-Za-z0-9]+)/(\d+))"_s,
        QRegularExpression::MultilineOption);

    auto it = rtpMap.globalMatch(sdp);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const QString encoding = match.captured(2).toUpper();
        if (encoding == u"H264"_s) {
            return WebRtcVideoCodec::H264;
        }
        if (encoding == u"H265"_s || encoding == u"HEVC"_s) {
            return WebRtcVideoCodec::H265;
        }
    }
    return WebRtcVideoCodec::Unknown;
}

/// Audio is optional in a WHEP stream: servers without it answer with no m=audio line
/// (or one on port 0). Only then do we wire up the audio track.
bool hasActiveAudioInAnswer(const QString &sdp)
{
    static const QRegularExpression audioLine(
        uR"(^m=audio\s+(\d+))"_s,
        QRegularExpression::MultilineOption);

    auto it = audioLine.globalMatch(sdp);
    while (it.hasNext()) {
        if (it.next().captured(1).toInt() != 0) {
            return true;
        }
    }
    return false;
}

} // namespace

WebRtcSource::WebRtcSource(QObject *parent)
    : QObject(parent)
{
}

WebRtcSource::~WebRtcSource()
{
    stop();
}

void WebRtcSource::setConfig(const WebRtcStreamConfig &config)
{
    m_config = config;
}

void WebRtcSource::setVideoCallback(MediaFrameCallback callback)
{
    m_onVideo = std::move(callback);
}

void WebRtcSource::setRawAudioPacketCallback(RawAudioPacketCallback callback)
{
    m_onRawAudioPacket = std::move(callback);
}

WebRtcStreamState WebRtcSource::state() const
{
    return m_state.load(std::memory_order_relaxed);
}

WebRtcVideoCodec WebRtcSource::negotiatedVideoCodec() const
{
    return m_videoCodec.load(std::memory_order_relaxed);
}

void WebRtcSource::setState(WebRtcStreamState state)
{
    const WebRtcStreamState previous = m_state.exchange(state, std::memory_order_relaxed);
    if (previous != state) {
        sgct::Log::Info(std::string("WebRTC: state ") + stateName(previous) + " -> "
                        + stateName(state) + "\n");
        Q_EMIT stateChanged(state);
    }
}

void WebRtcSource::start()
{
    stop();
    ensureWebRtcLogger();

    m_offerSent = false;
    m_videoCodec.store(WebRtcVideoCodec::Unknown, std::memory_order_relaxed);
    setState(WebRtcStreamState::Connecting);

    sgct::Log::Info("WebRTC: starting WHEP session against "
                    + m_config.whepUrl.toString(QUrl::RemoveUserInfo).toStdString() + "\n");

    m_whep = new WhepClient(this);
    m_whep->setEndpoint(m_config.whepUrl);
    // setCredentials() is a no-op for an empty username, so credentials embedded in
    // the URL (handled by setEndpoint) are kept unless explicitly overridden.
    m_whep->setCredentials(m_config.username, m_config.password);

    connect(m_whep, &WhepClient::iceServersReady, this, &WebRtcSource::beginNegotiation);
    connect(m_whep, &WhepClient::answerReceived, this, &WebRtcSource::onAnswer);
    connect(m_whep, &WhepClient::failed, this, [this](const QString &reason) {
        Q_EMIT errorOccurred(reason);
        setState(WebRtcStreamState::Failed);
    });

    m_whep->requestIceServers();
}

void WebRtcSource::beginNegotiation(const QList<IceServerSpec> &iceServers)
{
    rtc::Configuration configuration;
    for (const IceServerSpec &server : iceServers) {
        try {
            rtc::IceServer entry(server.url.toStdString());
            if (!server.username.isEmpty()) {
                entry.username = server.username.toStdString();
                entry.password = server.credential.toStdString();
            }
            configuration.iceServers.push_back(std::move(entry));
        } catch (const std::exception &error) {
            sgct::Log::Warning("WebRTC: ignoring unusable ICE server "
                               + server.url.toStdString() + ": " + error.what() + "\n");
        }
    }

    sgct::Log::Info("WebRTC: negotiating with "
                    + std::to_string(configuration.iceServers.size())
                    + " advertised ICE server(s)\n");

    configuration.disableAutoNegotiation = true;

    try {
        m_peer = std::make_shared<rtc::PeerConnection>(configuration);
    } catch (const std::exception &error) {
        Q_EMIT errorOccurred(u"Could not create the peer connection: %1"_s
                                 .arg(QString::fromUtf8(error.what())));
        setState(WebRtcStreamState::Failed);
        return;
    }

    m_peer->onStateChange([this](rtc::PeerConnection::State state) {
        switch (state) {
        case rtc::PeerConnection::State::Connected:
            setState(WebRtcStreamState::Running);
            break;
        case rtc::PeerConnection::State::Disconnected:
        case rtc::PeerConnection::State::Failed:
            setState(WebRtcStreamState::Failed);
            break;
        case rtc::PeerConnection::State::Closed:
            setState(WebRtcStreamState::Idle);
            break;
        default:
            break;
        }
    });

    m_peer->onGatheringStateChange([this](rtc::PeerConnection::GatheringState state) {
        if (state == rtc::PeerConnection::GatheringState::Complete) {
            QMetaObject::invokeMethod(this, [this] { onLocalDescriptionReady(); },
                                      Qt::QueuedConnection);
        }
    });

    rtc::Description::Video video("video", rtc::Description::Direction::RecvOnly);
    for (const WebRtcVideoCodec codec : m_config.preferredCodecs) {
        switch (codec) {
        case WebRtcVideoCodec::H264:
            video.addH264Codec(kVideoPayloadTypeH264);
            break;
        case WebRtcVideoCodec::H265:
            video.addH265Codec(kVideoPayloadTypeH265);
            break;
        case WebRtcVideoCodec::Unknown:
            break;
        }
    }
    m_videoTrack = m_peer->addTrack(video);

    // Offer a recvonly Opus audio track as well. Streams without audio simply omit it
    // in their answer, so this keeps video-only WHEP streams working unchanged.
    rtc::Description::Audio audio("audio", rtc::Description::Direction::RecvOnly);
    audio.addOpusCodec(kAudioPayloadTypeOpus);
    m_audioTrack = m_peer->addTrack(audio);

    try {
        m_peer->setLocalDescription(rtc::Description::Type::Offer);
    } catch (const std::exception &error) {
        Q_EMIT errorOccurred(u"Could not create the SDP offer: %1"_s
                                 .arg(QString::fromUtf8(error.what())));
        setState(WebRtcStreamState::Failed);
    }
}

void WebRtcSource::onLocalDescriptionReady()
{
    if (m_offerSent || !m_peer) {
        return;
    }
    const auto description = m_peer->localDescription();
    if (!description) {
        return;
    }

    m_offerSent = true;
    sgct::Log::Info("WebRTC: ICE gathering complete, posting the SDP offer\n");
    m_whep->sendOffer(QString::fromStdString(std::string(description.value())));
}

void WebRtcSource::onAnswer(const QString &sdpAnswer)
{
    if (!m_peer) {
        return;
    }

    const WebRtcVideoCodec codec = videoCodecFromAnswer(sdpAnswer);
    m_videoCodec.store(codec, std::memory_order_relaxed);
    if (codec == WebRtcVideoCodec::Unknown) {
        Q_EMIT errorOccurred(u"The WHEP answer contains no supported video codec"_s);
        setState(WebRtcStreamState::Failed);
        return;
    }
    sgct::Log::Info(std::string("WebRTC: negotiated video codec ")
                    + (codec == WebRtcVideoCodec::H265 ? "H.265" : "H.264") + "\n");
    Q_EMIT videoCodecNegotiated(codec);

    // Handlers must be in place before setRemoteDescription starts the media flow.
    if (!m_videoTrack) {
        Q_EMIT errorOccurred(u"The peer connection has no video track to attach a depacketizer to"_s);
        setState(WebRtcStreamState::Failed);
        return;
    }

    try {
        std::shared_ptr<rtc::MediaHandler> depacketizer;
        if (codec == WebRtcVideoCodec::H265) {
            depacketizer = std::make_shared<rtc::H265RtpDepacketizer>(
                rtc::NalUnit::Separator::LongStartSequence);
        } else {
            depacketizer = std::make_shared<rtc::H264RtpDepacketizer>(
                rtc::NalUnit::Separator::LongStartSequence);
        }

        m_videoTrack->setMediaHandler(depacketizer);
        m_videoTrack->chainMediaHandler(std::make_shared<rtc::RtcpReceivingSession>());
    } catch (const std::exception &error) {
        Q_EMIT errorOccurred(u"Could not install the video depacketizer: %1"_s
                                 .arg(QString::fromUtf8(error.what())));
        setState(WebRtcStreamState::Failed);
        return;
    }

    m_videoTrack->onFrame([this](rtc::binary data, rtc::FrameInfo info) {
        if (m_onVideo && !data.empty()) {
            m_onVideo(reinterpret_cast<const std::uint8_t *>(data.data()), data.size(),
                      info.timestamp);
        }
    });

    m_videoTrack->onOpen([this] { requestKeyframe(); });

    // Audio is optional: wire it up only if the answer carries an active m=audio line.
    // A failure here must not kill the video path, so errors are logged and swallowed.
    if (m_audioTrack) {
        if (!hasActiveAudioInAnswer(sdpAnswer)) {
            sgct::Log::Info("WebRTC: stream has no audio track (video-only)\n");
        } else {
            try {
                // libdatachannel's generic RtpDepacketizer does not strip RFC 3550 padding (its H264/H265
                // siblings do), so a padded sender would leak the pad bytes into the Opus payload and make
                // two-CBR-frame packets structurally invalid. Use a subclass that removes them first.
                auto opusDepacketizer = std::make_shared<OpusAudioDepacketizer>();
                opusDepacketizer->setRawPacketCallback(m_onRawAudioPacket);
                m_audioTrack->setMediaHandler(opusDepacketizer);
                m_audioTrack->chainMediaHandler(std::make_shared<rtc::RtcpReceivingSession>());
                m_audioTrack->onFrame([this](rtc::binary data, rtc::FrameInfo info) {
                    if (!data.empty()) {
                        Q_EMIT audioFrameReceived(
                            QByteArray(reinterpret_cast<const char *>(data.data()),
                                       static_cast<int>(data.size())),
                            info.timestamp);
                    }
                });
                sgct::Log::Info("WebRTC: stream carries an Opus audio track\n");
            } catch (const std::exception &error) {
                sgct::Log::Warning(std::string("WebRTC: could not install the audio depacketizer, "
                                               "continuing video-only: ")
                                   + error.what() + "\n");
            }
        }
    }

    try {
        m_peer->setRemoteDescription(
            rtc::Description(sdpAnswer.toStdString(), rtc::Description::Type::Answer));
    } catch (const std::exception &error) {
        Q_EMIT errorOccurred(u"Could not apply the SDP answer: %1"_s
                                 .arg(QString::fromUtf8(error.what())));
        setState(WebRtcStreamState::Failed);
    }
}

void WebRtcSource::requestKeyframe()
{
    if (m_videoTrack && m_videoTrack->isOpen()) {
        m_videoTrack->requestKeyframe();
    }
}

void WebRtcSource::teardownPeer()
{
    if (m_videoTrack) {
        m_videoTrack->onFrame(nullptr);
        m_videoTrack.reset();
    }
    if (m_audioTrack) {
        m_audioTrack->onFrame(nullptr);
        m_audioTrack.reset();
    }
    if (m_peer) {
        try {
            m_peer->close();
        } catch (const std::exception &error) {
            sgct::Log::Warning(std::string("WebRTC: error while closing the peer connection: ")
                               + error.what() + "\n");
        }
        m_peer.reset();
    }
}

void WebRtcSource::stop()
{
    if (!m_whep && !m_peer) {
        return;
    }

    setState(WebRtcStreamState::Stopping);

    if (m_whep) {
        m_whep->deleteSession();
        m_whep->deleteLater();
        m_whep = nullptr;
    }

    teardownPeer();
    setState(WebRtcStreamState::Idle);
}
