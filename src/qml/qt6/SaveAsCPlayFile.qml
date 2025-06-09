/*
 * SPDX-FileCopyrightText:
 * 2021-2025 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls

import QtQuick.Dialogs
import Qt.labs.platform 1.0 as Platform

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

Kirigami.ApplicationWindow {
    function updateValues() {
        if (!mpv.playSectionsModel.isEmpty()) {
            mediaFileLabel.text = mpv.playSectionsModel.currentEditItem.mediaFile();
            durationLabel.text = mpv.playSectionsModel.currentEditItem.durationFormatted();
            sectionsLabel.text = qsTr("%1").arg(Number(mpv.playSectionsModel.rowCount()));
            mediaTitleLabel.text = mpv.playSectionsModel.currentEditItem.mediaTitle();
            for (let i = 0; i < stereoscopicModeToSave.count; ++i) {
                if (stereoscopicModeToSave.get(i).value === mpv.playSectionsModel.currentEditItem.stereoVideo()) {
                    stereoscopicModeToSaveComboBox.currentIndex = i;
                    break;
                }
            }
            for (let j = 0; j < gridModeToSave.count; ++j) {
                if (gridModeToSave.get(j).value === mpv.playSectionsModel.currentEditItem.gridToMapOn()) {
                    gridModeToSaveComboBox.currentIndex = j;
                    break;
                }
            }
            separateAudioFileTextField.text = mpv.playSectionsModel.currentEditItem.separateAudioFile();
            separateAudioFileCheckBox.checked = (mpv.playSectionsModel.currentEditItem.separateAudioFile() !== "");
            separateOverlayFileTextField.text = mpv.playSectionsModel.currentEditItem.separateOverlayFile();
            separateOverlayFileCheckBox.checked = (mpv.playSectionsModel.currentEditItem.separateOverlayFile() !== "");
        }
    }

    height: 350
    title: qsTr("Save As C-Play File")
    visible: false
    width: 600

    onVisibilityChanged: {
        if (visible) {
            updateValues();
        }
    }

    Connections {
        function onCurrentEditItemChanged() {
            updateValues();
        }

        target: mpv.playSectionsModel
    }
    Platform.FileDialog {
        id: openSeparateAudioFileDialog

        fileMode: Platform.FileDialog.OpenFile
        title: "Choose Separate Audio File"

        onAccepted: {
            separateAudioFileCheckBox.checked = true;
            var filePath = openSeparateAudioFileDialog.file.toString();
            // remove prefixed "file:///"
            filePath = filePath.replace(/^(file:\/{3})/, "");
            separateAudioFileTextField.text = filePath;
            mpv.focus = true;
        }
        onRejected: mpv.focus = true
    }
    Platform.FileDialog {
        id: openSeparateOverlayFileDialog

        fileMode: Platform.FileDialog.OpenFile
        folder: LocationSettings.cPlayMediaLocation !== "" ? app.pathToUrl(LocationSettings.cPlayMediaLocation) : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        nameFilters: ["Image files (*.png *.jpg *.jpeg *.tga)"]
        title: "Choose Separate Overlay File"

        onAccepted: {
            separateOverlayFileCheckBox.checked = true;
            var filePath = openSeparateOverlayFileDialog.file.toString();
            // remove prefixed "file:///"
            filePath = filePath.replace(/^(file:\/{3})/, "");
            separateOverlayFileTextField.text = filePath;
            mpv.focus = true;
        }
        onRejected: mpv.focus = true
    }
    GridLayout {
        anchors.fill: parent
        anchors.margins: 15
        columnSpacing: 2
        columns: 3
        rowSpacing: 8

        RowLayout {
            Layout.bottomMargin: 5
            Layout.columnSpan: 3

            Rectangle {
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
                width: Kirigami.Units.gridUnit
            }
            Label {
                text: qsTr("Customize properties to save into a C-Play file.")
            }
            Rectangle {
                Layout.fillWidth: true
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }
        Label {
            text: qsTr("File:")
        }
        Label {
            id: mediaFileLabel

            Layout.fillWidth: true

            ToolTip {
                text: qsTr("The actual file to be played.")
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }
        Label {
            text: qsTr("Duration:")
        }
        Label {
            id: durationLabel

            Layout.fillWidth: true

            ToolTip {
                text: qsTr("Duration of the clip")
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }
        Label {
            text: qsTr("Sections:")
        }
        Label {
            id: sectionsLabel

            Layout.fillWidth: true

            ToolTip {
                text: qsTr("Duration of the clip")
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }
        Label {
            text: qsTr("Media title:")
        }
        TextField {
            id: mediaTitleLabel

            Layout.fillWidth: true
            placeholderText: "A title for playlists etc"

            onEditingFinished: {
                if (!mpv.playSectionsModel.isEmpty()) {
                    mpv.playSectionsModel.currentEditItem.setMediaTitle(text);
                }
            }

            ToolTip {
                text: qsTr("A title for playlists etc")
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }
        Label {
            text: qsTr("Stereoscopic mode:")
        }
        ComboBox {
            id: stereoscopicModeToSaveComboBox

            Layout.fillWidth: true
            textRole: "mode"

            model: ListModel {
                id: stereoscopicModeToSave

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

            onActivated: {
                if (!mpv.playSectionsModel.isEmpty()) {
                    mpv.playSectionsModel.currentEditItem.setStereoVideo(model.get(index).value);
                }
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }
        Label {
            text: qsTr("Grid mode:")
        }
        ComboBox {
            id: gridModeToSaveComboBox

            Layout.fillWidth: true
            textRole: "mode"

            model: ListModel {
                id: gridModeToSave

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

            onActivated: {
                if (!mpv.playSectionsModel.isEmpty()) {
                    mpv.playSectionsModel.currentEditItem.setGridToMapOn(model.get(index).value);
                }
            }
        }
        CheckBox {
            id: separateAudioFileCheckBox

            checked: false
            enabled: true
            text: qsTr("")

            onCheckedChanged: separateAudioFileTextField.enabled = checked

            ToolTip {
                text: qsTr("Save with separate audio file:")
            }
        }
        Label {
            text: qsTr("Separate Audio File:")
        }
        RowLayout {
            Layout.fillWidth: true

            TextField {
                id: separateAudioFileTextField

                Layout.fillWidth: true
                enabled: separateAudioFileCheckBox.checked
                readOnly: true

                onEnabledChanged: {
                    if (!mpv.playSectionsModel.isEmpty()) {
                        if (enabled) {
                            mpv.playSectionsModel.currentEditItem.setSeparateAudioFile(text);
                        } else {
                            mpv.playSectionsModel.currentEditItem.setSeparateAudioFile("");
                        }
                    }
                }
                onTextChanged: {
                    if (!mpv.playSectionsModel.isEmpty()) {
                        if (enabled) {
                            mpv.playSectionsModel.currentEditItem.setSeparateAudioFile(text);
                        }
                    }
                }
            }
            ToolButton {
                id: separateAudioFileLoadButton

                focusPolicy: Qt.NoFocus
                icon.height: 16
                icon.name: "system-file-manager"
                text: ""

                onClicked: {
                    if (separateAudioFileTextField.text !== "") {
                        openSeparateAudioFileDialog.folder = app.parentUrl(separateAudioFileTextField.text);
                    } else {
                        openSeparateAudioFileDialog.folder = app.parentUrl(mpv.playSectionsModel.currentEditItem.mediaFile());
                    }
                    openSeparateAudioFileDialog.open();
                }
            }
        }
        CheckBox {
            id: separateOverlayFileCheckBox

            checked: false
            enabled: true
            text: qsTr("")

            onCheckedChanged: separateOverlayFileTextField.enabled = checked

            ToolTip {
                text: qsTr("Save with separate overlay file:")
            }
        }
        Label {
            text: qsTr("Separate Overlay File:")
        }
        RowLayout {
            Layout.fillWidth: true

            TextField {
                id: separateOverlayFileTextField

                Layout.fillWidth: true
                enabled: separateOverlayFileCheckBox.checked
                readOnly: true

                onEnabledChanged: {
                    if (!mpv.playSectionsModel.isEmpty()) {
                        if (enabled) {
                            mpv.playSectionsModel.currentEditItem.setSeparateOverlayFile(text);
                        } else {
                            mpv.playSectionsModel.currentEditItem.setSeparateOverlayFile("");
                        }
                    }
                }
                onTextChanged: {
                    if (!mpv.playSectionsModel.isEmpty()) {
                        if (enabled) {
                            separateOverlayFileCheckBox.checked = true;
                            mpv.playSectionsModel.currentEditItem.setSeparateOverlayFile(text);
                        }
                    }
                }
            }
            ToolButton {
                id: separateOverlayFileLoadButton

                focusPolicy: Qt.NoFocus
                icon.height: 16
                icon.name: "system-file-manager"
                text: ""

                onClicked: {
                    if (separateOverlayFileTextField.text !== "") {
                        openSeparateOverlayFileDialog.folder = app.parentUrl(separateOverlayFileTextField.text);
                    } else {
                        openSeparateOverlayFileDialog.folder = app.parentUrl(mpv.playSectionsModel.currentEditItem.mediaFile());
                    }
                    openSeparateOverlayFileDialog.open();
                }
            }
        }
        Item {
            Layout.columnSpan: 3
            Layout.fillHeight: true
            // spacer item
            Layout.fillWidth: true
        }
        RowLayout {
            Layout.bottomMargin: 5
            Layout.columnSpan: 3

            Button {
                Layout.fillWidth: true
                icon.name: "document-save"
                text: qsTr("Save C-Play file")

                onClicked: saveCPlayFileDialog.open()
            }
            Item {
                Layout.columnSpan: 2
                // spacer item
                Layout.fillWidth: true
            }
        }
    }
}
