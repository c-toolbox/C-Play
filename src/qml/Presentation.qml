/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtGraphicalEffects 1.12

import org.kde.kirigami 2.11 as Kirigami
import org.ctoolbox.cplay 1.0

Rectangle {
    id: presentationRoot

    property alias scrollPositionTimer: scrollPositionTimer
    property alias presentationView: presentationView
    property alias mediaTitle: mediaTitle
    property string position: PlaylistSettings.position
    property int rowHeight: PlaylistSettings.rowHeight
    property int bigFont: PlaylistSettings.bigFontFullscreen

    height: mpv.height
    width: {
            const w = Kirigami.Units.gridUnit * 19
            return (parent.width * 0.17) < w ? w : parent.width * 0.17
    }
    x: position !== "right" ? parent.width : -width
    y: 0
    state: "hidden"
    color: Kirigami.Theme.backgroundColor

    ColumnLayout {
        id: presentationHeader
        spacing: 10
    }

    ScrollView {
        id: presentationScrollView

        z: 20
        anchors.fill: parent
        anchors.topMargin: presentationHeader.height + 5
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    }

    Timer {
        id: scrollPositionTimer
        interval: 50; running: true; repeat: true

        onTriggered: {
            scrollPositionTimer.stop()
        }
    }

    states: [
        State {
            name: "hidden"
            PropertyChanges { target: presentationRoot; x: position !== "right" ? parent.width : -width }
            PropertyChanges { target: presentationRoot; visible: false }
        },
        State {
            name : "visible"
            PropertyChanges { target: presentationRoot; x: position !== "right" ? parent.width - presentationRoot.width : 0 }
            PropertyChanges { target: presentationRoot; visible: true }
        }
    ]

    transitions: [
        Transition {
            from: "visible"
            to: "hidden"

            SequentialAnimation {
                NumberAnimation {
                    target: presentationRoot
                    property: "x"
                    duration: 120
                    easing.type: Easing.InQuad
                }
                PropertyAction {
                    target: presentationRoot
                    property: "visible"
                    value: false
                }
            }
        },
        Transition {
            from: "hidden"
            to: "visible"

            SequentialAnimation {
                PropertyAction {
                    target: presentationRoot
                    property: "visible"
                    value: true
                }
                NumberAnimation {
                    target: presentationRoot
                    property: "x"
                    duration: 120
                    easing.type: Easing.OutQuad
                }
            }
        }
    ]

}
