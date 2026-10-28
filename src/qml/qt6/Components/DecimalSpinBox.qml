/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Controls

import org.ctoolbox.cplay

import "DecimalSpinBoxHelpers.js" as DecimalHelpers

// Spin box for decimal values, built on DoubleSpinBox (Qt >= 6.11).
// Components/Legacy/DecimalSpinBox.qml provides the same interface for older Qt;
// CMake picks one of them. Bind realValue and write back in onRealValueModified.
DoubleSpinBox {
    id: control

    property real realFrom: 0
    property real realTo: 100
    property real realStepSize: 1
    property real realValue: 0
    property string suffix: ""

    signal realValueModified(real newValue)

    decimals: 2
    from: realFrom
    to: realTo
    stepSize: realStepSize
    // DoubleSpinBox passes (value, decimals, locale), SpinBoxTextField's width measurement (value, locale)
    textFromValue: function (value, decimals, locale) {
        return DecimalHelpers.format(value, control.decimals, control.suffix, locale === undefined ? decimals : locale);
    }
    valueFromText: function (text, locale) {
        const parsed = DecimalHelpers.parse(text, control.suffix);
        return isNaN(parsed) ? control.value : parsed;
    }

    validator: RegularExpressionValidator {
        regularExpression: DecimalHelpers.inputPattern(control.decimals, control.realFrom < 0, control.suffix)
    }

    contentItem: SpinBoxTextField {
        spinBox: control
    }

    onRealValueChanged: value = realValue
    Component.onCompleted: value = realValue
    onValueModified: realValueModified(value)
}
