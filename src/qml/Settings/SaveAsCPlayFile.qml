/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.10
import QtQuick.Window 2.1
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12
import QtQuick.Dialogs 1.3

import org.kde.kirigami 2.11 as Kirigami
import com.georgefb.haruna 1.0 as Haruna
import Haruna.Components 1.0

Kirigami.ApplicationWindow {
    width: 600
    height: 350
    title: qsTr("Save As C-Play File")
    visible: false

    onVisibilityChanged: {
        if(visible && !mpv.playSectionsModel.isEmpty()) {
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
            separateAudioFileCheckBox.checked = (mpv.playSectionsModel.currentEditItem.separateAudioFile() !== "")
            separateAudioFileTextField.text = mpv.playSectionsModel.currentEditItem.separateAudioFile()
            separateOverlayFileTextField.text = mpv.playSectionsModel.currentEditItem.separateOverlayFile()

        }
    }

    FileDialog {
        id: openSeparateAudioFileDialog

        //folder: location
        title: "Choose Separate Audio File"

        onAccepted: {
            separateAudioFileCheckBox.checked = true

            var filePath = openSeparateAudioFileDialog.fileUrl.toString();
            // remove prefixed "file:///"
            filePath = filePath.replace(/^(file:\/{3})/,"");

            separateAudioFileTextField.text = filePath
            mpv.focus = true
        }
        onRejected: mpv.focus = true
    }

    FileDialog {
        id: openSeparateOverlayFileDialog

        //folder: location
        title: "Choose Separate Overlay File"

        onAccepted: {
            separateOverlayFileCheckBox.checked = true

            var filePath = openSeparateOverlayFileDialog.fileUrl.toString();
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
            onCheckedChanged: {
            }
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
                        if(mpv.playSectionsModel.currentEditItem.separateAudioFile() !== ""){
                            separateAudioFileCheckBox.checked = true;
                        }
                        else {
                            separateAudioFileCheckBox.checked = false;
                        }

                        if(enabled){
                            mpv.playSectionsModel.currentEditItem.setSeparateAudioFile(text);
                        }
                        else {
                            mpv.playSectionsModel.currentEditItem.setSeparateAudioFile("");
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
                    openSeparateAudioFileDialog.open()
                }
            }
            Layout.fillWidth: true
        }

        CheckBox {
            id: separateOverlayFileCheckBox
            text: qsTr("")
            enabled: true
            checked: {
                if(!mpv.playSectionsModel.isEmpty()){
                    return mpv.playSectionsModel.currentEditItem.separateOverlayFile() !== ""
                }
                else {
                    return false
                }
            }
            onCheckedChanged: {
            }
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
                        if(mpv.playSectionsModel.currentEditItem.separateOverlayFile() !== "")
                            separateOverlayFileCheckBox.checked = true;
                        else
                            separateOverlayFileCheckBox.checked = false;

                        if(enabled){
                            mpv.playSectionsModel.currentEditItem.setSeparateOverlayFile(text);
                        }
                        else {
                            mpv.playSectionsModel.currentEditItem.setSeparateOverlayFile("");
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
