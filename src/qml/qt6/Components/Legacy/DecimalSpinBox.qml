/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Controls

import org.ctoolbox.cplay

import "../DecimalSpinBoxHelpers.js" as DecimalHelpers

// Spin box for decimal values on Qt < 6.11, which has no DoubleSpinBox. Same interface
// as Components/DecimalSpinBox.qml; stores the value times 10^decimals in SpinBox's int value.
DeferredSpinBox {
    id: control

    property int decimals: 2
    property real realFrom: 0
    property real realTo: 100
    property real realStepSize: 1
    property real realValue: 0
    property string suffix: ""
    readonly property real factor: Math.pow(10, decimals)

    signal realValueModified(real newValue)

    from: Math.round(realFrom * factor)
    to: Math.round(realTo * factor)
    stepSize: Math.max(1, Math.round(realStepSize * factor))
    textFromValue: function (value, locale) {
        return DecimalHelpers.format(value / control.factor, control.decimals, control.suffix, locale);
    }
    valueFromText: function (text, locale) {
        const parsed = DecimalHelpers.parse(text, control.suffix);
        return isNaN(parsed) ? control.value : Math.round(parsed * control.factor);
    }

    validator: RegularExpressionValidator {
        regularExpression: DecimalHelpers.inputPattern(control.decimals, control.realFrom < 0, control.suffix)
    }

    onRealValueChanged: value = Math.round(realValue * factor)
    Component.onCompleted: value = Math.round(realValue * factor)
    onValueModified: realValueModified(value / factor)
}
