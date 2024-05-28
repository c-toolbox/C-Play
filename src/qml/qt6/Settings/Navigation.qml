/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import org.kde.kirigami as Kirigami

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
        property string pagePath: "qrc:/qt/qml/org/ctoolbox/cplay/qml/qt6/Settings"
		ListElement {
            name: "Audio"
            iconName: "audio-speakers-symbolic"
            page: "AudioSettings.qml"
        }
		ListElement {
            name: "Grid/mapping"
            iconName: "kstars_hgrid"
            page: "GridSettings.qml"
        }
		ListElement {
            name: "Image"
            iconName: "kdenlive-select-images"
            page: "ImageSettings.qml"
        }
        ListElement {
            name: "Location"
            iconName: "find-location"
            page: "LocationSettings.qml"
        }
		ListElement {
            name: "Mouse"
            iconName: "input-mouse"
            page: "MouseSettings.qml"
        }
        ListElement {
            name: "Playback"
            iconName: "video-x-generic"
            page: "PlaybackSettings.qml"
        }
        ListElement {
            name: "Playlist"
            iconName: "format-list-unordered"
            page: "PlaylistSettings.qml"
        }
		ListElement {
            name: "User interface"
            iconName: "edit-paste-style"
            page: "UserInterfaceSettings.qml"
        }
    }

    ListView {
        id: settingsPagesList

        anchors.fill: parent
        model: settingsPagesModel
        delegate: ItemDelegate {
            width: settingsPagesList.width
            text: qsTr(name)
            icon.name: iconName
            onClicked: applicationWindow().pageStack.push(`${settingsPagesModel.pagePath}/${model.page}`)
        }
    }
}
