/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
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
        id: fileToLoadAsBackgroundDialog
        folder: LocationSettings.cPlayMediaLocation !== ""
                ? app.pathToUrl(LocationSettings.cPlayMediaLocation)
                : app.pathToUrl(LocationSettings.fileDialogLastLocation)
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
        folder: LocationSettings.cPlayMediaLocation !== ""
                ? app.pathToUrl(LocationSettings.cPlayMediaLocation)
                : app.pathToUrl(LocationSettings.fileDialogLastLocation)
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

        columns: 3

        // ------------------------------------
        // BACKGROUND AND FOREGROUND PARAMETERS
        // --

        SettingsHeader {
            text: qsTr("Background & Foreground settings")
            Layout.columnSpan: 3
            Layout.fillWidth: true
        }
        Label {
            text: qsTr("Image to load as background:")
            Layout.alignment: Qt.AlignRight
        }
        RowLayout {
            TextField {
                id: fileForBackgroundImageText
                text: playerController.backgroundImageFile()
                placeholderText: "Path to image file"
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
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("Stereoscopic mode for background:")
            Layout.alignment: Qt.AlignRight
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
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("Grid mode for background:")
            Layout.alignment: Qt.AlignRight
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
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("Image to load as foreground:")
            Layout.alignment: Qt.AlignRight
        }
        RowLayout {
            TextField {
                id: fileForForegroundImageText
                text: playerController.foregroundImageFile()
                placeholderText: "Path to image file"
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
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("Stereoscopic mode for foreground:")
            Layout.alignment: Qt.AlignRight
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
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("Grid mode for foreground:")
            Layout.alignment: Qt.AlignRight
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
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        RowLayout {
            Button {
                    text: "Reset background and foreground values"
                    onClicked: {
                        playerController.setBackgroundImageFile(ImageSettings.imageToLoadAsBackground)
                        playerController.setBackgroundStereoMode(ImageSettings.stereoModeForBackground)
                        playerController.setBackgroundGridMode(ImageSettings.gridToMapOnForBackground)

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

                        playerController.setForegroundImageFile(ImageSettings.imageToLoadAsForeground)
                        playerController.setForegroundStereoMode(ImageSettings.stereoModeForForeground)
                        playerController.setForegroundGridMode(ImageSettings.gridToMapOnForForeground)

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
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        RowLayout {
            Button {
                    text: "Save background and foreground values for startup"
                    onClicked: {
                        ImageSettings.imageToLoadAsBackground = playerController.backgroundImageFile()
                        ImageSettings.stereoModeForBackground = playerController.backgroundStereoMode()
                        ImageSettings.gridToMapOnForBackground = playerController.backgroundGridMode()
                        ImageSettings.imageToLoadAsForeground = playerController.foregroundImageFile()
                        ImageSettings.stereoModeForForeground = playerController.foregroundStereoMode()
                        ImageSettings.gridToMapOnForForeground = playerController.foregroundGridMode()
                        ImageSettings.save()
                    }
            }
            Layout.columnSpan: 2
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        SettingsHeader {
            text: qsTr("Image adjustments")
            Layout.columnSpan: 3
            Layout.fillWidth: true
        }

        // ------------------------------------
        // CONTRAST
        // ------------------------------------
        Label {
            text: qsTr("Contrast")
            Layout.alignment: Qt.AlignRight
        }
        ImageAdjustmentSlider {
            id: contrastSlider

            value: mpv.contrast
            onSliderValueChanged: mpv.contrast = value.toFixed(0)

            Layout.topMargin: Kirigami.Units.largeSpacing
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        // ------------------------------------
        // BRIGHTNESS
        // ------------------------------------
        Label {
            text: qsTr("Brightness")
            Layout.alignment: Qt.AlignRight
        }
        ImageAdjustmentSlider {
            id: brightnessSlider

            value: mpv.brightness
            onSliderValueChanged: mpv.brightness = value.toFixed(0)

            Layout.topMargin: Kirigami.Units.largeSpacing
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        // ------------------------------------
        // GAMMA
        // ------------------------------------
        Label {
            text: qsTr("Gamma")
            Layout.alignment: Qt.AlignRight
        }
        ImageAdjustmentSlider {
            id: gammaSlider

            value: mpv.gamma
            onSliderValueChanged: mpv.gamma = value.toFixed(0)

            Layout.topMargin: Kirigami.Units.largeSpacing
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        // ------------------------------------
        // SATURATION
        // ------------------------------------
        Label {
            text: qsTr("Saturation")
            Layout.alignment: Qt.AlignRight
        }
        ImageAdjustmentSlider {
            id: saturationSlider

            value: mpv.saturation
            onSliderValueChanged: mpv.saturation = value.toFixed(0)

            Layout.topMargin: Kirigami.Units.largeSpacing
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        Item { width: 1; height: 1 }
        Label {
            text: qsTr("Middle click on the sliders to reset them")
            Layout.topMargin: Kirigami.Units.largeSpacing
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }
    }
}
