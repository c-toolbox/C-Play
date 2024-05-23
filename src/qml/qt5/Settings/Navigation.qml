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
import org.kde.kirigami 2.11 as Kirigami

Kirigami.Page
{
    padding: 0
    leftPadding: 0
    topPadding: 0
    rightPadding: 0
    bottomPadding: 0

    Component.onCompleted: applicationWindow().pageStack.columnView.columnWidth = 250

    Loader { asynchronous: true }

    footer: ToolBar {
        RowLayout {
            anchors.fill: parent

            ToolButton {
                text: qsTr("Configure shortcuts")
                icon.name: "configure-shortcuts"
                onClicked: appActions.configureShortcutsAction.trigger()
                Layout.fillWidth: true
            }
        }
    }

    ListModel {
        id: settingsPagesModel
		ListElement {
            name: "Audio"
            iconName: "audio-speakers-symbolic"
            page: "qrc:/AudioSettings.qml"
        }
		ListElement {
            name: "Grid/mapping"
            iconName: "kstars_hgrid"
            page: "qrc:/GridSettings.qml"
        }
		ListElement {
            name: "Image"
            iconName: "kdenlive-select-images"
            page: "qrc:/ImageSettings.qml"
        }
        ListElement {
            name: "Location"
            iconName: "find-location"
            page: "qrc:/LocationSettings.qml"
        }
		ListElement {
            name: "Mouse"
            iconName: "input-mouse"
            page: "qrc:/MouseSettings.qml"
        }
        ListElement {
            name: "Playback"
            iconName: "video-x-generic"
            page: "qrc:/PlaybackSettings.qml"
        }
        ListElement {
            name: "Playlist"
            iconName: "format-list-unordered"
            page: "qrc:/PlaylistSettings.qml"
        }
		ListElement {
            name: "User interface"
            iconName: "edit-paste-style"
            page: "qrc:/UserInterfaceSettings.qml"
        }
    }

    ListView {
        id: settingsPagesList

        anchors.fill: parent
        model: settingsPagesModel
        delegate: Kirigami.BasicListItem {
            text: qsTr(name)
            icon: iconName
            onClicked: applicationWindow().pageStack.push(model.page)
        }
    }
}
