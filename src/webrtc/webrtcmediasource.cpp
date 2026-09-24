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

void WebRtcMediaSource::setVideoCallback(MediaFrameCallback callback)
{
    m_onVideo = std::move(callback);
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
