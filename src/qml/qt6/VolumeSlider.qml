/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
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

    property string overlayLabel: qsTr("Master volume")

    from: 0
    implicitHeight: 25
    implicitWidth: 130
    leftPadding: 0
    rightPadding: 0
    to: 100
    wheelEnabled: false

    background: Rectangle {
        id: harunaSliderBG

        color: Kirigami.Theme.alternateBackgroundColor

        Rectangle {
            color: Kirigami.Theme.highlightColor
            height: parent.height
            radius: 0
            width: root.visualPosition * parent.width
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
        text: root.overlayLabel + qsTr(": %1\%").arg(Number(root.value.toFixed(0)))

        layer.effect: DropShadow {
            color: "#111"
            radius: 5
            samples: 17
            spread: 0.3
            verticalOffset: 1
        }
    }
}
