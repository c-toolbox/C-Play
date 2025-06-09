/*
 * SPDX-FileCopyrightText:
 * 2021-2025 Erik Sundén <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Qt.labs.platform 1.0 as Platform

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

SettingsBasePage {
    id: root

    Platform.FileDialog {
        id: fileToLoadAsBackgroundDialog

        fileMode: Platform.FileDialog.OpenFile
        folder: LocationSettings.cPlayMediaLocation !== "" ? app.pathToUrl(LocationSettings.cPlayMediaLocation) : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        nameFilters: ["Image files (*.png *.jpg *.jpeg *.tga)"]
        title: "Choose file to load on startup"

        onAccepted: {
            playerController.setBackgroundImageFile(fileToLoadAsBackgroundDialog.file.toString());
            fileForBackgroundImageText.text = playerController.backgroundImageFile();
            mpv.focus = true;
        }
        onRejected: mpv.focus = true
    }
    Platform.FileDialog {
        id: fileToLoadAsForegroundDialog

        fileMode: Platform.FileDialog.OpenFile
        folder: LocationSettings.cPlayMediaLocation !== "" ? app.pathToUrl(LocationSettings.cPlayMediaLocation) : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        nameFilters: ["Image files (*.png *.jpg *.jpeg *.tga)"]
        title: "Choose file to load on startup"

        onAccepted: {
            playerController.setForegroundImageFile(fileToLoadAsForegroundDialog.file.toString());
            fileForForegroundImageText.text = playerController.foregroundImageFile();
            mpv.focus = true;
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
            Layout.columnSpan: 3
            Layout.fillWidth: true
            text: qsTr("Background & Foreground settings")
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Image to load as background:")
        }
        RowLayout {
            TextField {
                id: fileForBackgroundImageText

                placeholderText: "Path to image file"
                text: playerController.backgroundImageFile()

                onEditingFinished: {
                    playerController.setBackgroundImageFile(text);
                }

                ToolTip {
                    text: qsTr("Path to image file for first start and use as background")
                }
            }
            ToolButton {
                id: fileToLoadAsBackgroundLoadButton

                focusPolicy: Qt.NoFocus
                icon.height: 16
                icon.name: "system-file-manager"
                text: ""

                onClicked: {
                    fileToLoadAsBackgroundDialog.open();
                }
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Stereoscopic mode for background:")
        }
        RowLayout {
            ComboBox {
                id: stereoscopicModeOnStartupComboBoxBg

                enabled: true
                textRole: "mode"

                model: ListModel {
                    id: stereoscopicModeOnStartupModeBg

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

                Component.onCompleted: {
                    for (let i = 0; i < stereoscopicModeOnStartupModeBg.count; ++i) {
                        if (stereoscopicModeOnStartupModeBg.get(i).value === playerController.backgroundStereoMode()) {
                            currentIndex = i;
                            break;
                        }
                    }
                }
                onActivated: {
                    playerController.setBackgroundStereoMode(model.get(index).value);
                }
            }
            Label {
                Layout.alignment: Qt.AlignLeft
                font.italic: true
                text: qsTr("Also used as startup value.")
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Grid mode for background:")
        }
        RowLayout {
            ComboBox {
                id: loadGridOnStartupComboBoxBg

                enabled: true
                textRole: "mode"

                model: ListModel {
                    id: loadGridOnStartupModeBg

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

                Component.onCompleted: {
                    for (let i = 0; i < loadGridOnStartupModeBg.count; ++i) {
                        if (loadGridOnStartupModeBg.get(i).value === playerController.backgroundGridMode()) {
                            currentIndex = i;
                            break;
                        }
                    }
                }
                onActivated: {
                    playerController.setBackgroundGridMode(model.get(index).value);
                }
            }
            Label {
                Layout.alignment: Qt.AlignLeft
                font.italic: true
                text: qsTr("Also used as startup value.")
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Image to load as foreground:")
        }
        RowLayout {
            TextField {
                id: fileForForegroundImageText

                placeholderText: "Path to image file"
                text: playerController.foregroundImageFile()

                onEditingFinished: {
                    playerController.setForegroundImageFile(text);
                }

                ToolTip {
                    text: qsTr("Path to image file to use as foreground")
                }
            }
            ToolButton {
                id: fileToLoadAsForegroundLoadButton

                focusPolicy: Qt.NoFocus
                icon.height: 16
                icon.name: "system-file-manager"
                text: ""

                onClicked: {
                    fileToLoadAsForegroundDialog.open();
                }
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Stereoscopic mode for foreground:")
        }
        ComboBox {
            id: stereoscopicModeOnStartupComboBoxFg

            enabled: true
            textRole: "mode"

            model: ListModel {
                id: stereoscopicModeOnStartupModeFg

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

            Component.onCompleted: {
                for (let i = 0; i < stereoscopicModeOnStartupModeFg.count; ++i) {
                    if (stereoscopicModeOnStartupModeFg.get(i).value === playerController.foregroundStereoMode()) {
                        currentIndex = i;
                        break;
                    }
                }
            }
            onActivated: {
                playerController.setForegroundStereoMode(model.get(index).value);
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Grid mode for foreground:")
        }
        ComboBox {
            id: loadGridOnStartupComboBoxFg

            enabled: true
            textRole: "mode"

            model: ListModel {
                id: loadGridOnStartupModeFg

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

            Component.onCompleted: {
                for (let i = 0; i < loadGridOnStartupModeFg.count; ++i) {
                    if (loadGridOnStartupModeFg.get(i).value === playerController.foregroundGridMode()) {
                        currentIndex = i;
                        break;
                    }
                }
            }
            onActivated: {
                playerController.setForegroundGridMode(model.get(index).value);
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }
        RowLayout {
            Layout.columnSpan: 2

            Button {
                text: "Reset background and foreground values"

                onClicked: {
                    playerController.setBackgroundImageFile(ImageSettings.imageToLoadAsBackground);
                    playerController.setBackgroundStereoMode(ImageSettings.stereoModeForBackground);
                    playerController.setBackgroundGridMode(ImageSettings.gridToMapOnForBackground);
                    fileForBackgroundImageText.text = playerController.backgroundImageFile();
                    for (let i = 0; i < loadGridOnStartupModeBg.count; ++i) {
                        if (loadGridOnStartupModeBg.get(i).value === playerController.backgroundGridMode()) {
                            loadGridOnStartupComboBoxBg.currentIndex = i;
                            break;
                        }
                    }
                    for (let j = 0; j < stereoscopicModeOnStartupModeBg.count; ++j) {
                        if (stereoscopicModeOnStartupModeBg.get(j).value === playerController.backgroundStereoMode()) {
                            stereoscopicModeOnStartupComboBoxBg.currentIndex = j;
                            break;
                        }
                    }
                    playerController.setForegroundImageFile(ImageSettings.imageToLoadAsForeground);
                    playerController.setForegroundStereoMode(ImageSettings.stereoModeForForeground);
                    playerController.setForegroundGridMode(ImageSettings.gridToMapOnForForeground);
                    fileForForegroundImageText.text = playerController.foregroundImageFile();
                    for (let i = 0; i < loadGridOnStartupModeFg.count; ++i) {
                        if (loadGridOnStartupModeFg.get(i).value === playerController.foregroundGridMode()) {
                            loadGridOnStartupComboBoxFg.currentIndex = i;
                            break;
                        }
                    }
                    for (let j = 0; j < stereoscopicModeOnStartupModeFg.count; ++j) {
                        if (stereoscopicModeOnStartupModeFg.get(j).value === playerController.foregroundStereoMode()) {
                            stereoscopicModeOnStartupComboBoxFg.currentIndex = j;
                            break;
                        }
                    }
                }
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }
        RowLayout {
            Layout.columnSpan: 2

            Button {
                text: "Save background and foreground values for startup"

                onClicked: {
                    ImageSettings.imageToLoadAsBackground = playerController.backgroundImageFile();
                    ImageSettings.stereoModeForBackground = playerController.backgroundStereoMode();
                    ImageSettings.gridToMapOnForBackground = playerController.backgroundGridMode();
                    ImageSettings.imageToLoadAsForeground = playerController.foregroundImageFile();
                    ImageSettings.stereoModeForForeground = playerController.foregroundStereoMode();
                    ImageSettings.gridToMapOnForForeground = playerController.foregroundGridMode();
                    ImageSettings.save();
                }
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }
        SettingsHeader {
            Layout.columnSpan: 3
            Layout.fillWidth: true
            text: qsTr("Media image adjustments")
        }
        Label {
            Layout.alignment: Qt.AlignLeft
            Layout.columnSpan: 3
            text: qsTr("Only applied to media player files, not background, foreground or overlay images.")
        }
        Label {
            Layout.alignment: Qt.AlignLeft
            Layout.columnSpan: 3
            text: qsTr("Never saved. Primary use is testing.")
        }

        // ------------------------------------
        // CONTRAST
        // ------------------------------------
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Contrast")
        }
        ImageAdjustmentSlider {
            id: contrastSlider

            Layout.topMargin: Kirigami.Units.largeSpacing
            value: mpv.contrast

            onSliderValueChanged: mpv.contrast = value.toFixed(0)
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        // ------------------------------------
        // BRIGHTNESS
        // ------------------------------------
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Brightness")
        }
        ImageAdjustmentSlider {
            id: brightnessSlider

            Layout.topMargin: Kirigami.Units.largeSpacing
            value: mpv.brightness

            onSliderValueChanged: mpv.brightness = value.toFixed(0)
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        // ------------------------------------
        // GAMMA
        // ------------------------------------
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Gamma")
        }
        ImageAdjustmentSlider {
            id: gammaSlider

            Layout.topMargin: Kirigami.Units.largeSpacing
            value: mpv.gamma

            onSliderValueChanged: mpv.gamma = value.toFixed(0)
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        // ------------------------------------
        // SATURATION
        // ------------------------------------
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Saturation")
        }
        ImageAdjustmentSlider {
            id: saturationSlider

            Layout.topMargin: Kirigami.Units.largeSpacing
            value: mpv.saturation

            onSliderValueChanged: mpv.saturation = value.toFixed(0)
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }
        Item {
            height: 1
            width: 1
        }
        Label {
            Layout.topMargin: Kirigami.Units.largeSpacing
            text: qsTr("Middle click on the sliders to reset them")
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }
    }
}
