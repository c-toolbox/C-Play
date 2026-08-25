/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef FRAMESYNCCONTROLLER_H
#define FRAMESYNCCONTROLLER_H

#include <vector>
#include <cstddef>

// Ports the decision logic from the "mpv-udp-framesync" Lua script (mpv-sync.lua)
// into C++ as a reusable, dependency-free helper.
//
// The Lua script synchronized independent mpv instances over UDP broadcast.
// In C-Play, position/state is already transported between the cluster master
// and its slave nodes via the SGCT SyncHelper mechanism. What we adopt here is
// only the *decision logic*: when a slave receives the master's position, it
// decides whether to adjust playback speed progressively, reset to normal
// speed, or perform a hard seek, based on the time difference.
//
// This class is deliberately free of Qt/sgct dependencies so it can be used
// from both MpvObject (Qt/QML thread) and MpvLayer (SGCT render thread) and
// be unit-tested in isolation.
class FrameSyncController {
public:
    // What action should be taken by the slave player for the current tick.
    enum class Action {
        None,        // In sync: reset to base speed, no correction needed.
        SpeedAdjust, // Progressively adjust playback speed to catch up / slow down.
        HardSeek     // Difference too large: seek to target position.
    };

    // Result of one decision step.
    struct Decision {
        Action action = Action::None;
        double speed = 1.0;      // Target speed (only valid when action == SpeedAdjust/None).
        double seekTarget = 0.0; // Absolute seek target in seconds (only when action == HardSeek).
    };

    FrameSyncController() = default;

    // All tunables mirror the Lua script's --script-opts options.
    void setSeekThreshold(double v) { m_seekThreshold = v; }
    void setSpeedAdjustThreshold(double v) { m_speedAdjustThreshold = v; }
    void setMaxSpeedAdjust(double v) { m_maxSpeedAdjust = v; }
    void setInitialOffset(double v) { m_initialOffset = v; }

    // Configure all tunables in one call.
    void configure(double seekThreshold, double speedAdjustThreshold,
                   double maxSpeedAdjust, double initialOffset) {
        m_seekThreshold = seekThreshold;
        m_speedAdjustThreshold = speedAdjustThreshold;
        m_maxSpeedAdjust = maxSpeedAdjust;
        m_initialOffset = initialOffset;
    }

    double seekThreshold() const { return m_seekThreshold; }
    double speedAdjustThreshold() const { return m_speedAdjustThreshold; }
    double maxSpeedAdjust() const { return m_maxSpeedAdjust; }
    double initialOffset() const { return m_initialOffset; }

    // Compute the correction decision for a slave at `slavePos` whose target is
    // the master position `masterPos` (+ manual initialOffset). `baseSpeed` is the
    // unmodified playback speed set by the master.
    Decision decide(double masterPos, double slavePos, double baseSpeed);

private:
    double m_seekThreshold = 5.0;          // seconds diff before hard seek
    double m_speedAdjustThreshold = 0.02;  // below this = "in sync"
    double m_maxSpeedAdjust = 0.5;         // max speed change (fraction)
    double m_initialOffset = 0.015;        // manual offset in seconds
};

#endif // FRAMESYNCCONTROLLER_H
