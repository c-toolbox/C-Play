/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.15
import QtQuick.Window 2.1
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15
import QtQuick.Dialogs 1.3
import Qt.labs.platform 1.0 as Platform

import org.kde.kirigami 2.15 as Kirigami
import org.ctoolbox.cplay 1.0

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
    height: 300
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

        fileMode: Platform.FileDialog.OpenFile
        folder: LocationSettings.fileDialogLocation !== "" ? app.pathToUrl(LocationSettings.fileDialogLocation) : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        nameFilters: ["Image files (*.png *.jpg *.jpeg *.tga)"]
        title: "Choose image file"

        onAccepted: {
            fileForLayer.text = playerController.checkAndCorrectPath(fileToLoadAsImageLayerDialog.file);
            layerTitle.text = playerController.returnBaseName(fileForLayer.text);
            mpv.focus = true;
        }
        onRejected: mpv.focus = true
    }
    Platform.FileDialog {
        id: fileToLoadAsVideoLayerDialog

        fileMode: Platform.FileDialog.OpenFile
        folder: LocationSettings.fileDialogLocation !== "" ? app.pathToUrl(LocationSettings.fileDialogLocation) : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        title: "Choose video file"

        onAccepted: {
            fileForLayer.text = playerController.checkAndCorrectPath(fileToLoadAsVideoLayerDialog.file);
            layerTitle.text = playerController.returnBaseName(fileForLayer.text);
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

            onActivated: {}
        }
        Label {
            Layout.alignment: Qt.AlignRight
            font.pointSize: 9
            text: qsTr("File:")
            visible: typeComboBox.currentIndex >= 0 && typeComboBox.currentIndex <= 1
        }
        RowLayout {
            Layout.fillWidth: true
            visible: typeComboBox.currentIndex >= 0 && typeComboBox.currentIndex <= 1

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
                icon.name: "system-file-manager"
                text: ""

                onClicked: {
                    if (typeComboBox.currentIndex == 0)
                        fileToLoadAsImageLayerDialog.open();
                    else if (typeComboBox.currentIndex == 1)
                        fileToLoadAsVideoLayerDialog.open();
                }
            }
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Name:")
            visible: typeComboBox.currentIndex == 2
        }
        RowLayout {
            Layout.fillWidth: true
            visible: typeComboBox.currentIndex == 2

            ComboBox {
                id: ndiSenderComboBox

                Layout.fillWidth: true
                model: app.slides.selected.ndiSendersModel
                textRole: "typeName"

                Component.onCompleted: {}
                onActivated: {
                    layerTitle.text = ndiSenderComboBox.currentText;
                }
                onVisibleChanged: {
                    if (visible)
                        updateSendersBox.clicked();
                }
            }
            ToolButton {
                id: updateSendersBox

                focusPolicy: Qt.NoFocus
                icon.height: 16
                icon.name: "view-refresh"
                text: ""

                onClicked: {
                    ndiSenderComboBox.currentIndex = app.slides.selected.ndiSendersModel.updateSendersList();
                    layerTitle.text = ndiSenderComboBox.currentText;
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
            maximumLength: 18
            placeholderText: "Layer title"
            text: ""
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Stereo:")
        }
        ComboBox {
            id: stereoscopicModeForLayer

            Layout.fillWidth: true
            enabled: true
            focusPolicy: Qt.NoFocus
            textRole: "mode"

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
        }
        ComboBox {
            id: gridModeForLayer

            Layout.fillWidth: true
            enabled: true
            focusPolicy: Qt.NoFocus
            textRole: "mode"

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
                        if (typeComboBox.currentIndex == 2) {
                            layerView.layerItem.layerIdx = app.slides.selected.addLayer(layerTitle.text, typeComboBox.currentIndex + 1, ndiSenderComboBox.currentText, stereoscopicModeForLayer.currentIndex, gridModeForLayer.currentIndex);
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
