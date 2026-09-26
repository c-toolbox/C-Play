/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>

/// Maps the sender's 90 kHz media timeline onto the master's steady clock, so the master
/// can tell, for any wall-clock moment, which frame timestamp should be on screen.
///
/// Every received frame contributes offset = arrival - pts (both in 90 kHz ticks). The
/// minimum over a sliding window is the offset of the least delayed frame, i.e. network
/// jitter is filtered out while slow clock drift between sender and master is followed.
/// target(now) = now - offset - delay is then broadcast to the nodes each SGCT frame, and
/// every machine (the master included) shows the newest decoded frame with pts <= target.
///
/// Thread-safe: frames are reported from a libdatachannel thread, targets are read from
/// the SGCT encode and render threads.
class WebRtcPlayoutClock {
public:
    using Clock = std::chrono::steady_clock;

    void reset();

    /// Reports the arrival of a video frame with the unwrapped 90 kHz timestamp \p pts.
    void onFrame(std::int64_t pts, Clock::time_point arrival);

    /// The newest frame timestamp that should be displayed at \p now, \p delayMs behind the
    /// live edge. Never decreases (except after a timeline reset). False until frames arrive.
    bool target(Clock::time_point now, int delayMs, std::int64_t &targetPts);

private:
    static std::int64_t toTicks(Clock::time_point time);

    struct Sample {
        std::int64_t arrivalTicks;
        std::int64_t offset;
    };

    std::mutex m_mutex;
    std::deque<Sample> m_window; // monotonic: offsets increase front to back, front = minimum
    std::int64_t m_lastPts = 0;
    bool m_hasFrames = false;
    std::int64_t m_lastTarget = 0;
    bool m_hasTarget = false;
};
