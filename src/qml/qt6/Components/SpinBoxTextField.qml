/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Templates as T

import org.kde.kirigami as Kirigami

// contentItem for SpinBox/DoubleSpinBox that only applies typed text on Enter or when
// focus leaves the field. The org.kde.desktop style commits on every keystroke
// (onTextEdited), which applies intermediate values and reformats partial decimal
// input (e.g. "1" -> "1.00").
T.TextField {
    id: field

    required property var spinBox

    readonly property TextMetrics _textMetrics: TextMetrics {
        text: field.spinBox.textFromValue(field.spinBox.to, field.spinBox.locale)
        font: field.spinBox.font
    }

    implicitWidth: Math.max(_textMetrics.width + 2, Math.round(contentWidth)) + leftPadding + rightPadding
    implicitHeight: Math.round(contentHeight) + topPadding + bottomPadding
    z: 2
    font: spinBox.font
    palette: spinBox.palette
    text: spinBox.displayText
    color: Kirigami.Theme.textColor
    selectionColor: Kirigami.Theme.highlightColor
    selectedTextColor: Kirigami.Theme.highlightedTextColor
    selectByMouse: true
    hoverEnabled: false
    verticalAlignment: Qt.AlignVCenter
    readOnly: !spinBox.editable
    validator: spinBox.validator
    inputMethodHints: spinBox.inputMethodHints

    onEditingFinished: {
        if (text.length > 0 && acceptableInput) {
            const oldValue = spinBox.value;
            spinBox.value = spinBox.valueFromText(text, spinBox.locale);
            if (spinBox.value !== oldValue)
                spinBox.valueModified();
        }
        // Show the formatted value again (also reverts incomplete input)
        text = Qt.binding(() => field.spinBox.displayText);
    }

    Accessible.name: spinBox.Accessible.name
    Accessible.description: spinBox.Accessible.description
}
