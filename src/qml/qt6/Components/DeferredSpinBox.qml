/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Controls

import org.ctoolbox.cplay

// SpinBox that only applies typed text on Enter or when focus leaves the field,
// see SpinBoxTextField. For decimal values use DecimalSpinBox.
SpinBox {
    id: control

    contentItem: SpinBoxTextField {
        spinBox: control
    }
}
