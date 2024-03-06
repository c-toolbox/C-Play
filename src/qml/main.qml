/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick.Window 2.12
import QtQuick.Layouts 1.12
import QtGraphicalEffects 1.12
import Qt.labs.platform 1.0 as Platform

import org.kde.kirigami 2.11 as Kirigami
import com.georgefb.haruna 1.0

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

        FileMenu {}
        ViewMenu {}
        PlaybackMenu {}
        //SubtitlesMenu {}
        AudioMenu {}
        SettingsMenu {}
        HelpMenu {}

        //hoverEnabled: true
        //visible: !window.isFullScreen() && GeneralSettings.showMenuBar
        background: Rectangle {
            color: Kirigami.Theme.backgroundColor
        }
        Kirigami.Theme.colorSet: Kirigami.Theme.Header
    }

    Menu {
        id: mpvContextMenu

        modal: true

        FileMenu {}
        ViewMenu {}
        PlaybackMenu {}
        //SubtitlesMenu {}
        AudioMenu {}
        SettingsMenu {}
        HelpMenu {}
    }

    SystemPalette { id: systemPalette; colorGroup: SystemPalette.Active }

    SettingsEditor { id: settingsEditor }

    SaveAsCPlayFile { id: saveAsCPlayFileWindow }

    Actions { id: actions }

    BackgroundImage {
        id: bgImage
    }

    MpvVideo {
        id: mpv

        Osd { id: osd }
    }

    PlaySections { id: playSections }

    PlayList { id: playList }

    Footer { id: footer }

    Platform.FileDialog {
        id: openFileDialog
        folder: GeneralSettings.fileDialogLocation !== ""
                ? app.pathToUrl(GeneralSettings.fileDialogLocation)
                : app.pathToUrl(GeneralSettings.fileDialogLastLocation)
        title: "Open File"
        fileMode: Platform.FileDialog.OpenFile

        onAccepted: {
            openFile(openFileDialog.file.toString(), true, PlaylistSettings.loadSiblings)
            // the timer scrolls the playlist to the playing file
            // once the table view rows are loaded
            playList.scrollPositionTimer.start()
            mpv.focus = true

            GeneralSettings.fileDialogLastLocation = app.parentUrl(openFileDialog.file)
            GeneralSettings.save()
        }
        onRejected: mpv.focus = true
    }

    Platform.FileDialog {
        id: addToPlaylistDialog
        folder: GeneralSettings.fileDialogLocation !== ""
                ? app.pathToUrl(GeneralSettings.fileDialogLocation)
                : app.pathToUrl(GeneralSettings.fileDialogLastLocation)
        title: "Add file to playlist"
        fileMode: Platform.FileDialog.OpenFile
        nameFilters: [ "C-Play file (*.cplayfile)", "Uniview file (*.fdv)" ]

        onAccepted: {
            mpv.addFileToPlaylist(addToPlaylistDialog.file.toString())
            mpv.focus = true

            GeneralSettings.fileDialogLastLocation = app.parentUrl(addToPlaylistDialog.file)
            GeneralSettings.save()
        }
        onRejected: mpv.focus = true
    }

    Platform.FileDialog {
        id: saveCPlayFileDialog

        folder: GeneralSettings.cPlayFileLocation !== ""
                ? app.pathToUrl(GeneralSettings.cPlayFileLocation)
                : app.pathToUrl(GeneralSettings.fileDialogLastLocation)
        title: "Save C-Play File Config"
        fileMode: Platform.FileDialog.SaveFile
        nameFilters: [ "C-Play file (*.cplayfile)" ]

        onAccepted: {
            saveCPlayFile(saveCPlayFileDialog.file.toString())
            mpv.focus = true
            saveCPlayFileDialog.visible = false;

            GeneralSettings.fileDialogLastLocation = app.parentUrl(saveCPlayFileDialog.file)
            GeneralSettings.save()

            if (saveCPlayFileDialog.visible) {
                saveCPlayFileDialog.close()
            }
        }
        onRejected: mpv.focus = true
    }

    Platform.FileDialog {
        id: saveCPlayPlaylistDialog

        folder: GeneralSettings.cPlayFileLocation !== ""
                ? app.pathToUrl(GeneralSettings.cPlayFileLocation)
                : app.pathToUrl(GeneralSettings.fileDialogLastLocation)
        title: "Save C-Playlist"
        fileMode: Platform.FileDialog.SaveFile
        nameFilters: [ "C-Play playlist (*.cplaylist)" ]

        onAccepted: {
            saveCPlayPlaylist(saveCPlayPlaylistDialog.file.toString())
            mpv.focus = true

            GeneralSettings.fileDialogLastLocation = app.parentUrl(saveCPlayPlaylistDialog.file)
            GeneralSettings.save()
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
                Component.onCompleted: text = GeneralSettings.lastUrl

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
                    GeneralSettings.lastUrl = openUrlTextField.text
                    // in case the url is a playList, it opens the first video
                    GeneralSettings.lastPlaylistIndex = 0
                    openUrlPopup.close()
                    openUrlTextField.clear()
                }
            }
        }
    }

    Component.onCompleted: app.activateColorScheme(GeneralSettings.colorScheme)

    function openFile(path, startPlayback, loadSiblings) {
        /*if (app.isYoutubePlaylist(path)) {
            mpv.getYouTubePlaylist(path);
            playList.isYouTubePlaylist = true
        } else {
            playList.isYouTubePlaylist = false
        }*/

        mpv.pause = !startPlayback
        mpv.position = 0
        if (loadSiblings) {
            // get video files from same folder as the opened file
            mpv.playlistModel.getVideos(path)
        }
        mpv.loadFile(path)
    }

    function saveCPlayFile(path) {
        mpv.playSectionsModel.currentEditItem.saveAsJSONPlayFile(path)
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
