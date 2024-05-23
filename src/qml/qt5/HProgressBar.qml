/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQml.Models 2.15
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Shapes 1.15

import org.kde.kirigami 2.11 as Kirigami
import org.ctoolbox.cplay 1.0

Slider {
    id: root

    property alias sectionIndicator: sectionIndicator
    property bool seekStarted: false
    property bool videoWasPaused: false

    from: 0
    to: mpv.duration
    implicitWidth: 200
    implicitHeight: 25
    leftPadding: 0
    rightPadding: 0

    handle: Item { visible: false }

    background: Rectangle {
        id: progressBarBackground
        color: Kirigami.Theme.alternateBackgroundColor

        Rectangle {
            id: sectionIndicator
            property double startPosition: -1
            property double endPosition: -1
            width: endPosition === -1 ? 1 : (endPosition / mpv.duration * progressBarBackground.width) - x
            height: parent.height
            color: Qt.hsla(0, 0, 0, 0.4)
            visible: startPosition !== -1
            x: startPosition / mpv.duration * progressBarBackground.width
            z: 110
        }

        Rectangle {
            width: visualPosition * parent.width
            height: parent.height
            color: Kirigami.Theme.highlightColor
        }

        ToolTip {
            id: progressBarToolTip

            visible: progressBarMouseArea.containsMouse
            timeout: -1
            delay: 0
        }

        MouseArea {
            id: progressBarMouseArea

            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.MiddleButton | Qt.RightButton

            onClicked: {
            }

            onMouseXChanged: {
                progressBarToolTip.x = mouseX - (progressBarToolTip.width * 0.5)

                const time = mouseX * 100 / progressBarBackground.width * root.to / 100
                progressBarToolTip.text = app.formatTime(time)
            }

            onEntered: {
                progressBarToolTip.x = mouseX - (progressBarToolTip.width * 0.5)
                progressBarToolTip.y = root.height
            }
        }
    }

    onToChanged: value = mpv.position
    onPressedChanged: {
        if (pressed) {
            //videoWasPaused = mpv.pause
            //mpv.pause = true
            seekStarted = true
        } else {
            mpv.pause = true
            mpv.position = value
            //mpv.pause = videoWasPaused
            seekStarted = false
        }
    }

    Connections {
        target: mpv
        function onPositionChanged() {
            if (!root.seekStarted) {
                root.value = mpv.position
            }
        }
        function onFileLoaded() {
            sectionIndicator.startPosition = -1
            sectionIndicator.endPosition = -1
        }
        function onRewind() {
            sectionIndicator.startPosition = -1
            sectionIndicator.endPosition = -1
        }
        function onSectionLoaded(sectionIdx) {
            sectionIndicator.startPosition = mpv.playSectionsModel.sectionStartTime(sectionIdx)
            sectionIndicator.endPosition = mpv.playSectionsModel.sectionEndTime(sectionIdx)
        }
    }
}
