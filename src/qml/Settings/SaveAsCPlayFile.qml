/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.12
import QtQuick.Window 2.1
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Dialogs 1.3
import Qt.labs.platform 1.0 as Platform

import org.kde.kirigami 2.11 as Kirigami
import com.georgefb.haruna 1.0
import Haruna.Components 1.0
import mpv 1.0

Kirigami.ApplicationWindow {
    width: 600
    height: 350
    title: qsTr("Save As C-Play File")
    visible: false

    function updateValues() {
        if(!mpv.playSectionsModel.isEmpty()) {
            mediaFileLabel.text = mpv.playSectionsModel.currentEditItem.mediaFile()
            durationLabel.text = mpv.playSectionsModel.currentEditItem.durationFormatted()
            sectionsLabel.text = qsTr("%1").arg(Number(mpv.playSectionsModel.rowCount()))
            mediaTitleLabel.text = mpv.playSectionsModel.currentEditItem.mediaTitle()
            for (let i = 0; i < stereoscopicModeToSave.count; ++i) {
                if (stereoscopicModeToSave.get(i).value === mpv.playSectionsModel.currentEditItem.stereoVideo()) {
                    stereoscopicModeToSaveComboBox.currentIndex = i
                    break
                }
            }
            for (let j = 0; j < gridModeToSave.count; ++j) {
                if (gridModeToSave.get(j).value === mpv.playSectionsModel.currentEditItem.gridToMapOn()) {
                    gridModeToSaveComboBox.currentIndex = j
                    break
                }
            }
            separateAudioFileTextField.text = mpv.playSectionsModel.currentEditItem.separateAudioFile()
            separateAudioFileCheckBox.checked = (mpv.playSectionsModel.currentEditItem.separateAudioFile() !== "")

            separateOverlayFileTextField.text = mpv.playSectionsModel.currentEditItem.separateOverlayFile()
            separateOverlayFileCheckBox.checked = (mpv.playSectionsModel.currentEditItem.separateOverlayFile() !== "")
        }
    }

    Connections {
        target: mpv.playSectionsModel
        function onCurrentEditItemChanged() {
            updateValues()
        }
    }

    onVisibilityChanged: {
        if(visible) {
            updateValues()
        }
    }

    Platform.FileDialog {
        id: openSeparateAudioFileDialog
        title: "Choose Separate Audio File"
        fileMode: Platform.FileDialog.OpenFile

        onAccepted: {
            separateAudioFileCheckBox.checked = true

            var filePath = openSeparateAudioFileDialog.file.toString();
            // remove prefixed "file:///"
            filePath = filePath.replace(/^(file:\/{3})/,"");

            separateAudioFileTextField.text = filePath
            mpv.focus = true
        }
        onRejected: mpv.focus = true
    }

    Platform.FileDialog {
        id: openSeparateOverlayFileDialog
        title: "Choose Separate Overlay File"
        folder: GeneralSettings.cPlayMediaLocation !== ""
                ? app.pathToUrl(GeneralSettings.cPlayMediaLocation)
                : app.pathToUrl(GeneralSettings.fileDialogLastLocation)
        fileMode: Platform.FileDialog.OpenFile
        nameFilters: [ "Image files (*.png *.jpg *.jpeg *.tga)" ]

        onAccepted: {
            separateOverlayFileCheckBox.checked = true

            var filePath = openSeparateOverlayFileDialog.file.toString();
            // remove prefixed "file:///"
            filePath = filePath.replace(/^(file:\/{3})/,"");

            separateOverlayFileTextField.text = filePath
            mpv.focus = true
        }

        onRejected: mpv.focus = true
    }

    GridLayout {
        columnSpacing : 2
        rowSpacing: 8

        anchors.fill: parent
        anchors.margins: 15

        columns: 3

        RowLayout {
            Rectangle {
                width: Kirigami.Units.gridUnit
                height: 1
                color: Kirigami.Theme.alternateBackgroundColor
            }
            Label {
                text: qsTr("Customize properties to save into a C-Play file.")
            }
            Rectangle {
                height: 1
                color: Kirigami.Theme.alternateBackgroundColor
                Layout.fillWidth: true
            }

            Layout.bottomMargin: 5
            Layout.columnSpan: 3
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
            placeholderText: "A title for playlists etc"
            Layout.fillWidth: true
            onEditingFinished: {
                if(!mpv.playSectionsModel.isEmpty()){
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
            textRole: "mode"
            model: ListModel {
                id: stereoscopicModeToSave
                ListElement { mode: "2D (mono)"; value: 0 }
                ListElement { mode: "3D (side-by-side)"; value: 1}
                ListElement { mode: "3D (top-bottom)"; value: 2 }
                ListElement { mode: "3D (top-bottom+flip)"; value: 3 }
            }

            onActivated: {
                if(!mpv.playSectionsModel.isEmpty()){
                    mpv.playSectionsModel.currentEditItem.setStereoVideo(model.get(index).value)
                }
            }

            Layout.fillWidth: true
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
            textRole: "mode"
            model: ListModel {
                id: gridModeToSave
                ListElement { mode: "None/Pre-split"; value: 0 }
                ListElement { mode: "Plane"; value: 1 }
                ListElement { mode: "Dome"; value: 2}
                ListElement { mode: "Sphere EQR"; value: 3 }
                ListElement { mode: "Sphere EAC"; value: 4 }
            }

            onActivated: {
                if(!mpv.playSectionsModel.isEmpty()){
                    mpv.playSectionsModel.currentEditItem.setGridToMapOn(model.get(index).value)
                }
            }

            Layout.fillWidth: true
        }

        CheckBox {
            id: separateAudioFileCheckBox
            text: qsTr("")
            enabled: true
            checked: false
            onCheckedChanged: separateAudioFileTextField.enabled = checked
            ToolTip {
                text: qsTr("Save with separate audio file:")
            }
        }
        Label {
            text: qsTr("Separate Audio File:")
        }
        RowLayout {
            TextField {
                id: separateAudioFileTextField
                onTextChanged: {
                    if(!mpv.playSectionsModel.isEmpty()){
                        if(enabled){
                            mpv.playSectionsModel.currentEditItem.setSeparateAudioFile(text);
                        }
                    }
                }
                onEnabledChanged: {
                    if(!mpv.playSectionsModel.isEmpty()){
                        if(enabled){
                            mpv.playSectionsModel.currentEditItem.setSeparateAudioFile(text);
                        }
                        else {
                            mpv.playSectionsModel.currentEditItem.setSeparateAudioFile("");
                        }
                    }
                }
                enabled: separateAudioFileCheckBox.checked
                readOnly: true
                Layout.fillWidth: true
            }
            ToolButton {
                id: separateAudioFileLoadButton

                text: ""
                icon.name: "system-file-manager"
                icon.height: 16
                focusPolicy: Qt.NoFocus

                onClicked: {
                    if(separateAudioFileTextField.text !== ""){
                        openSeparateAudioFileDialog.folder = app.parentUrl(separateAudioFileTextField.text)
                    }
                    else {
                        openSeparateAudioFileDialog.folder = app.parentUrl(mpv.playSectionsModel.currentEditItem.mediaFile())
                    }
                    openSeparateAudioFileDialog.open()
                }
            }
            Layout.fillWidth: true
        }

        CheckBox {
            id: separateOverlayFileCheckBox
            text: qsTr("")
            enabled: true
            checked: false
            onCheckedChanged: separateOverlayFileTextField.enabled = checked
            ToolTip {
                text: qsTr("Save with separate overlay file:")
            }
        }
        Label {
            text: qsTr("Separate Overlay File:")
        }
        RowLayout {
            TextField {
                id: separateOverlayFileTextField
                onTextChanged: {
                    if(!mpv.playSectionsModel.isEmpty()){
                        if(enabled){
                            separateOverlayFileCheckBox.checked = true;
                            mpv.playSectionsModel.currentEditItem.setSeparateOverlayFile(text);
                        }
                    }
                }
                onEnabledChanged: {
                    if(!mpv.playSectionsModel.isEmpty()){
                        if(enabled){
                            mpv.playSectionsModel.currentEditItem.setSeparateOverlayFile(text);
                        }
                        else {
                            mpv.playSectionsModel.currentEditItem.setSeparateOverlayFile("");
                        }
                    }
                }
                enabled: separateOverlayFileCheckBox.checked
                readOnly: true
                Layout.fillWidth: true
            }
            ToolButton {
                id: separateOverlayFileLoadButton

                text: ""
                icon.name: "system-file-manager"
                icon.height: 16
                focusPolicy: Qt.NoFocus

                onClicked: {
                    if(separateOverlayFileTextField.text !== ""){
                        openSeparateOverlayFileDialog.folder = app.parentUrl(separateOverlayFileTextField.text)
                    }
                    else {
                        openSeparateOverlayFileDialog.folder = app.parentUrl(mpv.playSectionsModel.currentEditItem.mediaFile())
                    }
                    openSeparateOverlayFileDialog.open()
                }
            }
            Layout.fillWidth: true
        }

        Item {
            // spacer item
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.columnSpan: 3
        }

        RowLayout {
            Button {
                text: qsTr("Save C-Play file")
                icon.name: "document-save"
                onClicked: saveCPlayFileDialog.open()
                Layout.fillWidth: true
            }
            Item {
                // spacer item
                Layout.fillWidth: true
                Layout.columnSpan: 2
            }
            Layout.bottomMargin: 5
            Layout.columnSpan: 3
        }
    }
}
