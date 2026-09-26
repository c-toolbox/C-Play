/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QString>
#include <QUrl>

#include <cstddef>
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
/// The media itself does not use this port: each node gets its own PeerConnection with its
/// own UDP socket, bound inside [kWebRtcRelayPortRangeBegin, kWebRtcRelayPortRangeEnd].
constexpr std::uint16_t kWebRtcHubPort = 8890;

/// UDP port range of the relay peer connections (master hub and node clients). One socket
/// per peer connection, so every node has its own kernel send buffer and a slow node cannot
/// make the others drop datagrams. Open this range in the firewall between the machines.
constexpr std::uint16_t kWebRtcRelayPortRangeBegin = 8891;
constexpr std::uint16_t kWebRtcRelayPortRangeEnd = 8990;

/// Path MTU of the relay peer connections. The cluster runs on a LAN, so the conservative
/// libdatachannel default (1280) only costs packets; 1400 still leaves room for VPN/VLAN tags.
constexpr std::size_t kWebRtcRelayMtu = 1400;

/// Version of the hub <-> node relay protocol, sent in the node's hello message. The hub
/// rejects nodes speaking another version, since the frame layouts are not compatible.
constexpr int kWebRtcRelayProtocolVersion = 2;

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

    /// Payload: [1 byte flags (WebRtcRelayVideoFlags)][8 byte big-endian unwrapped 90 kHz
    /// timestamp][Annex-B access unit].
    kWebRtcRelayVideo = 0x02,

    /// Payload: [8 byte big-endian unwrapped 48 kHz timestamp][depacketized Opus payload].
    kWebRtcRelayAudio = 0x03,

    /// No payload. The hub dropped frames for this node (it fell too far behind); the next
    /// video frame is a keyframe and the decoder must resynchronise on it.
    kWebRtcRelayDiscontinuity = 0x04,
};

enum WebRtcRelayVideoFlags : std::uint8_t {
    kWebRtcRelayVideoKeyframe = 0x01, ///< the access unit is a random access point
};

/// Maximum data channel message size on both sides of the relay. libdatachannel's default
/// is only 256 KiB, which a single high-bitrate video access unit can exceed; SCTP then
/// fragments larger messages transparently (reliable + ordered), so raising it is safe.
constexpr std::size_t kWebRtcRelayMaxMessageSize = 16 * 1024 * 1024;

/// Per-node backlog limits of the hub. When a node's unsent data exceeds either limit, the
/// hub drops its queue, sends kWebRtcRelayDiscontinuity and resumes at the next keyframe, so
/// a slow node recovers instead of accumulating latency (and memory) forever.
constexpr std::size_t kWebRtcRelayMaxNodeBacklogBytes = 4 * 1024 * 1024;
constexpr int kWebRtcRelayMaxNodeBacklogMs = 250;

/// RTP clock rates of the relayed media.
constexpr std::int64_t kWebRtcVideoClockRate = 90000;
constexpr std::int64_t kWebRtcAudioClockRate = 48000;

/// Default playout delay of synchronised (relayed) WebRTC layers: every machine shows the
/// frame whose timestamp is this far behind the master's arrival clock, which gives the
/// relay hop and the node decoders time to catch up so all screens change frame together.
constexpr int kWebRtcDefaultSyncDelayMs = 50;
constexpr int kWebRtcMinSyncDelayMs = 0;
constexpr int kWebRtcMaxSyncDelayMs = 1000;

/// Extends 32-bit RTP timestamps to a monotonic 64-bit timeline (wrap-around safe as long
/// as consecutive timestamps are less than 2^31 ticks apart). Not thread-safe.
class RtpTimestampUnwrapper {
public:
    std::int64_t unwrap(std::uint32_t timestamp) {
        if (!m_started) {
            m_started = true;
            m_last = timestamp;
            m_value = timestamp;
            return m_value;
        }
        m_value += static_cast<std::int32_t>(timestamp - m_last);
        m_last = timestamp;
        return m_value;
    }

    void reset() { m_started = false; }

private:
    bool m_started = false;
    std::uint32_t m_last = 0;
    std::int64_t m_value = 0;
};

/// True when an Annex-B access unit carries parameter sets or starts a new GOP, i.e. when a
/// decoder can be handed data from this point on without missing references or SPS/PPS.
inline bool webRtcIsRandomAccessPoint(const std::uint8_t *data, std::size_t size, WebRtcVideoCodec codec) {
    for (std::size_t i = 0; i + 3 < size; ++i) {
        if (data[i] != 0 || data[i + 1] != 0) {
            continue;
        }

        std::size_t header = 0;
        if (data[i + 2] == 1) {
            header = i + 3;
        } else if (data[i + 2] == 0 && data[i + 3] == 1) {
            header = i + 4;
        } else {
            continue;
        }
        if (header >= size) {
            break;
        }

        if (codec == WebRtcVideoCodec::H265) {
            const int type = (data[header] >> 1) & 0x3F;
            // VPS/SPS/PPS, or any IRAP picture (BLA/IDR/CRA).
            if (type == 32 || type == 33 || type == 34 || (type >= 16 && type <= 21)) {
                return true;
            }
        } else {
            const int type = data[header] & 0x1F;
            if (type == 5 || type == 7 || type == 8) { // IDR, SPS, PPS
                return true;
            }
        }
        i = header;
    }
    return false;
}

