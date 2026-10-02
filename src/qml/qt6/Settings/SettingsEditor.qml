/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay as Haruna

Kirigami.ApplicationWindow {
    id: root

    property string pagePath: "qrc:/qt/qml/org/ctoolbox/cplay/qml/qt6/Settings"

    height: 880
    title: qsTr("C-Play Preferences")
    visible: false
    width: 1080

    Component.onCompleted: pageStack.push(`${root.pagePath}/Navigation.qml`)
    onVisibleChanged: {
        if (visible) {
            // Center over the main application window. From this nested component "window" is
            // that outer window, not this one. The top edge is clamped so a dialog taller than
            // the main window never rises above its top corner.
            x = window.x + (window.width - width) / 2;
            y = Math.max(window.y, window.y + (window.height - height) / 2);
        }
    }

    Loader {
        asynchronous: true
        source: `${root.pagePath}/PlaybackSettings.qml`
    }
    Loader {
        asynchronous: true
        source: `${root.pagePath}/GridSettings.qml`
    }
    Loader {
        asynchronous: true
        source: `${root.pagePath}/ImageSettings.qml`
    }
    Window {
        id: helpWindow

        color: Kirigami.Theme.backgroundColor
        height: 740
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
