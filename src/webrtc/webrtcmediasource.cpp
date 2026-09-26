/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "webrtc/webrtcmediasource.h"

#include <rtc/rtc.hpp>
#include <sgct/log.h>

WebRtcMediaSource::WebRtcMediaSource(QObject *parent)
    : QObject(parent)
{
}

WebRtcMediaSource::~WebRtcMediaSource() = default;

void WebRtcMediaSource::setVideoCallback(VideoFrameCallback callback)
{
    m_onVideo.store(callback ? std::make_shared<const VideoFrameCallback>(std::move(callback)) : nullptr);
}

void WebRtcMediaSource::setAudioCallback(AudioFrameCallback callback)
{
    m_onAudio.store(callback ? std::make_shared<const AudioFrameCallback>(std::move(callback)) : nullptr);
}

void WebRtcMediaSource::deliverVideo(const WebRtcVideoFrame &frame) const
{
    // Hold a reference for the duration of the call, so a concurrent setVideoCallback()
    // cannot destroy the callback while it runs.
    if (const auto callback = m_onVideo.load()) {
        (*callback)(frame);
    }
}

void WebRtcMediaSource::deliverAudio(const std::uint8_t *data, std::size_t size, std::int64_t pts) const
{
    if (const auto callback = m_onAudio.load()) {
        (*callback)(data, size, pts);
    }
}

void webRtcEnsureRelayTransportSettings()
{
    static const bool initialized = [] {
        rtc::SctpSettings settings;
        // Room for a few high-bitrate keyframes in flight. The send buffer is kept moderate on
        // purpose: data inside usrsctp is invisible to bufferedAmount(), which the hub's
        // backlog control relies on.
        settings.recvBufferSize = 4 * 1024 * 1024;
        settings.sendBufferSize = 2 * 1024 * 1024;
        // A keyframe should not have to wait for slow start after an idle period.
        settings.initialCongestionWindow = 32;
        // LAN round trips are sub-millisecond: acknowledge quickly and retransmit a lost
        // chunk after tens of milliseconds instead of the WAN defaults (20 ms SACK delay,
        // 200 ms minimum / 1 s initial / 10 s maximum RTO).
        settings.delayedSackTime = std::chrono::milliseconds(5);
        settings.minRetransmitTimeout = std::chrono::milliseconds(50);
        settings.initialRetransmitTimeout = std::chrono::milliseconds(200);
        settings.maxRetransmitTimeout = std::chrono::milliseconds(1000);
        rtc::SetSctpSettings(std::move(settings));
        return true;
    }();
    (void)initialized;
}

void webRtcEnsureLibDataChannelLogger()
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

/// The state and codec enums travel through queued signal/slot connections, so they must
/// be known to the meta type system. Registered once for every WebRtcMediaSource subclass.
struct WebRtcMetaTypes {
    WebRtcMetaTypes()
    {
        qRegisterMetaType<WebRtcStreamState>("WebRtcStreamState");
        qRegisterMetaType<WebRtcVideoCodec>("WebRtcVideoCodec");
    }
};
const WebRtcMetaTypes webRtcMetaTypes;
