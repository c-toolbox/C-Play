/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "webrtc/webrtcplayoutclock.h"

#include "webrtc/webrtctypes.h"

namespace {

/// Length of the minimum filter. Long enough to contain a jitter-free frame, short enough
/// to follow a lasting change of the network delay within a couple of seconds.
constexpr std::int64_t kWindowTicks = 2 * kWebRtcVideoClockRate;

/// A timestamp this far away from the previous one is a new timeline (sender restart or a
/// new WHEP session), not a gap in the same one.
constexpr std::int64_t kBackwardResetTicks = kWebRtcVideoClockRate;
constexpr std::int64_t kForwardResetTicks = 10 * kWebRtcVideoClockRate;

} // namespace

std::int64_t WebRtcPlayoutClock::toTicks(Clock::time_point time) {
    const auto us = std::chrono::duration_cast<std::chrono::microseconds>(time.time_since_epoch()).count();
    return us * kWebRtcVideoClockRate / 1000000;
}

void WebRtcPlayoutClock::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_window.clear();
    m_hasFrames = false;
    m_hasTarget = false;
}

void WebRtcPlayoutClock::onFrame(std::int64_t pts, Clock::time_point arrival) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_hasFrames && (pts < m_lastPts - kBackwardResetTicks || pts > m_lastPts + kForwardResetTicks)) {
        m_window.clear();
        m_hasTarget = false; // the target may jump backwards onto the new timeline
    }
    m_hasFrames = true;
    m_lastPts = pts;

    const std::int64_t now = toTicks(arrival);
    const std::int64_t offset = now - pts;

    // Sliding-window minimum: a newer sample with a smaller (or equal) offset makes every
    // older, larger one irrelevant.
    while (!m_window.empty() && m_window.back().offset >= offset) {
        m_window.pop_back();
    }
    m_window.push_back({now, offset});
    while (m_window.front().arrivalTicks < now - kWindowTicks) {
        m_window.pop_front();
    }
}

bool WebRtcPlayoutClock::target(Clock::time_point now, int delayMs, std::int64_t &targetPts) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_window.empty()) {
        return false;
    }

    std::int64_t value = toTicks(now) - m_window.front().offset
                         - static_cast<std::int64_t>(delayMs) * kWebRtcVideoClockRate / 1000;
    // When the fastest sample leaves the window the minimum rises and the raw target steps
    // back; holding it keeps the presentation from ever going backwards.
    if (m_hasTarget && value < m_lastTarget) {
        value = m_lastTarget;
    }
    m_lastTarget = value;
    m_hasTarget = true;
    targetPts = value;
    return true;
}
