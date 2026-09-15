/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "webrtc/webrtctypes.h"
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
/// event loop. State changes do use signals and are safe to bind to the UI.
class WebRtcSource : public QObject
{
    Q_OBJECT

public:
    /// data points at an Annex-B access unit; it is only valid for the duration of
    /// the call, so consumers must copy what they keep.
    using MediaFrameCallback =
        std::function<void(const std::uint8_t *data, std::size_t size, quint32 rtpTimestamp)>;

    explicit WebRtcSource(QObject *parent = nullptr);
    ~WebRtcSource() override;

    void setConfig(const WebRtcStreamConfig &config);

    void setVideoCallback(MediaFrameCallback callback);

    void start();
    void stop();

    WebRtcStreamState state() const;
    WebRtcVideoCodec negotiatedVideoCodec() const;

    /// Asks the sender for an IDR. MediaMTX does not send one on connect, so this
    /// is called on track open and whenever a consumer reports it is starved.
    void requestKeyframe();

Q_SIGNALS:
    void stateChanged(WebRtcStreamState state);
    void videoCodecNegotiated(WebRtcVideoCodec codec);
    void errorOccurred(const QString &message);

    // Emitted on every depacketized Opus payload received from the stream (raw codec
    // bytes, not PCM). rtpTimestamp is the RTP timestamp in 48 kHz units.
    void audioFrameReceived(const QByteArray &payload, quint32 rtpTimestamp);

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

    MediaFrameCallback m_onVideo;

    std::atomic<WebRtcStreamState> m_state { WebRtcStreamState::Idle };
    std::atomic<WebRtcVideoCodec> m_videoCodec { WebRtcVideoCodec::Unknown };
    bool m_offerSent = false;
};
