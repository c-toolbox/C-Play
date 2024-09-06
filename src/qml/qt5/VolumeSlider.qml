/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Shapes 1.15
import QtGraphicalEffects 1.15

import org.kde.kirigami 2.15 as Kirigami
import org.ctoolbox.cplay 1.0

Slider {
    id: root

    property string overlayLabel: qsTr("Media volume")

    from: 0
    implicitHeight: 25
    implicitWidth: 130
    leftPadding: 0
    rightPadding: 0
    to: 100
    value: mpv.volume
    wheelEnabled: false

    background: Rectangle {
        id: harunaSliderBG

        color: Kirigami.Theme.alternateBackgroundColor

        Rectangle {
            color: Kirigami.Theme.highlightColor
            height: parent.height
            radius: 0
            width: visualPosition * parent.width
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
        text: root.overlayLabel + qsTr(": %1").arg(Number(root.value.toFixed(0)))

        layer.effect: DropShadow {
            color: "#111"
            radius: 5
            samples: 17
            spread: 0.3
            verticalOffset: 1
        }
    }
}
