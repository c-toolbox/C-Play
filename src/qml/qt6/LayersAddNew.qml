/*
 * SPDX-FileCopyrightText:
 * 2024-2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls
import Qt.labs.platform 1.0 as Platform

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

Kirigami.ApplicationWindow {
    id: root

    function resetValues() {
        typeComboBox.currentIndex = 0;
        fileForLayer.text = "";
        layerTitle.text = "";
        for (let sm = 0; sm < stereoscopicModeForLayerList.count; ++sm) {
            if (stereoscopicModeForLayerList.get(sm).value === PresentationSettings.defaultStereoModeForLayers) {
                stereoscopicModeForLayer.currentIndex = sm;
                break;
            }
        }
        for (let gm = 0; gm < gridModeForLayerList.count; ++gm) {
            if (gridModeForLayerList.get(gm).value === PresentationSettings.defaultGridModeForLayers) {
                gridModeForLayer.currentIndex = gm;
                break;
            }
        }
    }

    color: Kirigami.Theme.alternateBackgroundColor
    height: 350
    title: qsTr("Add new layer")
    visible: false
    width: 400

    Component.onCompleted: {
        if (window.x > width) {
            x = window.x - width;
        } else {
            x = window.x;
        }
        y = window.y;
    }
    onVisibilityChanged: {
        if (visible) {
            resetValues();
        }
    }

    Platform.FileDialog {
        id: fileToLoadAsImageLayerDialog
        property bool acceptedOnes: false

        fileMode: Platform.FileDialog.OpenFile
        folder: fileToLoadAsImageLayerDialog.acceptedOnes ? app.pathToUrl(LocationSettings.imageFileDialogLastLocation) : app.pathToUrl(LocationSettings.imageFileDialogLocation)
        nameFilters: ["Image files (*.png *.jpg *.jpeg *.tga)"]
        title: "Choose image file"

        onAccepted: {
            fileForLayer.text = playerController.checkAndCorrectPath(fileToLoadAsImageLayerDialog.file);
            layerTitle.text = playerController.returnBaseName(fileForLayer.text);
            LocationSettings.imageFileDialogLastLocation = app.parentUrl(fileToLoadAsImageLayerDialog.file);
            LocationSettings.save();
            fileToLoadAsImageLayerDialog.acceptedOnes = true;
            mpv.focus = true;
        }
        onRejected: mpv.focus = true
    }
    Platform.FileDialog {
        id: fileToLoadAsPdfLayerDialog
        property bool acceptedOnes: false

        fileMode: Platform.FileDialog.OpenFile
        folder: fileToLoadAsPdfLayerDialog.acceptedOnes ? app.pathToUrl(LocationSettings.pdfFileDialogLastLocation) : app.pathToUrl(LocationSettings.pdfFileDialogLocation)
        nameFilters: ["PDF files (*.pdf)"]
        title: "Choose pdf file"

        onAccepted: {
            fileForLayer.text = playerController.checkAndCorrectPath(fileToLoadAsPdfLayerDialog.file);
            layerTitle.text = playerController.returnBaseName(fileForLayer.text);
            LocationSettings.pdfFileDialogLastLocation = app.parentUrl(fileToLoadAsPdfLayerDialog.file);
            LocationSettings.save();
            fileToLoadAsPdfLayerDialog.acceptedOnes = true;
            mpv.focus = true;
        }
        onRejected: mpv.focus = true
    }
    Platform.FileDialog {
        id: fileToLoadAsVideoLayerDialog
        property bool acceptedOnes: false

        fileMode: Platform.FileDialog.OpenFile
        folder: fileToLoadAsVideoLayerDialog.acceptedOnes ? app.pathToUrl(LocationSettings.videoFileDialogLastLocation) : app.pathToUrl(LocationSettings.videoFileDialogLocation)
        title: "Choose video file"

        onAccepted: {
            fileForLayer.text = playerController.checkAndCorrectPath(fileToLoadAsVideoLayerDialog.file);
            layerTitle.text = playerController.returnBaseName(fileForLayer.text);
            LocationSettings.videoFileDialogLastLocation = app.parentUrl(fileToLoadAsVideoLayerDialog.file);
            LocationSettings.save();
            fileToLoadAsVideoLayerDialog.acceptedOnes = true;
            mpv.focus = true;
        }
        onRejected: mpv.focus = true
    }
    Platform.FileDialog {
        id: fileToLoadAsAudioLayerDialog
        property bool acceptedOnes: false

        fileMode: Platform.FileDialog.OpenFile
        folder: fileToLoadAsAudioLayerDialog.acceptedOnes ? app.pathToUrl(LocationSettings.audioFileDialogLastLocation) : app.pathToUrl(LocationSettings.audioFileDialogLocation)
        title: "Choose audio file"

        onAccepted: {
            fileForLayer.text = playerController.checkAndCorrectPath(fileToLoadAsAudioLayerDialog.file);
            layerTitle.text = playerController.returnBaseName(fileForLayer.text);
            LocationSettings.audioFileDialogLastLocation = app.parentUrl(fileToLoadAsAudioLayerDialog.file);
            LocationSettings.save();
            fileToLoadAsAudioLayerDialog.acceptedOnes = true;
            mpv.focus = true;
        }
        onRejected: mpv.focus = true
    }
    GridLayout {
        anchors.fill: parent
        anchors.margins: 15
        columnSpacing: 2
        columns: 2
        rowSpacing: 8

        RowLayout {
            Layout.bottomMargin: 5
            Layout.columnSpan: 2

            Rectangle {
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
                width: Kirigami.Units.gridUnit
            }
            Label {
                text: qsTr("Properties for the new layer")
            }
            Rectangle {
                Layout.fillWidth: true
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
            }
        }
        Label {
            Layout.alignment: Qt.AlignRight
            font.pointSize: 9
            text: qsTr("Type:")
        }
        ComboBox {
            id: typeComboBox

            Layout.fillWidth: true
            model: app.slides.selected.layersTypeModel
            textRole: "typeName"

            onActivated: {
                if (typeComboBox.currentText === "NDI") {
                    app.ndiSendersModel.updateSendersList();
                    ndiSenderComboBox.currentIndex = app.ndiSendersModel.numberOfSenders - 1;
                    layerTitle.text = ndiSenderComboBox.currentText;
                }
                else if (typeComboBox.currentText === "Spout") {
                    app.spoutSendersModel.updateSendersList();
                    spoutSenderComboBox.currentIndex = app.spoutSendersModel.numberOfSenders - 1;
                    layerTitle.text = spoutSenderComboBox.currentText;
                }
                else if (typeComboBox.currentText === "Stream") {
                    app.streamsModel.updateStreamsList();
                    streamsLayout.customEntry = false;
                    streamsComboBox.currentIndex = 0;
                    streamCustomEntryField.text = "";
                    layerTitle.text = streamsComboBox.currentText;
                }
                else {
                    layerTitle.text = "";
                }
            }
        }
        Label {
            Layout.alignment: Qt.AlignRight
            font.pointSize: 9
            text: qsTr("File:")
            visible: typeComboBox.currentText != "Stream" && typeComboBox.currentText != "NDI" && typeComboBox.currentText != "Spout"
        }
        RowLayout {
            Layout.fillWidth: true
            visible: typeComboBox.currentText != "Stream" && typeComboBox.currentText != "NDI" && typeComboBox.currentText != "Spout"

            TextField {
                id: fileForLayer

                Layout.fillWidth: true
                Layout.preferredWidth: font.pointSize * 17
                placeholderText: "Path to file"
                text: ""

                onEditingFinished: {}

                ToolTip {
                    text: qsTr("Path to file for layer")
                }
            }
            ToolButton {
                id: fileToLoadAsLayerButton

                focusPolicy: Qt.NoFocus
                icon.height: 16
                icon.name: "document-open"
                text: ""

                onClicked: {
                    if (typeComboBox.currentText === "Image")
                        fileToLoadAsImageLayerDialog.open();
                    else if (typeComboBox.currentText === "PDF")
                        fileToLoadAsPdfLayerDialog.open();
                    else if (typeComboBox.currentText === "Video")
                        fileToLoadAsVideoLayerDialog.open();
                    else if (typeComboBox.currentText === "Audio")
                        fileToLoadAsAudioLayerDialog.open();
                }
            }
        }

        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Path:")
            visible: typeComboBox.currentText === "Stream"
        }
        RowLayout {
            id: streamsLayout
            Layout.fillWidth: true
            visible: typeComboBox.currentText === "Stream"
            property bool customEntry: false

            ComboBox {
                id: streamsComboBox

                Layout.fillWidth: true
                model: app.streamsModel
                currentIndex: 0
                textRole: "title"
                valueRole: "path"
                visible: streamsLayout.customEntry === false

                Component.onCompleted: {
                    app.streamsModel.updateStreamsList();
                    streamsLayout.customEntry = false;
                    streamsComboBox.currentIndex = 0;
                    layerTitle.text = streamsComboBox.currentText;
                }
                onActivated: {
                    layerTitle.text = streamsComboBox.currentText;
                }
            }
            TextField {
                id: streamCustomEntryField

                Layout.fillWidth: true
                Layout.preferredWidth: font.pointSize * 17
                placeholderText: "Stream path (see mpv docs).."
                text: ""
                visible: streamsLayout.customEntry === true

                onEditingFinished: {}

                ToolTip {
                    text: qsTr("Stream path (see mpv docs)..")
                }
            }
            ToolButton {
                id: streamComboOrFieldButton

                focusPolicy: Qt.NoFocus
                icon.height: 16
                icon.name: streamsLayout.customEntry ? "gnumeric-object-combo" : "text-field"
                text: ""

                onClicked: {
                    if(streamsLayout.customEntry) {
                        app.streamsModel.updateStreamsList();
                        streamsComboBox.currentIndex = 0;
                        streamsLayout.customEntry = false;
                        layerTitle.text = streamsComboBox.currentText;
                    }
                    else {
                        streamsLayout.customEntry = true;
                        layerTitle.text = ""
                    }

                }

                ToolTip {
                    text: streamsLayout.customEntry ? qsTr("Use predefined stream list") : qsTr("Use custom stream path")
                }
            }
        }

        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Name:")
            visible: typeComboBox.currentText === "NDI" || typeComboBox.currentText === "Spout"
        }
        RowLayout {
            Layout.fillWidth: true
            visible: typeComboBox.currentText === "NDI"

            ComboBox {
                id: ndiSenderComboBox

                Layout.fillWidth: true
                model: app.ndiSendersModel
                currentIndex: (app.ndiSendersModel ? app.ndiSendersModel.numberOfSenders - 1 : -1)
                textRole: "typeName"

                Component.onCompleted: {
                    if(app.ndiSendersModel){
                        app.ndiSendersModel.updateSendersList();
                        ndiSenderComboBox.currentIndex = app.ndiSendersModel.numberOfSenders - 1;
                        layerTitle.text = ndiSenderComboBox.currentText;
                    }
                }
                onActivated: {
                    layerTitle.text = ndiSenderComboBox.currentText;
                }
            }
            ToolButton {
                id: updateNdiSendersBox

                focusPolicy: Qt.NoFocus
                icon.height: 16
                icon.name: "view-refresh"
                text: ""

                onClicked: {
                    if(app.ndiSendersModel){
                        app.ndiSendersModel.updateSendersList();
                        ndiSenderComboBox.currentIndex = app.ndiSendersModel.numberOfSenders - 1;
                        layerTitle.text = ndiSenderComboBox.currentText;
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            visible: typeComboBox.currentText === "Spout"

            ComboBox {
                id: spoutSenderComboBox

                Layout.fillWidth: true
                model: app.spoutSendersModel
                currentIndex: (app.spoutSendersModel ? app.spoutSendersModel.numberOfSenders - 1 : -1)
                textRole: "typeName"

                Component.onCompleted: {
                    if(app.spoutSendersModel){
                        app.spoutSendersModel.updateSendersList();
                        spoutSenderComboBox.currentIndex = app.spoutSendersModel.numberOfSenders - 1;
                        layerTitle.text = spoutSenderComboBox.currentText;
                    }
                }
                onActivated: {
                    layerTitle.text = spoutSenderComboBox.currentText;
                }
            }
            ToolButton {
                id: updateSpoutSendersBox

                focusPolicy: Qt.NoFocus
                icon.height: 16
                icon.name: "view-refresh"
                text: ""

                onClicked: {
                    if(app.spoutSendersModel){
                        app.spoutSendersModel.updateSendersList();
                        spoutSenderComboBox.currentIndex = app.spoutSendersModel.numberOfSenders - 1;
                        layerTitle.text = spoutSenderComboBox.currentText;
                    }
                }
            }
        }
        Label {
            Layout.alignment: Qt.AlignRight
            font.pointSize: 9
            text: qsTr("Title:")
        }
        TextField {
            id: layerTitle

            Layout.fillWidth: true
            font.pointSize: 9
            maximumLength: 30
            placeholderText: "Layer title"
            text: ""
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Stereo:")
            enabled: typeComboBox.currentText !== "Audio"
        }
        ComboBox {
            id: stereoscopicModeForLayer

            Layout.fillWidth: true
            focusPolicy: Qt.NoFocus
            textRole: "mode"
            enabled: typeComboBox.currentText !== "Audio"

            model: ListModel {
                id: stereoscopicModeForLayerList

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

            Component.onCompleted: {}
            onActivated: {}
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Grid:")
            enabled: typeComboBox.currentText !== "Audio"
        }
        ComboBox {
            id: gridModeForLayer

            Layout.fillWidth: true
            focusPolicy: Qt.NoFocus
            textRole: "mode"
            enabled: typeComboBox.currentText !== "Audio"

            model: ListModel {
                id: gridModeForLayerList

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

            Component.onCompleted: {}
            onActivated: {}
        }
        Item {
            Layout.columnSpan: 2
            Layout.fillHeight: true
            // spacer item
            Layout.fillWidth: true
        }
        RowLayout {
            Layout.bottomMargin: 5
            Layout.columnSpan: 2

            Button {
                Layout.fillWidth: true
                icon.name: "layer-new"
                text: qsTr("Add new layer")

                onClicked: {
                    if (layerTitle.text !== "") {
                        if (typeComboBox.currentText === "NDI") {
                            layerView.layerItem.layerIdx = app.slides.selected.addLayer(layerTitle.text, typeComboBox.currentIndex + 1, ndiSenderComboBox.currentText, stereoscopicModeForLayer.currentIndex, gridModeForLayer.currentIndex);
                            layersAddNew.visible = false;
                            app.slides.updateSelectedSlide();
                            mpv.focus = true;
                        } else if (typeComboBox.currentText === "Spout") {
                            layerView.layerItem.layerIdx = app.slides.selected.addLayer(layerTitle.text, typeComboBox.currentIndex + 1, spoutSenderComboBox.currentText, stereoscopicModeForLayer.currentIndex, gridModeForLayer.currentIndex);
                            layersAddNew.visible = false;
                            app.slides.updateSelectedSlide();
                            mpv.focus = true;
                        } else if (typeComboBox.currentText === "Stream") {
                            if(streamsLayout.customEntry){
                                layerView.layerItem.layerIdx = app.slides.selected.addLayer(layerTitle.text, typeComboBox.currentIndex + 1, streamCustomEntryField.text, stereoscopicModeForLayer.currentIndex, gridModeForLayer.currentIndex);
                            }
                            else {
                                layerView.layerItem.layerIdx = app.slides.selected.addLayer(layerTitle.text, typeComboBox.currentIndex + 1, streamsComboBox.currentValue, stereoscopicModeForLayer.currentIndex, gridModeForLayer.currentIndex);
                            }
                            layersAddNew.visible = false;
                            app.slides.updateSelectedSlide();
                            mpv.focus = true;
                        } else if (fileForLayer.text !== "") {
                            layerView.layerItem.layerIdx = app.slides.selected.addLayer(layerTitle.text, typeComboBox.currentIndex + 1, fileForLayer.text, stereoscopicModeForLayer.currentIndex, gridModeForLayer.currentIndex);
                            layersAddNew.visible = false;
                            app.slides.updateSelectedSlide();
                            mpv.focus = true;
                        }
                    }
                }

                ToolTip {
                    text: qsTr("Add layer to bottom of list")
                }
            }
            Item {
                // spacer item
                Layout.fillWidth: true
            }
        }
    }
}
