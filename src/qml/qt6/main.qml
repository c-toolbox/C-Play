/*
 * SPDX-FileCopyrightText:
 * 2021-2025 Erik Sunden <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick.Window
import QtQuick.Layouts
import QtQuick
import QtQuick.Controls
import Qt.labs.platform as Platform

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

import "Menus"
import "Settings"

Kirigami.ApplicationWindow {
    id: window

    property var appActions: actions.list
    property var configure: app.action("configure")
    property bool isFullScreenMode: false
    property bool isIdleMode: false
    property bool hideUI: (isFullScreenMode || isIdleMode)
    property url newMediaFileToOpen: ""

    function openMediaFile(path, startPlayback, loadSiblings) {
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

    function openFile(path) {
        var openFileExt = playerController.returnFileExtension(path);
        if(openFileExt == "cplaypres"){
            slides.presentationToLoad = path.toString();
            slides.openCPlayPresentation();
        }
        else if(UserInterfaceSettings.mappingModeOnOpenFile 
        && openFileExt != "cplayfile" && openFileExt != "cplaylist" 
        && openFileExt != "cplay_file" && openFileExt != "cplay_list" 
        && openFileExt != "fdv" && openFileExt != "playlist"){
            openFileValuesDialogLabel.text = playerController.returnFileName(path);
            newMediaFileToOpen = path;
            openFileValuesDialog.open();
        }
        else{
            openMediaFile(path.toString(), true, PlaylistSettings.loadSiblings);
        }
        // the timer scrolls the playlist to the playing file
        // once the table view rows are loaded
        playList.scrollPositionTimer.start();
        mpv.focus = true;
        LocationSettings.fileDialogLastLocation = app.parentUrl(path);
        LocationSettings.save();
    }

    Connections {
        function onActionsUpdated() {
            actions.updateShortcuts();
            actionsAlternate.updateShortcuts();
        }
        function onApplicationInteraction() {
            if(window.isIdleMode) {
                window.isIdleMode = false;
            }
            if(UserInterfaceSettings.idleModeOn){
                idleModeTimer.restart();
            }
        }
        target: app
    }

    title: mpv.mediaTitle || qsTr("C-Play")
    visible: true
    visibility: window.isFullScreenMode ? Window.FullScreen : Window.Windowed
    color: window.hideUI ? "black" : Kirigami.Theme.alternateBackgroundColor
    maximumHeight: 909 * Screen.devicePixelRatio > Screen.height ? 909 : Screen.height 
    maximumWidth: 1616 * Screen.devicePixelRatio > Screen.width ? 1616 : Screen.width
    minimumHeight: 621 > Screen.height / 2 ? Screen.height / 2 : 621
    minimumWidth: 1104 > Screen.width / 2 ? Screen.width / 2 : 1104
    height: 909 * Screen.devicePixelRatio > Screen.height ? Screen.height - 60 : 909
    width: 1616 * Screen.devicePixelRatio > Screen.width ? Screen.width - 20 : 1616

    header: Header {
        id: header

    }
    menuBar: MenuBar {
        id: menuBar
        property bool hide: false

        visible: !window.hideUI && !menuBar.hide

        Kirigami.Theme.colorSet: Kirigami.Theme.Header

        background: Rectangle {
            color: Kirigami.Theme.backgroundColor
        }

        FileMenu {
        }
        PlaybackMenu {
        }
        AudioMenu {
        }
        SubtitleMenu {
        }
        SettingsMenu {
        }
        HelpMenu {
        }
    }

    Component.onCompleted: app.activateColorScheme(UserInterfaceSettings.colorScheme)

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
    SlidesQtItem {
        id: slidesViewItem

        Component.onCompleted: {
            slidesViewItem.initializeWithControlWindow(window, app.slides);
        }
    }
    Actions {
        id: actions

    }
    Actions {
        id: actionsAlternate
        isPrimary: false
    }
    BackgroundImage {
        id: bgImage

    }
    MpvVideo {
        id: mpv

        onFileLoaded: {
            floatingOverlayImage.source = mpv.getOverlayFileUrl();
            floatingOverlayImage.opacity = (floatingOverlayImage.source !== "" ? 1 : 0);
        }

        Osd {
            id: osd

        }
        DropArea {
            id: dropAreaMpv

            anchors.fill: parent
            keys: ["text/uri-list"]

            onDropped: {
                openFile(app.pathToUrl(drop.urls[0]))
            }
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

        DropArea {
            id: dropAreaPlaylist

            anchors.fill: parent
            keys: ["text/uri-list"]

            onDropped: {
                for(var i in drop.urls){
                    mpv.addFileToPlaylist(drop.urls[i].toString());      
                }
                mpv.focus = true;
            }
        }
    }
    Slides {
        id: slides

    }
    SlidesVisibilityView {
        id: slidesVisView

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

    Window {
        id: floatingTextureWindow
        visible: false
        flags: Qt.FramelessWindowHint | Qt.Window
        x: UserInterfaceSettings.floatingWindowPosX; 
        y: UserInterfaceSettings.floatingWindowPosY; 
        width: UserInterfaceSettings.floatingWindowWidth;
        height: UserInterfaceSettings.floatingWindowHeight;

        LayerQtItem {
            id: floatingLayerViewItem
            visible: !UserInterfaceSettings.floatingWindowShowsMainVideoLayer

            height: parent.height
            width: parent.width

            Component.onCompleted: {
                if(UserInterfaceSettings.floatingWindowLayerType >= 0 && UserInterfaceSettings.floatingWindowLayerPath !== ""){
                    floatingLayerViewItem.createLayer(UserInterfaceSettings.floatingWindowLayerType, UserInterfaceSettings.floatingWindowLayerPath);
                    floatingLayerViewItem.layerVolume = UserInterfaceSettings.floatingWindowVolume;
                }
                if(UserInterfaceSettings.floatingWindowVisibleAtStartup){
                    floatingTextureWindow.visible = true;
                }
            }
        }

        MpvView {
            id: floatingMpvView
            visible: UserInterfaceSettings.floatingWindowShowsMainVideoLayer
            anchors.fill: parent
            mpvObject: mpv
        }

        Image {
            id: floatingOverlayImage
            visible: UserInterfaceSettings.floatingWindowShowsMainVideoLayer
            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
            opacity: 1
        }

        onVisibleChanged: {
            if(!UserInterfaceSettings.floatingWindowShowsMainVideoLayer){
                if (visible){
                    floatingLayerViewItem.start()
                }
                else {
                    floatingLayerViewItem.stop()
                }
            }
        }
    }

    Platform.FileDialog {
        id: openFileDialog

        fileMode: Platform.FileDialog.OpenFile
        folder: LocationSettings.fileDialogLocation !== "" ? app.pathToUrl(LocationSettings.fileDialogLocation) : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        title: "Open File"

        Dialog {
            id: openFileValuesDialog
            standardButtons: Dialog.Ok | Dialog.Cancel
            width: 250

            GridLayout {
                anchors.fill: parent
                anchors.margins: 15
                columnSpacing: 2
                columns: 2
                rowSpacing: 8

                RowLayout {
                    Label {
                        text: qsTr("Mappings of ")
                        font.bold: true
                    }
                    Label {
                        id: openFileValuesDialogLabel
                        text: "..."
                        font.italic: true
                        Layout.fillWidth: true
                    }
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                }

                Label {
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("Stereo:")
                }
                ComboBox {
                    id: stereoscopicMode

                    Layout.fillWidth: true
                    focusPolicy: Qt.NoFocus
                    textRole: "mode"

                    model: ListModel {
                        id: stereoscopicModeList

                        ListElement {
                            mode: "2D (mono)"
                            value: 0
                        }
                        ListElement {
                            mode: "3D (side-by-side)"
                            value: 1
                        }
                        ListElement {
                            mode: "3D (top-bottom)"
                            value: 2
                        }
                        ListElement {
                            mode: "3D (top-bottom+flip)"
                            value: 3
                        }
                    }
                }
                Label {
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("Grid:")
                }
                ComboBox {
                    id: gridMode

                    Layout.fillWidth: true
                    focusPolicy: Qt.NoFocus
                    textRole: "mode"

                    model: ListModel {
                        id: gridModeList

                        ListElement {
                            mode: "None/Pre-split"
                            value: 0
                        }
                        ListElement {
                            mode: "Plane"
                            value: 1
                        }
                        ListElement {
                            mode: "Dome"
                            value: 2
                        }
                        ListElement {
                            mode: "Sphere EQR"
                            value: 3
                        }
                        ListElement {
                            mode: "Sphere EAC"
                            value: 4
                        }
                    }
                }
            }

            onVisibleChanged: {
                for (let i = 0; i < stereoscopicModeList.count; ++i) {
                    if (stereoscopicModeList.get(i).value === playerController.backgroundStereoMode()) {
                        stereoscopicMode.currentIndex = i;
                        break;
                    }
                }
                for (let i = 0; i < gridModeList.count; ++i) {
                    if (gridModeList.get(i).value === playerController.backgroundGridMode()) {
                        gridMode.currentIndex = i;
                        break;
                    }
                }
            }

            onAccepted: {
                openMediaFile(newMediaFileToOpen.toString(), true, PlaylistSettings.loadSiblings);
                mpv.stereoscopicMode = stereoscopicModeList.get(stereoscopicMode.currentIndex).value;
                mpv.gridToMapOn = gridModeList.get(gridMode.currentIndex).value;
                // the timer scrolls the playlist to the playing file
                // once the table view rows are loaded
                playList.scrollPositionTimer.start();
                mpv.focus = true;
                LocationSettings.fileDialogLastLocation = app.parentUrl(newMediaFileToOpen);
                LocationSettings.save();
            }
        }

        onAccepted: {
            openFile(openFileDialog.file);
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
    Timer {
        id: garbageCollectionTimer

        interval: 5000
        repeat: false

        onTriggered: {
            gc();
        }
    }
    Timer {
        id: idleModeTimer

        interval: UserInterfaceSettings.idleModeTime * 1000
        running: UserInterfaceSettings.idleModeOn && !window.isIdleMode
        repeat: false

        onTriggered: {
            window.isIdleMode = true;
            garbageCollectionTimer.restart();
        }
    }
}
