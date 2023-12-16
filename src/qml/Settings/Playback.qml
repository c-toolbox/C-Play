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
import com.georgefb.haruna 1.0
import Haruna.Components 1.0

SettingsBasePage {
    id: root

    hasHelp: true
    helpFile: ":/PlaybackSettings.html"

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

    GridLayout {
        id: content

        columns: 2

        // ------------------------------------
        // DECODING AND SYNC PARAMETERS
        // --
        SettingsHeader {
            //text: qsTr("Decoding and sync settings")
            text: qsTr("Sync settings")
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
            text: qsTr("Set position interval:")
        }

        RowLayout {
            SpinBox {
                id: timeSetInterval
                from: 0
                to: 20000
                value: PlaybackSettings.intervalToSetPosition

                onValueChanged: {
                    PlaybackSettings.intervalToSetPosition = value
                    PlaybackSettings.save()
                }
            }

            LabelWithTooltip {
                text: {
                    qsTr("ms = Set time position every %1 seconds of the video").arg(Number((timeSetInterval.value*1.0)/1000.0).toFixed(3))
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
            text: qsTr("Startup / Background settings")
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
            id: stereoscopicModeOnStartupComboBox
            enabled: true
            textRole: "mode"
            model: ListModel {
                id: stereoscopicModeOnStartupMode
                ListElement { mode: "2D (mono)"; value: 0 }
                ListElement { mode: "3D (side-by-side)"; value: 1}
                ListElement { mode: "3D (top-bottom)"; value: 2 }
                ListElement { mode: "3D (top-bottom+flip)"; value: 3 }
            }

            onActivated: {
                playerController.setBackgroundStereoMode(model.get(index).value);
            }

            Component.onCompleted: {
                for (let i = 0; i < stereoscopicModeOnStartupMode.count; ++i) {
                    if (stereoscopicModeOnStartupMode.get(i).value === playerController.backgroundStereoMode()) {
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
            id: loadGridOnStartupComboBox
            enabled: true
            textRole: "mode"
            model: ListModel {
                id: loadGridOnStartupMode
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
                for (let i = 0; i < loadGridOnStartupMode.count; ++i) {
                    if (loadGridOnStartupMode.get(i).value === playerController.backgroundGridMode()) {
                        currentIndex = i
                        break
                    }
                }
            }
        }

        // ------------------------------------
        // Save & Reset
        // ------------------------------------

        RowLayout {
            Button {
                    text: "Reset background values"
                    onClicked: {
                        playerController.setBackgroundImageFile(PlaybackSettings.imageToLoadAsBackground)
                        playerController.setBackgroundStereoMode(PlaybackSettings.stereoModeForBackground)
                        playerController.setBackgroundGridMode(PlaybackSettings.gridToMapOnForBackground)

                        fileForBackgroundImageText.text = playerController.backgroundImageFile();

                        for (let i = 0; i < loadGridOnStartupMode.count; ++i) {
                            if (loadGridOnStartupMode.get(i).value === playerController.backgroundGridMode()) {
                                loadGridOnStartupComboBox.currentIndex = i
                                break
                            }
                        }

                        for (let j = 0; j < stereoscopicModeOnStartupMode.count; ++j) {
                            if (stereoscopicModeOnStartupMode.get(j).value === playerController.backgroundStereoMode()) {
                                stereoscopicModeOnStartupComboBox.currentIndex = j
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

        Item {
            width: Kirigami.Units.gridUnit
            height: Kirigami.Units.gridUnit
        }
        Item {
            width: Kirigami.Units.gridUnit
            height: Kirigami.Units.gridUnit
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
