/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
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

    property string overlayLabel: qsTr("Media visibility")

    from: 0
    to: 100
    value: mpv.visibility
    implicitWidth: 130
    implicitHeight: 25
    wheelEnabled: false
    leftPadding: 0
    rightPadding: 0

    handle: Item { visible: false }

    background: Rectangle {
        id: harunaSliderBG
        color: Kirigami.Theme.alternateBackgroundColor

        Rectangle {
            width: visualPosition * parent.width
            height: parent.height
            color: Kirigami.Theme.highlightColor
            radius: 0
        }
    }

    Label {
        id: progressBarToolTip
        text: root.overlayLabel + qsTr(": %1\%").arg(Number(root.value.toFixed(0)))
        font.pointSize: 9
        anchors.centerIn: root
        color: "#fff"
        layer.enabled: true
        layer.effect: DropShadow { verticalOffset: 1; color: "#111"; radius: 5; spread: 0.3; samples: 17 }
    }

}
