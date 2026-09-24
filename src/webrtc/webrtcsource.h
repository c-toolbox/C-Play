/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "webrtc/webrtcmediasource.h"
#include "webrtc/whepclient.h"

#include <QObject>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

namespace rtc {
class PeerConnection;
class Track;
}

/// Pulls one WHEP stream and hands out elementary media.
///
/// Media is delivered on libdatachannel's own threads through a plain std::function
/// callback rather than Qt signals: the hot path must not queue through the GUI
/// event loop. State changes do use signals and are safe to bind to the UI (see
/// WebRtcMediaSource for the interface shared with the node-side relay client).
class WebRtcSource : public WebRtcMediaSource
{
    Q_OBJECT

public:
    explicit WebRtcSource(QObject *parent = nullptr);
    ~WebRtcSource() override;

    void setConfig(const WebRtcStreamConfig &config);

    /// Diagnostic hook (used for live wire-level debugging): receives each raw audio RTP
    /// datagram, header included, before depacketization and padding stripping, so behaviour
    /// such as RFC 3550 padding can be verified. Not called when unset.
    using RawAudioPacketCallback = std::function<void(const std::uint8_t *data, std::size_t size)>;
    void setRawAudioPacketCallback(RawAudioPacketCallback callback);

    void start() override;
    void stop() override;

    WebRtcStreamState state() const;
    WebRtcVideoCodec negotiatedVideoCodec() const;

    /// Asks the sender for an IDR. MediaMTX does not send one on connect, so this
    /// is called on track open and whenever a consumer reports it is starved.
    void requestKeyframe() override;

private:
    void beginNegotiation(const QList<IceServerSpec> &iceServers);
    void onLocalDescriptionReady();
    void onAnswer(const QString &sdpAnswer);
    void setState(WebRtcStreamState state);
    void teardownPeer();

    WebRtcStreamConfig m_config;

    WhepClient *m_whep = nullptr;
    std::shared_ptr<rtc::PeerConnection> m_peer;
    std::shared_ptr<rtc::Track> m_videoTrack;
    std::shared_ptr<rtc::Track> m_audioTrack; // recvonly Opus track, active only if the stream has audio

    RawAudioPacketCallback m_onRawAudioPacket;

    std::atomic<WebRtcStreamState> m_state { WebRtcStreamState::Idle };
    std::atomic<WebRtcVideoCodec> m_videoCodec { WebRtcVideoCodec::Unknown };
    bool m_offerSent = false;
};
