/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick.Window
import QtQuick.Layouts
import QtQuick
import QtQuick.Controls
import Qt.labs.platform 1.0 as Platform

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

import "Menus"
import "Settings"

Kirigami.ApplicationWindow {
    id: window

    property var appActions: actions.list
    property var configure: app.action("configure")
    property int preFullScreenVisibility

    function isFullScreen() {
        return window.visibility === Window.FullScreen;
    }
    function openFile(path, startPlayback, loadSiblings) {
        mpv.pause = true;
        mpv.position = 0;
        if (loadSiblings) {
            // get video files from same folder as the opened file
            mpv.playlistModel.getVideos(path);
        }
        mpv.loadFile(path);
    }
    function saveCPlayFile(path) {
        mpv.playSectionsModel.currentEditItem.saveAsJSONPlayFile(path);
        mpv.playSectionsModel.setCurrentEditItemIsEdited(false);
    }
    function saveCPlayPlaylist(path) {
        mpv.playlistModel.saveAsJSONPlaylist(path);
        mpv.playlistModelChanged();
    }
    function toggleFullScreen() {
        if (!isFullScreen()) {
            window.showFullScreen();
        } else {
            if (window.preFullScreenVisibility === Window.Windowed) {
                window.showNormal();
            }
            if (window.preFullScreenVisibility === Window.Maximized) {
                window.show();
                window.showMaximized();
            }
        }
        app.showCursor();
        playList.scrollPositionTimer.start();
    }

    color: Kirigami.Theme.alternateBackgroundColor
    height: 880
    maximumHeight: 990
    maximumWidth: 1728
    minimumHeight: 660
    minimumWidth: 1152
    title: mpv.mediaTitle || qsTr("C-Play")
    visible: true
    width: 1610

    header: Header {
        id: header

    }
    menuBar: MenuBar {
        id: menuBar

        Kirigami.Theme.colorSet: Kirigami.Theme.Header
        visible: UserInterfaceSettings.showMenuBar

        background: Rectangle {
            color: Kirigami.Theme.backgroundColor
        }

        FileMenu {
        }
        PlaybackMenu {
        }
        AudioMenu {
        }
        SettingsMenu {
        }
        HelpMenu {
        }
    }

    Component.onCompleted: app.activateColorScheme(UserInterfaceSettings.colorScheme)
    onVisibilityChanged: function (visibility) {
        if (!window.isFullScreen()) {
            preFullScreenVisibility = visibility;
        }
    }

    SystemPalette {
        id: systemPalette

        colorGroup: SystemPalette.Active
    }
    SettingsEditor {
        id: settingsEditor

    }
    SaveAsCPlayFile {
        id: saveAsCPlayFileWindow

    }
    ViewPlaylistItem {
        id: viewPlaylistItemWindow

    }
    Actions {
        id: actions

    }
    BackgroundImage {
        id: bgImage

    }
    MpvVideo {
        id: mpv

        Osd {
            id: osd

        }
    }
    ForegroundImage {
        id: fgImage

    }
    PlaySections {
        id: playSections

    }
    PlayList {
        id: playList

    }
    Slides {
        id: slides

    }
    Layers {
        id: layers

    }
    LayersAddNew {
        id: layersAddNew

    }
    LayerView {
        id: layerView

    }
    LayerViewGridParams {
        id: layerViewGridParams

    }
    Footer {
        id: footer

    }
    Platform.FileDialog {
        id: openFileDialog

        fileMode: Platform.FileDialog.OpenFile
        folder: LocationSettings.fileDialogLocation !== "" ? app.pathToUrl(LocationSettings.fileDialogLocation) : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        title: "Open File"

        onAccepted: {
            openFile(openFileDialog.file.toString(), true, PlaylistSettings.loadSiblings);
            // the timer scrolls the playlist to the playing file
            // once the table view rows are loaded
            playList.scrollPositionTimer.start();
            mpv.focus = true;
            LocationSettings.fileDialogLastLocation = app.parentUrl(openFileDialog.file);
            LocationSettings.save();
        }
        onRejected: mpv.focus = true
    }
    Platform.FileDialog {
        id: addToPlaylistDialog

        fileMode: Platform.FileDialog.OpenFile
        folder: LocationSettings.cPlayFileLocation !== "" ? app.pathToUrl(LocationSettings.cPlayFileLocation) : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        nameFilters: ["C-Play file (*.cplayfile)", "Uniview file (*.fdv)", "All files (*)"]
        title: "Add file to playlist"

        onAccepted: {
            mpv.addFileToPlaylist(addToPlaylistDialog.file.toString());
            mpv.focus = true;
        }
        onRejected: mpv.focus = true
    }
    Platform.FileDialog {
        id: saveCPlayFileDialog

        fileMode: Platform.FileDialog.SaveFile
        folder: LocationSettings.cPlayFileLocation !== "" ? app.pathToUrl(LocationSettings.cPlayFileLocation) : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        nameFilters: ["C-Play file (*.cplayfile)"]
        title: "Save C-Play File Config"

        onAccepted: {
            saveCPlayFile(saveCPlayFileDialog.file.toString());
            mpv.focus = true;
            saveCPlayFileDialog.visible = false;
            if (saveCPlayFileDialog.visible) {
                saveCPlayFileDialog.close();
            }
            saveAsCPlayFileWindow.visible = false;
        }
        onRejected: mpv.focus = true
    }
    Platform.FileDialog {
        id: saveCPlayPlaylistDialog

        fileMode: Platform.FileDialog.SaveFile
        folder: LocationSettings.cPlayFileLocation !== "" ? app.pathToUrl(LocationSettings.cPlayFileLocation) : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        nameFilters: ["C-Play playlist (*.cplaylist)"]
        title: "Save C-Playlist"

        onAccepted: {
            saveCPlayPlaylist(saveCPlayPlaylistDialog.file.toString());
            mpv.focus = true;
        }
        onRejected: mpv.focus = true
    }
    Popup {
        id: openUrlPopup

        x: 10
        y: 10

        onOpened: {
            openUrlTextField.forceActiveFocus(Qt.MouseFocusReason);
            openUrlTextField.selectAll();
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

                Layout.fillWidth: true
                Layout.preferredWidth: 400
                visible: app.hasYoutubeDl()

                Component.onCompleted: text = LocationSettings.lastUrl
                Keys.onPressed: {
                    if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                        openUrlButton.clicked();
                    }
                    if (event.key === Qt.Key_Escape) {
                        openUrlPopup.close();
                    }
                }
            }
            Button {
                id: openUrlButton

                text: qsTr("Open")
                visible: app.hasYoutubeDl()

                onClicked: {
                    openFile(openUrlTextField.text, true, false);
                    LocationSettings.lastUrl = openUrlTextField.text;
                    // in case the url is a playList, it opens the first video
                    PlaylistSettings.lastPlaylistIndex = 0;
                    openUrlPopup.close();
                    openUrlTextField.clear();
                }
            }
        }
    }
}
