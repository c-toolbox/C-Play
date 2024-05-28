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

    width: 1000
    height: 700
    title: qsTr("C-Play Settings")
    visible: false
    //pageStack.initialPage: "qrc:/Navigation.qml"

    Component.onCompleted: pageStack.push("qrc:/Navigation.qml")

    Loader {
        source: "qrc:/PlaybackSettings.qml"
        asynchronous: true
    }

    Loader {
        source: "qrc:/GridSettings.qml"
        asynchronous: true
    }
	
	Loader {
        source: "qrc:/ImageSettings.qml"
        asynchronous: true
    }

    Window {
        id: helpWindow

        width: 900
        height: 700
        title: qsTr("Help")
        color: Kirigami.Theme.backgroundColor
        onVisibleChanged: info.text = app.getFileContent(applicationWindow().pageStack.currentItem.helpFile)

        Flickable {
            id: scrollView

            property int scrollStepSize: 100

            anchors.fill: parent
            contentHeight: info.height

            ScrollBar.vertical: ScrollBar {
                id: scrollbar
                policy: ScrollBar.AlwaysOn
                stepSize: scrollView.scrollStepSize/scrollView.contentHeight
            }

            MouseArea {
                anchors.fill: parent
                onWheel: {
                    if (wheel.angleDelta.y > 0) {
                        scrollbar.decrease()
                    } else {
                        scrollbar.increase()
                    }
                }
            }

            TextArea {
                id: info

                background: Rectangle {
                    color: "transparent"
                    border.color: "transparent"
                }
                width: parent.width
                color: Kirigami.Theme.textColor
                readOnly: true
                textFormat: Text.RichText
                wrapMode: Text.WordWrap
                selectByMouse: true
                rightPadding: scrollbar.width
                onLinkActivated: Qt.openUrlExternally(link)
                onHoveredLinkChanged: hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
            }
        }
    }
}
