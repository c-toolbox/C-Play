/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import org.kde.kirigami 2.15 as Kirigami

Kirigami.Page {
    bottomPadding: 0
    leftPadding: 0
    padding: 0
    rightPadding: 0
    topPadding: 0

    footer: ToolBar {
        RowLayout {
            anchors.fill: parent

            ToolButton {
                Layout.fillWidth: true
                icon.name: "configure-shortcuts"
                text: qsTr("Configure shortcuts")

                onClicked: appActions.configureShortcutsAction.trigger()
            }
        }
    }

    Component.onCompleted: applicationWindow().pageStack.columnView.columnWidth = 250

    Loader {
        asynchronous: true
    }
    ListModel {
        id: settingsPagesModel

        ListElement {
            iconName: "audio-speakers-symbolic"
            name: "Audio"
            page: "qrc:/AudioSettings.qml"
        }
        ListElement {
            iconName: "kstars_hgrid"
            name: "Grid/mapping"
            page: "qrc:/GridSettings.qml"
        }
        ListElement {
            iconName: "kdenlive-select-images"
            name: "Image"
            page: "qrc:/ImageSettings.qml"
        }
        ListElement {
            iconName: "find-location"
            name: "Location"
            page: "qrc:/LocationSettings.qml"
        }
        ListElement {
            iconName: "input-mouse"
            name: "Mouse"
            page: "qrc:/MouseSettings.qml"
        }
        ListElement {
            iconName: "video-x-generic"
            name: "Playback"
            page: "qrc:/PlaybackSettings.qml"
        }
        ListElement {
            iconName: "format-list-unordered"
            name: "Playlist"
            page: "qrc:/PlaylistSettings.qml"
        }
        ListElement {
            iconName: "dialog-layers"
            name: "Presentation"
            page: "qrc:/PresentationSettings.qml"
        }
        ListElement {
            iconName: "edit-paste-style"
            name: "User interface"
            page: "qrc:/UserInterfaceSettings.qml"
        }
    }
    ListView {
        id: settingsPagesList

        anchors.fill: parent
        model: settingsPagesModel

        delegate: Kirigami.BasicListItem {
            icon: iconName
            text: qsTr(name)

            onClicked: applicationWindow().pageStack.push(model.page)
        }
    }
}
