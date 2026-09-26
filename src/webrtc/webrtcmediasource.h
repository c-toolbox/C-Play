/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "webrtc/webrtctypes.h"

#include <QObject>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

/// One video access unit handed out by a WebRtcMediaSource.
struct WebRtcVideoFrame {
    const std::uint8_t *data = nullptr; ///< Annex-B access unit, valid only during the call
    std::size_t size = 0;
    std::int64_t pts = 0;       ///< unwrapped 90 kHz RTP timestamp
    bool keyframe = false;      ///< random access point (IDR/IRAP or parameter sets)
    bool discontinuity = false; ///< frames were dropped before this one (relay backlog)
};

/// Common interface for the two ways a WebRTCLayer can obtain elementary media:
///  - master (and standalone runs): WebRtcSource pulls directly from the WHEP endpoint.
///  - node: WebRtcRelayClient receives the frames relayed by the master's WebRtcHub over
///    a unique, reliable + ordered data channel instead of pulling WHEP itself.
///
/// Media is delivered on libdatachannel's own threads through plain std::function
/// callbacks rather than Qt signals: the hot path must not queue through the GUI event
/// loop. State changes do use signals and are safe to bind to the UI.
class WebRtcMediaSource : public QObject {
    Q_OBJECT

public:
    using VideoFrameCallback = std::function<void(const WebRtcVideoFrame &frame)>;
    /// data points at a depacketized Opus payload, valid only for the duration of the call;
    /// pts is the unwrapped 48 kHz RTP timestamp.
    using AudioFrameCallback =
        std::function<void(const std::uint8_t *data, std::size_t size, std::int64_t pts)>;

    explicit WebRtcMediaSource(QObject *parent = nullptr);
    ~WebRtcMediaSource() override;

    /// Safe to call from any thread at any time, also while media flows; a callback that is
    /// already running keeps its own reference and finishes normally.
    void setVideoCallback(VideoFrameCallback callback);
    void setAudioCallback(AudioFrameCallback callback);

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

protected:
    void deliverVideo(const WebRtcVideoFrame &frame) const;
    void deliverAudio(const std::uint8_t *data, std::size_t size, std::int64_t pts) const;

private:
    std::atomic<std::shared_ptr<const VideoFrameCallback>> m_onVideo{nullptr};
    std::atomic<std::shared_ptr<const AudioFrameCallback>> m_onAudio{nullptr};
};

/// Routes libdatachannel's own ICE/DTLS diagnostics into the C-Play log. Without it a
/// handshake that dies after the SDP exchange fails completely silently. Idempotent.
void webRtcEnsureLibDataChannelLogger();

/// Applies LAN-tuned SCTP settings (larger buffers and initial window, short delayed-SACK
/// and retransmission timeouts) for the relay data channels. The settings are process-wide
/// in libdatachannel and must be in place before the first relay PeerConnection is created,
/// so both the hub and the relay client call this first. Idempotent.
void webRtcEnsureRelayTransportSettings();
