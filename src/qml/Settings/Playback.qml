/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.12
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.12
import Qt.labs.platform 1.0 as Platform

import org.kde.kirigami 2.11 as Kirigami
import org.ctoolbox.cplay 1.0
import Haruna.Components 1.0

SettingsBasePage {
    id: root

    Platform.FileDialog {
        id: playlistToLoadOnStartupDialog

        folder: GeneralSettings.cPlayFileLocation !== ""
                ? app.pathToUrl(GeneralSettings.cPlayFileLocation)
                : app.pathToUrl(GeneralSettings.fileDialogLastLocation)
        title: "Choose playlist to load on startup"
        fileMode: Platform.FileDialog.OpenFile
        nameFilters: [ "C-Play playlist (*.cplaylist)", "Uniview playlist (*.playlist)" ]

        onAccepted: {
            var filePath = playerController.returnRelativeOrAbsolutePath(playlistToLoadOnStartupDialog.file.toString());
            PlaybackSettings.playlistToLoadOnStartup = filePath
            PlaybackSettings.save()

            mpv.focus = true
        }
        onRejected: mpv.focus = true
    }

    Platform.FileDialog {
        id: fileToLoadAsBackgroundDialog
        folder: GeneralSettings.cPlayMediaLocation !== ""
                ? app.pathToUrl(GeneralSettings.cPlayMediaLocation)
                : app.pathToUrl(GeneralSettings.fileDialogLastLocation)
        fileMode: Platform.FileDialog.OpenFile
        title: "Choose file to load on startup"
        nameFilters: [ "Image files (*.png *.jpg *.jpeg *.tga)" ]

        onAccepted: {
            playerController.setBackgroundImageFile(fileToLoadAsBackgroundDialog.file.toString());
            fileForBackgroundImageText.text = playerController.backgroundImageFile();
            mpv.focus = true
        }
        onRejected: mpv.focus = true
    }

    Platform.FileDialog {
        id: fileToLoadAsForegroundDialog
        folder: GeneralSettings.cPlayMediaLocation !== ""
                ? app.pathToUrl(GeneralSettings.cPlayMediaLocation)
                : app.pathToUrl(GeneralSettings.fileDialogLastLocation)
        fileMode: Platform.FileDialog.OpenFile
        title: "Choose file to load on startup"
        nameFilters: [ "Image files (*.png *.jpg *.jpeg *.tga)" ]

        onAccepted: {
            playerController.setForegroundImageFile(fileToLoadAsForegroundDialog.file.toString());
            fileForForegroundImageText.text = playerController.foregroundImageFile();
            mpv.focus = true
        }
        onRejected: mpv.focus = true
    }

    GridLayout {
        id: content

        columns: 2

        // ------------------------------------
        // DECODING AND SYNC PARAMETERS
        // --
        SettingsHeader {
            text: qsTr("Time settings")
            Layout.columnSpan: 2
            Layout.fillWidth: true
        }

        /*CheckBox {
            id: hwDecodingCheckBox
            text: qsTr("Use hardware decoding")
            checked: PlaybackSettings.useHWDecoding
            onCheckedChanged: {
                mpv.hwDecoding = checked
                PlaybackSettings.useHWDecoding = checked
                PlaybackSettings.save()
            }
        }

        ComboBox {
            id: hwDecodingComboBox

            enabled: hwDecodingCheckBox.checked
            textRole: "key"
            model: ListModel {
                id: hwDecModel
                ListElement { key: "auto"; }
                ListElement { key: "auto-safe"; }
                ListElement { key: "auto-copy"; }
                ListElement { key: "vdpau"; }
                ListElement { key: "vdpau-copy"; }
                ListElement { key: "vaapi"; }
                ListElement { key: "vaapi-copy"; }
                ListElement { key: "videotoolbox"; }
                ListElement { key: "videotoolbox-copy"; }
                ListElement { key: "dxva2"; }
                ListElement { key: "dxva2-copy"; }
                ListElement { key: "d3d11va"; }
                ListElement { key: "d3d11va-copy"; }
                ListElement { key: "mediacodec"; }
                ListElement { key: "mediacodec-copy"; }
                ListElement { key: "mmal"; }
                ListElement { key: "mmal-copy"; }
                ListElement { key: "nvdec"; }
                ListElement { key: "nvdec-copy"; }
                ListElement { key: "cuda"; }
                ListElement { key: "cuda-copy"; }
                ListElement { key: "crystalhd"; }
                ListElement { key: "rkmpp"; }
            }

            onActivated: {
                PlaybackSettings.hWDecoding = model.get(index).key
                PlaybackSettings.save()
                mpv.setProperty("hwdec", PlaybackSettings.hWDecoding)
            }

            Component.onCompleted: {
                for (let i = 0; i < hwDecModel.count; ++i) {
                    if (hwDecModel.get(i).key === PlaybackSettings.hWDecoding) {
                        currentIndex = i
                        break
                    }
                }
            }
        }*/

        Label {
            text: qsTr("Remember time position:")
        }

        RowLayout {
            SpinBox {
                id: timePositionSaving
                from: -1
                to: 9999
                value: PlaybackSettings.minDurationToSavePosition

                onValueChanged: {
                    PlaybackSettings.minDurationToSavePosition = value
                    PlaybackSettings.save()
                }
            }

            LabelWithTooltip {
                text: {
                    if (timePositionSaving.value === -1) {
                        return qsTr("Disabled")
                    } else if (timePositionSaving.value === 0) {
                        return qsTr("For all files")
                    } else if (timePositionSaving.value === 1) {
                        return qsTr("For files longer than %1 minute").arg(timePositionSaving.value)
                    } else {
                        return qsTr("For files longer than %1 minutes").arg(timePositionSaving.value)
                    }
                }
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        Label {
            text: qsTr("Time position sync threshold:")
        }

        RowLayout {
            SpinBox {
                id: timeThresholdSaving
                from: 100
                to: 5000
                value: PlaybackSettings.thresholdToSyncTimePosition

                onValueChanged: {
                    PlaybackSettings.thresholdToSyncTimePosition = value
                    PlaybackSettings.save()
                }
            }

            LabelWithTooltip {
                text: {
                    qsTr("ms = Set time position if it is %1 seconds off from master").arg(Number((timeThresholdSaving.value*1.0)/1000.0).toFixed(3))
                }
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        Label {
            text: qsTr("Set fade duration:")
        }

        RowLayout {
            SpinBox {
                id: fadeDuration
                from: 0
                to: 20000
                value: PlaybackSettings.fadeDuration

                onValueChanged: {
                    PlaybackSettings.fadeDuration = value
                    PlaybackSettings.save()
                }
            }

            LabelWithTooltip {
                text: {
                    qsTr("ms = Fades out/in %1 seconds in total when loading new content").arg(Number((timeSetInterval.value*1.0)/1000.0).toFixed(3))
                }
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        // ------------------------------------
        // STARTUP AND BACKGROUND PARAMETERS
        // --
        SettingsHeader {
            text: qsTr("Startup settings")
            Layout.columnSpan: 2
            Layout.fillWidth: true
        }

        CheckBox {
            id: checkSyncImageAudioFading
            text: qsTr("Sync audio+image fading at startup")
            checked: PlaybackSettings.syncImageVideoFading
            onCheckedChanged: {
                PlaybackSettings.syncImageVideoFading = checked
                PlaybackSettings.save()
            }
            Layout.columnSpan: 2
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("Playlist/file to load on startup:")
        }
        RowLayout {
            TextField {
                id: playlistToLoadOnStartupText
                text: PlaybackSettings.playlistToLoadOnStartup
                placeholderText: "Path to playlist"
                Layout.fillWidth: true
                onEditingFinished: {
                    PlaybackSettings.playlistToLoadOnStartup = text
                    PlaybackSettings.save()
                }

                ToolTip {
                    text: qsTr("Path to playlist")
                }
            }
            ToolButton {
                id: playlistToLoadOnStartupButton
                text: ""
                icon.name: "system-file-manager"
                icon.height: 16
                focusPolicy: Qt.NoFocus

                onClicked: {
                    playlistToLoadOnStartupDialog.open()
                }
            }
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("Volume at startup:")
        }
        SpinBox {
            from: 0
            to: 100
            value: GeneralSettings.volume
            editable: true
            onValueChanged: {
                GeneralSettings.volume = value.toFixed(0)
                GeneralSettings.save()
            }
        }


        SettingsHeader {
            text: qsTr("Background settings")
            Layout.columnSpan: 2
            Layout.fillWidth: true
        }
        Label {
            text: qsTr("Image to load as background:")
        }
        RowLayout {
            TextField {
                id: fileForBackgroundImageText
                text: playerController.backgroundImageFile()
                placeholderText: "Path to image file"
                Layout.fillWidth: true
                onEditingFinished: {
                    playerController.setBackgroundImageFile(text);
                }

                ToolTip {
                    text: qsTr("Path to image file for first start and use as background")
                }
            }
            ToolButton {
                id: fileToLoadAsBackgroundLoadButton
                text: ""
                icon.name: "system-file-manager"
                icon.height: 16
                focusPolicy: Qt.NoFocus

                onClicked: {
                    fileToLoadAsBackgroundDialog.open()
                }
            }
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("Stereoscopic mode for background:")
        }
        ComboBox {
            id: stereoscopicModeOnStartupComboBoxBg
            enabled: true
            textRole: "mode"
            model: ListModel {
                id: stereoscopicModeOnStartupModeBg
                ListElement { mode: "2D (mono)"; value: 0 }
                ListElement { mode: "3D (side-by-side)"; value: 1}
                ListElement { mode: "3D (top-bottom)"; value: 2 }
                ListElement { mode: "3D (top-bottom+flip)"; value: 3 }
            }

            onActivated: {
                playerController.setBackgroundStereoMode(model.get(index).value);
            }

            Component.onCompleted: {
                for (let i = 0; i < stereoscopicModeOnStartupModeBg.count; ++i) {
                    if (stereoscopicModeOnStartupModeBg.get(i).value === playerController.backgroundStereoMode()) {
                        currentIndex = i
                        break
                    }
                }
            }
        }

        Label {
            text: qsTr("Grid mode for background:")
        }
        ComboBox {
            id: loadGridOnStartupComboBoxBg
            enabled: true
            textRole: "mode"
            model: ListModel {
                id: loadGridOnStartupModeBg
                ListElement { mode: "None/Pre-split"; value: 0 }
                ListElement { mode: "Plane"; value: 1 }
                ListElement { mode: "Dome"; value: 2}
                ListElement { mode: "Sphere EQR"; value: 3 }
                ListElement { mode: "Sphere EAC"; value: 4 }
            }

            onActivated: {
                playerController.setBackgroundGridMode(model.get(index).value);
            }

            Component.onCompleted: {
                for (let i = 0; i < loadGridOnStartupModeBg.count; ++i) {
                    if (loadGridOnStartupModeBg.get(i).value === playerController.backgroundGridMode()) {
                        currentIndex = i
                        break
                    }
                }
            }
        }

        RowLayout {
            Button {
                    text: "Reset background values"
                    onClicked: {
                        playerController.setBackgroundImageFile(PlaybackSettings.imageToLoadAsBackground)
                        playerController.setBackgroundStereoMode(PlaybackSettings.stereoModeForBackground)
                        playerController.setBackgroundGridMode(PlaybackSettings.gridToMapOnForBackground)

                        fileForBackgroundImageText.text = playerController.backgroundImageFile();

                        for (let i = 0; i < loadGridOnStartupModeBg.count; ++i) {
                            if (loadGridOnStartupModeBg.get(i).value === playerController.backgroundGridMode()) {
                                loadGridOnStartupComboBoxBg.currentIndex = i
                                break
                            }
                        }

                        for (let j = 0; j < stereoscopicModeOnStartupModeBg.count; ++j) {
                            if (stereoscopicModeOnStartupModeBg.get(j).value === playerController.backgroundStereoMode()) {
                                stereoscopicModeOnStartupComboBoxBg.currentIndex = j
                                break
                            }
                        }
                    }
            }
            Layout.columnSpan: 2
            Layout.fillWidth: true
        }
        RowLayout {
            Button {
                    text: "Save background values for startup"
                    onClicked: {
                        PlaybackSettings.imageToLoadAsBackground = playerController.backgroundImageFile()
                        PlaybackSettings.stereoModeForBackground = playerController.backgroundStereoMode()
                        PlaybackSettings.gridToMapOnForBackground = playerController.backgroundGridMode()
                        PlaybackSettings.save()
                    }
            }
            Layout.columnSpan: 2
            Layout.fillWidth: true
        }

        SettingsHeader {
            text: qsTr("Foreground settings")
            Layout.columnSpan: 2
            Layout.fillWidth: true
        }
        Label {
            text: qsTr("Image to load as foreground:")
        }
        RowLayout {
            TextField {
                id: fileForForegroundImageText
                text: playerController.foregroundImageFile()
                placeholderText: "Path to image file"
                Layout.fillWidth: true
                onEditingFinished: {
                    playerController.setForegroundImageFile(text);
                }

                ToolTip {
                    text: qsTr("Path to image file to use as foreground")
                }
            }
            ToolButton {
                id: fileToLoadAsForegroundLoadButton
                text: ""
                icon.name: "system-file-manager"
                icon.height: 16
                focusPolicy: Qt.NoFocus

                onClicked: {
                    fileToLoadAsForegroundDialog.open()
                }
            }
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("Stereoscopic mode for foreground:")
        }
        ComboBox {
            id: stereoscopicModeOnStartupComboBoxFg
            enabled: true
            textRole: "mode"
            model: ListModel {
                id: stereoscopicModeOnStartupModeFg
                ListElement { mode: "2D (mono)"; value: 0 }
                ListElement { mode: "3D (side-by-side)"; value: 1}
                ListElement { mode: "3D (top-bottom)"; value: 2 }
                ListElement { mode: "3D (top-bottom+flip)"; value: 3 }
            }

            onActivated: {
                playerController.setForegroundStereoMode(model.get(index).value);
            }

            Component.onCompleted: {
                for (let i = 0; i < stereoscopicModeOnStartupModeFg.count; ++i) {
                    if (stereoscopicModeOnStartupModeFg.get(i).value === playerController.foregroundStereoMode()) {
                        currentIndex = i
                        break
                    }
                }
            }
        }

        Label {
            text: qsTr("Grid mode for foreground:")
        }
        ComboBox {
            id: loadGridOnStartupComboBoxFg
            enabled: true
            textRole: "mode"
            model: ListModel {
                id: loadGridOnStartupModeFg
                ListElement { mode: "None/Pre-split"; value: 0 }
                ListElement { mode: "Plane"; value: 1 }
                ListElement { mode: "Dome"; value: 2}
                ListElement { mode: "Sphere EQR"; value: 3 }
                ListElement { mode: "Sphere EAC"; value: 4 }
            }

            onActivated: {
                playerController.setForegroundGridMode(model.get(index).value);
            }

            Component.onCompleted: {
                for (let i = 0; i < loadGridOnStartupModeFg.count; ++i) {
                    if (loadGridOnStartupModeFg.get(i).value === playerController.foregroundGridMode()) {
                        currentIndex = i
                        break
                    }
                }
            }
        }

        RowLayout {
            Button {
                    text: "Reset foreground values"
                    onClicked: {
                        playerController.setForegroundImageFile(PlaybackSettings.imageToLoadAsForeground)
                        playerController.setForegroundStereoMode(PlaybackSettings.stereoModeForForeground)
                        playerController.setForegroundGridMode(PlaybackSettings.gridToMapOnForForeground)

                        fileForForegroundImageText.text = playerController.foregroundImageFile();

                        for (let i = 0; i < loadGridOnStartupModeFg.count; ++i) {
                            if (loadGridOnStartupModeFg.get(i).value === playerController.foregroundGridMode()) {
                                loadGridOnStartupComboBoxFg.currentIndex = i
                                break
                            }
                        }

                        for (let j = 0; j < stereoscopicModeOnStartupModeFg.count; ++j) {
                            if (stereoscopicModeOnStartupModeFg.get(j).value === playerController.foregroundStereoMode()) {
                                stereoscopicModeOnStartupComboBoxFg.currentIndex = j
                                break
                            }
                        }
                    }
            }
            Layout.columnSpan: 2
            Layout.fillWidth: true
        }
        RowLayout {
            Button {
                    text: "Save foreground values for startup"
                    onClicked: {
                        PlaybackSettings.imageToLoadAsForeground = playerController.foregroundImageFile()
                        PlaybackSettings.stereoModeForForeground = playerController.foregroundStereoMode()
                        PlaybackSettings.gridToMapOnForForeground = playerController.foregroundGridMode()
                        PlaybackSettings.save()
                    }
            }
            Layout.columnSpan: 2
            Layout.fillWidth: true
        }

        Item {
            width: Kirigami.Units.gridUnit
            height: Kirigami.Units.gridUnit
        }
        Item {
            width: Kirigami.Units.gridUnit
            height: Kirigami.Units.gridUnit
        }

        SettingsHeader {
            text: qsTr("Loaded configuration")
            Layout.columnSpan: 2
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("Configuration from external files:")
        }
        ScrollView {
            id: confText
            TextArea {
                text: mpv.getReadableExternalConfiguration()
                readOnly: true
                Layout.fillWidth: true
            }
            Layout.fillWidth: true
        }
    }
}
