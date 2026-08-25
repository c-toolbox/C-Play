/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "framesynccontroller.h"

#include <cmath>
#include <algorithm>

FrameSyncController::Decision FrameSyncController::decide(double masterPos, double slavePos, double baseSpeed) {
    Decision decision;

    // The target position the slave should be at: master position + manual offset.
    const double targetPos = masterPos + m_initialOffset;
    const double timeDiff = targetPos - slavePos; // positive = slave is behind, negative = ahead

    // Large difference: hard seek to the target position.
    if (std::abs(timeDiff) > m_seekThreshold) {
        decision.action = Action::HardSeek;
        decision.seekTarget = targetPos;
        return decision;
    }

    // Close enough: return to base speed (in sync).
    if (std::abs(timeDiff) < m_speedAdjustThreshold) {
        decision.action = Action::None;
        decision.speed = baseSpeed;
        return decision;
    }

    // Progressive speed adjustment without oscillation (mirrors the Lua table):
    //   < 50ms   : ultra-fine,  max 5%
    //   < 200ms  : fine,        max 10%
    //   < 1s     : moderate,    max 32%
    //   otherwise: larger,      capped by maxSpeedAdjust
    const double absDiff = std::abs(timeDiff);
    double speedFactor;
    if (absDiff < 0.05) {
        speedFactor = absDiff * 1.0;
    } else if (absDiff < 0.2) {
        speedFactor = 0.05 + (absDiff - 0.05) * 0.5;
    } else if (absDiff < 1.0) {
        speedFactor = 0.125 + (absDiff - 0.2) * 0.25;
    } else {
        speedFactor = std::min(absDiff / 3.0, m_maxSpeedAdjust);
    }

    speedFactor = std::min(speedFactor, m_maxSpeedAdjust);

    double newSpeed;
    if (timeDiff > 0) {
        // Slave is behind: speed up.
        newSpeed = baseSpeed + speedFactor;
    } else {
        // Slave is ahead: slow down.
        newSpeed = baseSpeed - speedFactor;
    }

    // Clamp to reasonable values.
    newSpeed = std::max(0.5, std::min(2.0, newSpeed));

    decision.action = Action::SpeedAdjust;
    decision.speed = newSpeed;
    return decision;
}
