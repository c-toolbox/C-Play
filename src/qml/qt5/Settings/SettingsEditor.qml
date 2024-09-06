/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.10
import QtQuick.Window 2.1
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import QtGraphicalEffects 1.15

import org.kde.kirigami 2.15 as Kirigami
import org.ctoolbox.cplay 1.0 as Haruna

Kirigami.ApplicationWindow {
    id: root

    height: 700
    title: qsTr("C-Play Settings")
    visible: false
    width: 1000

    //pageStack.initialPage: "qrc:/Navigation.qml"

    Component.onCompleted: pageStack.push("qrc:/Navigation.qml")

    Loader {
        asynchronous: true
        source: "qrc:/PlaybackSettings.qml"
    }
    Loader {
        asynchronous: true
        source: "qrc:/GridSettings.qml"
    }
    Loader {
        asynchronous: true
        source: "qrc:/ImageSettings.qml"
    }
    Window {
        id: helpWindow

        color: Kirigami.Theme.backgroundColor
        height: 700
        title: qsTr("Help")
        width: 900

        onVisibleChanged: info.text = app.getFileContent(applicationWindow().pageStack.currentItem.helpFile)

        Flickable {
            id: scrollView

            property int scrollStepSize: 100

            anchors.fill: parent
            contentHeight: info.height

            ScrollBar.vertical: ScrollBar {
                id: scrollbar

                policy: ScrollBar.AlwaysOn
                stepSize: scrollView.scrollStepSize / scrollView.contentHeight
            }

            MouseArea {
                anchors.fill: parent

                onWheel: {
                    if (wheel.angleDelta.y > 0) {
                        scrollbar.decrease();
                    } else {
                        scrollbar.increase();
                    }
                }
            }
            TextArea {
                id: info

                color: Kirigami.Theme.textColor
                readOnly: true
                rightPadding: scrollbar.width
                selectByMouse: true
                textFormat: Text.RichText
                width: parent.width
                wrapMode: Text.WordWrap

                background: Rectangle {
                    border.color: "transparent"
                    color: "transparent"
                }

                onHoveredLinkChanged: hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                onLinkActivated: Qt.openUrlExternally(link)
            }
        }
    }
}
