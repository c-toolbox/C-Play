/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick.Window 2.12
import QtQuick.Layouts 1.12
import QtGraphicalEffects 1.12
import Qt.labs.platform 1.0 as Platform

import org.kde.kirigami 2.11 as Kirigami
import org.ctoolbox.cplay 1.0

import QtQuick 2.12
import QtQuick.Controls 2.12

import mpv 1.0
import "Menus"
import "Settings"

Kirigami.ApplicationWindow {
    id: window

    property var configure: app.action("configure")
    property int preFullScreenVisibility
    property var appActions: actions.list

    visible: true
    title: mpv.mediaTitle || qsTr("C-Play")
    width: 1580
    minimumWidth: 1152
    maximumWidth: 1728
    height: 880
    minimumHeight: 660
    maximumHeight: 990
    color: Kirigami.Theme.backgroundColor

    onVisibilityChanged: {
        if (!window.isFullScreen()) {
            preFullScreenVisibility = visibility
        }
    }

    header: Header { id: header }

    menuBar: MenuBar {
        id: menuBar
        visible: UserInterfaceSettings.showMenuBar

        FileMenu {}
        PlaybackMenu {}
        AudioMenu {}
        SettingsMenu {}
        HelpMenu {}

        background: Rectangle {
            color: Kirigami.Theme.backgroundColor
        }
        Kirigami.Theme.colorSet: Kirigami.Theme.Header
    }

    SystemPalette { id: systemPalette; colorGroup: SystemPalette.Active }

    SettingsEditor { id: settingsEditor }

    SaveAsCPlayFile { id: saveAsCPlayFileWindow }

    ViewPlaylistItem { id: viewPlaylistItemWindow }

    Actions { id: actions }

    BackgroundImage { id: bgImage }

    MpvVideo {
        id: mpv

        Osd { id: osd }
    }

    ForegroundImage { id: fgImage }

    PlaySections { id: playSections }

    PlayList { id: playList }

    Footer { id: footer }

    Platform.FileDialog {
        id: openFileDialog
        folder: LocationSettings.fileDialogLocation !== ""
                ? app.pathToUrl(LocationSettings.fileDialogLocation)
                : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        title: "Open File"
        fileMode: Platform.FileDialog.OpenFile

        onAccepted: {
            openFile(openFileDialog.file.toString(), true, PlaylistSettings.loadSiblings)
            // the timer scrolls the playlist to the playing file
            // once the table view rows are loaded
            playList.scrollPositionTimer.start()
            mpv.focus = true

            LocationSettings.fileDialogLastLocation = app.parentUrl(openFileDialog.file)
            LocationSettings.save()
        }
        onRejected: mpv.focus = true
    }

    Platform.FileDialog {
        id: addToPlaylistDialog
        folder: LocationSettings.cPlayFileLocation !== ""
                ? app.pathToUrl(LocationSettings.cPlayFileLocation)
                : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        title: "Add file to playlist"
        fileMode: Platform.FileDialog.OpenFile
        nameFilters: [ "C-Play file (*.cplayfile)", "Uniview file (*.fdv)", "All files (*)"  ]

        onAccepted: {
            mpv.addFileToPlaylist(addToPlaylistDialog.file.toString())
            mpv.focus = true
        }
        onRejected: mpv.focus = true
    }

    Platform.FileDialog {
        id: saveCPlayFileDialog

        folder: LocationSettings.cPlayFileLocation !== ""
                ? app.pathToUrl(LocationSettings.cPlayFileLocation)
                : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        title: "Save C-Play File Config"
        fileMode: Platform.FileDialog.SaveFile
        nameFilters: [ "C-Play file (*.cplayfile)" ]

        onAccepted: {
            saveCPlayFile(saveCPlayFileDialog.file.toString())
            mpv.focus = true
            saveCPlayFileDialog.visible = false;

            if (saveCPlayFileDialog.visible) {
                saveCPlayFileDialog.close()
            }

            saveAsCPlayFileWindow.visible = false
        }
        onRejected: mpv.focus = true
    }

    Platform.FileDialog {
        id: saveCPlayPlaylistDialog

        folder: LocationSettings.cPlayFileLocation !== ""
                ? app.pathToUrl(LocationSettings.cPlayFileLocation)
                : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        title: "Save C-Playlist"
        fileMode: Platform.FileDialog.SaveFile
        nameFilters: [ "C-Play playlist (*.cplaylist)" ]

        onAccepted: {
            saveCPlayPlaylist(saveCPlayPlaylistDialog.file.toString())
            mpv.focus = true
        }
        onRejected: mpv.focus = true
    }

    Popup {
        id: openUrlPopup
        x: 10
        y: 10

        onOpened: {
            openUrlTextField.forceActiveFocus(Qt.MouseFocusReason)
            openUrlTextField.selectAll()
        }

        RowLayout {
            anchors.fill: parent

            Label {
                text: qsTr("<a href=\"https://youtube-dl.org\">Youtube-dl</a> was not found.")
                visible: !app.hasYoutubeDl()
                onLinkActivated: Qt.openUrlExternally(link)
            }

            TextField {
                id: openUrlTextField

                visible: app.hasYoutubeDl()
                Layout.preferredWidth: 400
                Layout.fillWidth: true
                Component.onCompleted: text = LocationSettings.lastUrl

                Keys.onPressed: {
                    if (event.key === Qt.Key_Enter
                            || event.key === Qt.Key_Return) {
                        openUrlButton.clicked()
                    }
                    if (event.key === Qt.Key_Escape) {
                        openUrlPopup.close()
                    }
                }
            }
            Button {
                id: openUrlButton

                visible: app.hasYoutubeDl()
                text: qsTr("Open")

                onClicked: {
                    openFile(openUrlTextField.text, true, false)
                    LocationSettings.lastUrl = openUrlTextField.text
                    // in case the url is a playList, it opens the first video
                    PlaylistSettings.lastPlaylistIndex = 0
                    openUrlPopup.close()
                    openUrlTextField.clear()
                }
            }
        }
    }

    Component.onCompleted: app.activateColorScheme(UserInterfaceSettings.colorScheme)

    function openFile(path, startPlayback, loadSiblings) {
        mpv.pause = true
        mpv.position = 0
        if (loadSiblings) {
            // get video files from same folder as the opened file
            mpv.playlistModel.getVideos(path)
        }
        mpv.loadFile(path)
    }

    function saveCPlayFile(path) {
        mpv.playSectionsModel.currentEditItem.saveAsJSONPlayFile(path)
        mpv.playSectionsModel.setCurrentEditItemIsEdited(false)
    }

    function saveCPlayPlaylist(path) {
        mpv.playlistModel.saveAsJSONPlaylist(path)
        mpv.playlistModelChanged()
    }

    function isFullScreen() {
        return window.visibility === Window.FullScreen
    }

    function toggleFullScreen() {
        if (!isFullScreen()) {
            window.showFullScreen()
        } else {
            if (window.preFullScreenVisibility === Window.Windowed) {
                window.showNormal()
            }
            if (window.preFullScreenVisibility === Window.Maximized) {
                window.show()
                window.showMaximized()
            }
        }
        app.showCursor()
        playList.scrollPositionTimer.start()
    }

}
