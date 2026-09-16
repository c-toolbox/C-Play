/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "webrtc/opusaudiodepacketizer.h"

#include <rtc/rtp.hpp>

void OpusAudioDepacketizer::incoming(rtc::message_vector &messages, const rtc::message_callback &send) {
    for (auto &message : messages) {
        if (message->type != rtc::Message::Binary || message->size() < sizeof(rtc::RtpHeader)) {
            continue; // Control messages and undersized packets are handled by the base class.
        }

        const auto *header = reinterpret_cast<const rtc::RtpHeader *>(message->data());
        if (m_onRawPacket) {
            m_onRawPacket(reinterpret_cast<const std::uint8_t *>(message->data()), message->size());
        }

        if (!header->padding()) {
            continue;
        }

        // RFC 3550: the last byte of a padded packet holds the total number of padding bytes,
        // including itself.
        const std::uint8_t padCount = static_cast<std::uint8_t>(message->back());
        const std::size_t headerSize = sizeof(rtc::RtpHeader) + 4 * header->csrcCount()
                                       + header->getExtensionHeaderSize();

        if (padCount == 0 || message->size() <= headerSize + padCount) {
            // Malformed padding: drop the packet rather than feed garbage to the decoder.
            message->clear();
            continue;
        }

        message->resize(message->size() - padCount);
    }

    rtc::RtpDepacketizer::incoming(messages, send);
}
