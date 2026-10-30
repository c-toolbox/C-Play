/*
 * SPDX-FileCopyrightText:
 * 2021-2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import Qt5Compat.GraphicalEffects

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

Slider {
    id: root

    property string overlayLabel: qsTr("Media visibility: ")
    property real maximumValue: 100
    property bool maximumLocked: false

    from: 0
    implicitHeight: 25
    implicitWidth: 130
    leftPadding: 0
    // Keep the capped part of the 0–100% track visible, but outside the input range.
    rightPadding: width * (1 - maximumValue / 100)
    to: maximumValue
    wheelEnabled: false

    background: Rectangle {
        id: visibilitySliderBG

        width: root.width
        color: Kirigami.Theme.alternateBackgroundColor

        Rectangle {
            color: Kirigami.Theme.highlightColor
            height: parent.height
            radius: 0
            width: root.visualPosition * parent.width * root.maximumValue / 100
        }
        Rectangle {
            x: parent.width * root.maximumValue / 100
            width: parent.width - x
            height: parent.height
            visible: root.maximumLocked && root.maximumValue < 100
            color: Qt.alpha(Kirigami.Theme.textColor, 0.18)
            border.color: Qt.alpha(Kirigami.Theme.textColor, 0.35)
            border.width: 1
        }
    }
    handle: Item {
        visible: false
    }

    Label {
        id: progressBarToolTip

        anchors.centerIn: root
        color: "#fff"
        font.pointSize: 9
        layer.enabled: true
        text: root.maximumLocked
              ? root.overlayLabel + qsTr("%1% (max %2%)").arg(Math.round(root.value)).arg(Math.round(root.maximumValue))
              : root.overlayLabel + qsTr("%1%").arg(Math.round(root.value))

        layer.effect: DropShadow {
            color: "#111"
            radius: 5
            samples: 17
            spread: 0.3
            verticalOffset: 1
        }
    }
}
