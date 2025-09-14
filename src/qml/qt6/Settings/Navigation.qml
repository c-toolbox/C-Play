/*
 * SPDX-FileCopyrightText:
 * 2021-2025 Erik Sunden <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import org.kde.kirigami as Kirigami

Kirigami.Page {
    bottomPadding: 0
    leftPadding: 0
    padding: 0
    rightPadding: 0
    topPadding: 0

    Component.onCompleted: applicationWindow().pageStack.columnView.columnWidth = 250

    Loader {
        asynchronous: true
    }
    ListModel {
        id: settingsPagesModel

        property string pagePath: "qrc:/qt/qml/org/ctoolbox/cplay/qml/qt6/Settings"

        ListElement {
            iconName: "audio-speakers-symbolic"
            name: "Audio"
            page: "AudioSettings.qml"
        }
        ListElement {
            iconName: "kstars_hgrid"
            name: "Grid/mapping"
            page: "GridSettings.qml"
        }
        ListElement {
            iconName: "kdenlive-select-images"
            name: "Image"
            page: "ImageSettings.qml"
        }
        ListElement {
            iconName: "find-location"
            name: "Location"
            page: "LocationSettings.qml"
        }
        ListElement {
            iconName: "input-mouse"
            name: "Mouse"
            page: "MouseSettings.qml"
        }
        ListElement {
            iconName: "video-x-generic"
            name: "Playback"
            page: "PlaybackSettings.qml"
        }
        ListElement {
            iconName: "format-list-unordered"
            name: "Playlist"
            page: "PlaylistSettings.qml"
        }
        ListElement {
            iconName: "dialog-layers"
            name: "Presentation"
            page: "PresentationSettings.qml"
        }
        ListElement {
            iconName: "media-view-subtitles-symbolic"
            name: "Text & subtitles"
            page: "SubtitleSettings.qml"
        }
        ListElement {
            iconName: "edit-paste-style"
            name: "Window & UI"
            page: "UserInterfaceSettings.qml"
        }
    }
    ListView {
        id: settingsPagesList

        anchors.fill: parent
        model: settingsPagesModel

        delegate: ItemDelegate {
            icon.name: iconName
            text: qsTr(name)
            width: settingsPagesList.width
            highlighted: settingsPagesList.currentIndex === index
 
            onClicked: {
                settingsPagesList.currentIndex = index
                applicationWindow().pageStack.removePage(1);
                applicationWindow().pageStack.push(`${settingsPagesModel.pagePath}/${model.page}`);
            }
        }
    }
}
