/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQml.Models
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

Slider {
    id: root

    property alias sectionIndicator: sectionIndicator
    property bool seekStarted: false

    from: 0
    implicitHeight: 25
    implicitWidth: 200
    leftPadding: 0
    rightPadding: 0
    to: 0

    background: Rectangle {
        id: progressBarBackground

        color: Kirigami.Theme.alternateBackgroundColor

        Rectangle {
            id: sectionIndicator

            property double endPosition: -1
            property double startPosition: -1

            color: Qt.hsla(0, 0, 0, 0.4)
            height: parent.height
            visible: startPosition !== -1
            width: endPosition === -1 ? 1 : ((endPosition - root.from) / (root.to - root.from) * progressBarBackground.width) - x
            x: (startPosition - root.from) / (root.to - root.from) * progressBarBackground.width
            z: 110
        }
        Rectangle {
            color: Kirigami.Theme.highlightColor
            height: parent.height
            width: root.visualPosition * parent.width
        }
        ToolTip {
            id: progressBarToolTip

            delay: 0
            timeout: -1
            visible: progressBarMouseArea.containsMouse
        }
        MouseArea {
            id: progressBarMouseArea

            acceptedButtons: Qt.MiddleButton | Qt.RightButton
            anchors.fill: parent
            hoverEnabled: true

            onClicked: {}
            onEntered: {
                progressBarToolTip.x = mouseX - (progressBarToolTip.width * 0.5);
                progressBarToolTip.y = root.height;
            }
            onMouseXChanged: {
                progressBarToolTip.x = mouseX - (progressBarToolTip.width * 0.5);
                const time = root.from + mouseX * (root.to - root.from) / progressBarBackground.width;
                progressBarToolTip.text = app.formatTime(time);
            }
        }
    }
    handle: Item {
        visible: false
    }
}
