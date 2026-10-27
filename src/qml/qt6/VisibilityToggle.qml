/*
 * SPDX-FileCopyrightText:
 * 2021-2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
import Qt5Compat.GraphicalEffects

import org.kde.kirigami as Kirigami

Item {
    id: root

    property real value: 0
    property bool canFadeIn: value < 100
    property bool canFadeOut: value > 0
    // 1 while the value rises, -1 while it falls, 0 once it has settled.
    property int trend: 0
    property real lastValue: 0

    readonly property string stateText: trend > 0 ? qsTr("Fading in")
                                      : trend < 0 ? qsTr("Fading out")
                                      : value > 0 ? qsTr("On") : qsTr("Off")
    readonly property color stateColor: trend !== 0 ? "#8b8000" : "#006400"

    signal fadeInRequested
    signal fadeOutRequested
    signal toggleRequested

    implicitHeight: 25
    implicitWidth: 80

    onValueChanged: {
        if (value <= 0 || value >= 100) {
            trend = 0;
        } else if (value !== lastValue) {
            trend = value > lastValue ? 1 : -1;
            settleTimer.restart();
        }
        lastValue = value;
    }
    Component.onCompleted: {
        lastValue = value;
        trend = 0;
    }

    Timer {
        id: settleTimer

        interval: 500
        onTriggered: root.trend = 0
    }

    ToolButton {
        id: fadeOutButton

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.top: parent.top
        enabled: root.canFadeOut
        icon.height: 14
        icon.name: "go-previous"
        icon.width: 14
        padding: 0
        width: 18
        z: 1

        ToolTip.text: qsTr("Fade out")
        ToolTip.visible: hovered

        onClicked: root.fadeOutRequested()
    }
    Rectangle {
        id: curve

        anchors.fill: parent
        color: Kirigami.Theme.alternateBackgroundColor

        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                fillColor: Qt.alpha(Kirigami.Theme.textColor, 0.12)
                startX: 0
                startY: curve.height
                strokeColor: "transparent"

                PathLine { x: curve.width; y: 0 }
                PathLine { x: curve.width; y: curve.height }
                PathLine { x: 0; y: curve.height }
            }
        }
        Item {
            clip: true
            height: parent.height
            width: parent.width * Math.max(0, Math.min(100, root.value)) / 100

            Shape {
                height: curve.height
                preferredRendererType: Shape.CurveRenderer
                width: curve.width

                ShapePath {
                    fillColor: root.stateColor
                    startX: 0
                    startY: curve.height
                    strokeColor: "transparent"

                    PathLine { x: curve.width; y: 0 }
                    PathLine { x: curve.width; y: curve.height }
                    PathLine { x: 0; y: curve.height }
                }
            }
        }
        Label {
            anchors.centerIn: parent
            color: "#fff"
            font.pointSize: 8
            layer.enabled: true
            text: root.stateText

            layer.effect: DropShadow {
                color: "#111"
                radius: 5
                samples: 17
                spread: 0.3
                verticalOffset: 1
            }
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
        }
    }
    ToolButton {
        id: fadeInButton

        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.top: parent.top
        enabled: root.canFadeIn
        icon.height: 14
        icon.name: "go-next"
        icon.width: 14
        padding: 0
        width: 18
        z: 1

        ToolTip.text: qsTr("Fade in")
        ToolTip.visible: hovered

        onClicked: root.fadeInRequested()
    }
}
