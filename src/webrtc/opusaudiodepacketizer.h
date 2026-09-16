/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <rtc/rtpdepacketizer.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>

/// Opus RTP depacketizer that also strips RFC 3550 padding.
///
/// libdatachannel's generic RtpDepacketizer (and its rtc::OpusRtpDepacketizer alias) only removes
/// the fixed header, CSRC list and extension headers; when the sender sets the P bit - as MediaMTX
/// does for SRTP-protected streams - the trailing padding bytes are forwarded as part of the
/// payload. For two-CBR-frame Opus packets (minptime=10) an odd number of such bytes makes the whole
/// packet structurally invalid: opus_packet_parse_impl() rejects any odd length after the TOC byte,
/// so libopus reports "corrupted stream" and the frame is lost. This class strips the padding exactly
/// like libdatachannel's H264/H265 depacketizers do for video, then delegates to the base class.
class OpusAudioDepacketizer : public rtc::RtpDepacketizer {
public:
    /// Diagnostic hook: receives each raw audio RTP datagram (header included) before padding is
    /// stripped, so wire-level behaviour can be verified. Not called when unset.
    using RawPacketCallback = std::function<void(const std::uint8_t *data, std::size_t size)>;

    static constexpr std::uint32_t kClockRate = 48000; // Opus RTP clock (RFC 7587)

    OpusAudioDepacketizer() : rtc::RtpDepacketizer(kClockRate) {}

    void setRawPacketCallback(RawPacketCallback callback) { m_onRawPacket = std::move(callback); }

    void incoming(rtc::message_vector &messages, const rtc::message_callback &send) override;

private:
    RawPacketCallback m_onRawPacket;
};
