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

import org.kde.kirigami 2.15 as Kirigami
import org.ctoolbox.cplay 1.0

Slider {
    id: root

    property alias sectionIndicator: sectionIndicator
    property bool seekStarted: false
    property bool videoWasPaused: false

    from: 0
    implicitHeight: 25
    implicitWidth: 200
    leftPadding: 0
    rightPadding: 0
    to: mpv.duration

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
            width: endPosition === -1 ? 1 : (endPosition / mpv.duration * progressBarBackground.width) - x
            x: startPosition / mpv.duration * progressBarBackground.width
            z: 110
        }
        Rectangle {
            color: Kirigami.Theme.highlightColor
            height: parent.height
            width: visualPosition * parent.width
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
                const time = mouseX * 100 / progressBarBackground.width * root.to / 100;
                progressBarToolTip.text = app.formatTime(time);
            }
        }
    }
    handle: Item {
        visible: false
    }

    onPressedChanged: {
        if (pressed) {
            //videoWasPaused = mpv.pause
            //mpv.pause = true
            seekStarted = true;
        } else {
            mpv.pause = true;
            mpv.position = value;
            //mpv.pause = videoWasPaused
            seekStarted = false;
        }
    }
    onToChanged: value = mpv.position

    Connections {
        function onFileLoaded() {
            sectionIndicator.startPosition = -1;
            sectionIndicator.endPosition = -1;
        }
        function onPositionChanged() {
            if (!root.seekStarted) {
                root.value = mpv.position;
            }
        }
        function onRewind() {
            sectionIndicator.startPosition = -1;
            sectionIndicator.endPosition = -1;
        }
        function onSectionLoaded(sectionIdx) {
            sectionIndicator.startPosition = mpv.playSectionsModel.sectionStartTime(sectionIdx);
            sectionIndicator.endPosition = mpv.playSectionsModel.sectionEndTime(sectionIdx);
        }

        target: mpv
    }
}
