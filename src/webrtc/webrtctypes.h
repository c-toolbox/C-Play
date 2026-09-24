/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QString>
#include <QUrl>

#include <cstdint>
#include <vector>

/// Video codec negotiated with the WHEP endpoint.
enum class WebRtcVideoCodec {
    Unknown,
    H264,
    H265,
};

/// High level state of one WHEP pull session.
enum class WebRtcStreamState {
    Idle,
    Connecting,
    Running,
    Failed,
    Stopping,
};

/// Everything needed to pull one WHEP stream. The URL is stored in the layer's
/// filepath property; credentials may be embedded in it (user:pass@host).
struct WebRtcStreamConfig {
    QUrl whepUrl;
    QString username;
    QString password;

    // Order matters: the first entry is offered with the highest priority.
    std::vector<WebRtcVideoCodec> preferredCodecs = { WebRtcVideoCodec::H264, WebRtcVideoCodec::H265 };
};

/// Fixed port of the master-side relay hub's WebSocket signaling server (see WebRtcHub).
/// The media itself does not use this port: each node gets its own PeerConnection whose
/// UDP traffic is multiplexed by libjuice onto a single ephemeral master socket.
constexpr std::uint16_t kWebRtcHubPort = 8890;

/// Label of the reliable + ordered data channel that carries the relayed media from the
/// master hub to one node. The channel is created by the node once its PeerConnection is
/// connected; the master receives it through onDataChannel().
constexpr const char *kWebRtcRelayDataChannelLabel = "cplay-media";

/// Frame types on the relay data channel. Each data channel message is exactly one frame:
/// [1 byte type][payload]. The channel is reliable and ordered, so SCTP guarantees delivery
/// and ordering; no sequence numbers are needed.
enum WebRtcRelayFrameType : std::uint8_t {
    /// Payload: 1 byte, the negotiated video codec as a WebRtcVideoCodec value. Always the
    /// first frame on the channel; the node starts decoding only after receiving it.
    kWebRtcRelayMetadata = 0x01,

    /// Payload: [8 byte big-endian RTP timestamp][Annex-B access unit].
    kWebRtcRelayVideo = 0x02,

    /// Payload: [8 byte big-endian RTP timestamp][depacketized Opus payload].
    kWebRtcRelayAudio = 0x03,
};

/// Maximum data channel message size on both sides of the relay. libdatachannel's default
/// is only 256 KiB, which a single high-bitrate video access unit can exceed; SCTP then
/// fragments larger messages transparently (reliable + ordered), so raising it is safe.
constexpr std::size_t kWebRtcRelayMaxMessageSize = 16 * 1024 * 1024;

