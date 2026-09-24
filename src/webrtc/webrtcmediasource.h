/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "webrtc/webrtctypes.h"

#include <QObject>

#include <cstddef>
#include <cstdint>
#include <functional>

/// Common interface for the two ways a WebRTCLayer can obtain elementary media:
///  - master (and standalone runs): WebRtcSource pulls directly from the WHEP endpoint.
///  - node: WebRtcRelayClient receives the frames relayed by the master's WebRtcHub over
///    a unique, reliable + ordered data channel instead of pulling WHEP itself.
///
/// Media is delivered on libdatachannel's own threads through a plain std::function
/// callback rather than Qt signals: the hot path must not queue through the GUI event
/// loop. State changes do use signals and are safe to bind to the UI.
class WebRtcMediaSource : public QObject {
    Q_OBJECT

public:
    /// data points at an Annex-B access unit; it is only valid for the duration of the
    /// call, so consumers must copy what they keep.
    using MediaFrameCallback =
        std::function<void(const std::uint8_t *data, std::size_t size, quint32 rtpTimestamp)>;

    explicit WebRtcMediaSource(QObject *parent = nullptr);
    ~WebRtcMediaSource() override;

    /// Must be set before start(); the callback may fire from a libdatachannel thread.
    void setVideoCallback(MediaFrameCallback callback);

    virtual void start() = 0;
    virtual void stop() = 0;

    /// Asks the sender for an IDR. MediaMTX does not send one on connect, so this is called
    /// when a consumer reports it is starved. For WebRtcSource that is a PLI to the WHEP
    /// endpoint; for WebRtcRelayClient it is a request routed through the hub to the master.
    virtual void requestKeyframe() = 0;

Q_SIGNALS:
    void stateChanged(WebRtcStreamState state);
    void videoCodecNegotiated(WebRtcVideoCodec codec);
    void errorOccurred(const QString &message);

    // Emitted on every depacketized Opus payload received from the stream (raw codec
    // bytes, not PCM). rtpTimestamp is the RTP timestamp in 48 kHz units.
    void audioFrameReceived(const QByteArray &payload, quint32 rtpTimestamp);

protected:
    MediaFrameCallback m_onVideo;
};

/// Routes libdatachannel's own ICE/DTLS diagnostics into the C-Play log. Without it a
/// handshake that dies after the SDP exchange fails completely silently. Idempotent.
void webRtcEnsureLibDataChannelLogger();
