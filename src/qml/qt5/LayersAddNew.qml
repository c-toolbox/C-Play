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
    width: 400
    height: 300
    title: qsTr("Add new layer")
    visible: false
    color: Kirigami.Theme.alternateBackgroundColor

    Component.onCompleted: {
        if(window.x > width) {
            x = window.x - width
        }
        else {
            x = window.x
        }
        y = window.y
    }

    function resetValues() {
        typeComboBox.currentIndex = 0
        fileForLayer.text = "";
        layerTitle.text = "";

        for (let sm = 0; sm < stereoscopicModeForLayerList.count; ++sm) {
            if (stereoscopicModeForLayerList.get(sm).value === LayerSettings.defaultStereoModeForLayers) {
                stereoscopicModeForLayer.currentIndex = sm
                break
            }
        }
        for (let gm = 0; gm < gridModeForLayerList.count; ++gm) {
            if (gridModeForLayerList.get(gm).value === LayerSettings.defaultGridModeForLayers) {
                gridModeForLayer.currentIndex = gm
                break
            }
        }
    }

    onVisibilityChanged: {
        if(visible) {
            resetValues()
        }
    }

    Platform.FileDialog {
        id: fileToLoadAsImageLayerDialog
        folder: LocationSettings.fileDialogLocation !== ""
                ? app.pathToUrl(LocationSettings.fileDialogLocation)
                : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        fileMode: Platform.FileDialog.OpenFile
        title: "Choose image file"
        nameFilters: [ "Image files (*.png *.jpg *.jpeg *.tga)" ]

        onAccepted: {
            fileForLayer.text = playerController.checkAndCorrectPath(fileToLoadAsImageLayerDialog.file);
            layerTitle.text = playerController.returnBaseName(fileForLayer.text);
            mpv.focus = true
        }
        onRejected: mpv.focus = true
    }

    Platform.FileDialog {
        id: fileToLoadAsVideoLayerDialog
        folder: LocationSettings.fileDialogLocation !== ""
                ? app.pathToUrl(LocationSettings.fileDialogLocation)
                : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        title: "Choose video file"
        fileMode: Platform.FileDialog.OpenFile

        onAccepted: {
            fileForLayer.text = playerController.checkAndCorrectPath(fileToLoadAsVideoLayerDialog.file);
            layerTitle.text = playerController.returnBaseName(fileForLayer.text);
            mpv.focus = true
        }
        onRejected: mpv.focus = true
    }

    GridLayout {
        columnSpacing : 2
        rowSpacing: 8

        anchors.fill: parent
        anchors.margins: 15

        columns: 2

        RowLayout {
            Rectangle {
                width: Kirigami.Units.gridUnit
                height: 1
                color: Kirigami.Theme.alternateBackgroundColor
            }
            Label {
                text: qsTr("Properties for the new layer")
            }
            Rectangle {
                height: 1
                color: Kirigami.Theme.alternateBackgroundColor
                Layout.fillWidth: true
            }

            Layout.bottomMargin: 5
            Layout.columnSpan: 2
        }

        Label {
            text: qsTr("Type:")
            Layout.alignment: Qt.AlignRight
            font.pointSize: 9
        }
        ComboBox {
            id: typeComboBox
            model: app.layersModel.layersTypeModel
            textRole: "typeName"

            onActivated: {
            }

            Layout.fillWidth: true
        }

        Label {
            visible: typeComboBox.currentIndex >= 0 && typeComboBox.currentIndex <= 1
            text: qsTr("File:")
            Layout.alignment: Qt.AlignRight
            font.pointSize: 9
        }
        RowLayout {
            visible: typeComboBox.currentIndex >= 0 && typeComboBox.currentIndex <= 1

            TextField {
                id: fileForLayer
                text: ""
                placeholderText: "Path to file"
                Layout.preferredWidth: font.pointSize * 17
                Layout.fillWidth: true
                onEditingFinished: {
                }

                ToolTip {
                    text: qsTr("Path to file for layer")
                }
            }
            ToolButton {
                id: fileToLoadAsLayerButton
                text: ""
                icon.name: "system-file-manager"
                icon.height: 16
                focusPolicy: Qt.NoFocus

                onClicked: {
                    if(typeComboBox.currentIndex == 0)
                        fileToLoadAsImageLayerDialog.open()
                    else if(typeComboBox.currentIndex == 1)
                        fileToLoadAsVideoLayerDialog.open()
                }
            }
            Layout.fillWidth: true
        }

        Label {
            visible: typeComboBox.currentIndex == 2
            text: qsTr("Name:")
            Layout.alignment: Qt.AlignRight
        }
        RowLayout {
            visible: typeComboBox.currentIndex == 2
            ComboBox {
                id: ndiSenderComboBox
                model: app.layersModel.ndiSendersModel
                textRole: "typeName"

                onVisibleChanged: {
                    if (visible)
                        updateSendersBox.clicked()
                }

                onActivated: {
                    layerTitle.text = ndiSenderComboBox.currentText
                }

                Component.onCompleted: {
                }

                Layout.fillWidth: true
            }
            ToolButton {
                id: updateSendersBox
                text: ""
                icon.name: "view-refresh"
                icon.height: 16
                focusPolicy: Qt.NoFocus

                onClicked: {
                    ndiSenderComboBox.currentIndex = app.layersModel.ndiSendersModel.updateSendersList()
                    layerTitle.text = ndiSenderComboBox.currentText
                }
            }
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("Title:")
            Layout.alignment: Qt.AlignRight
            font.pointSize: 9
        }

        TextField {
            id: layerTitle
            text: ""
            placeholderText: "Layer title"
            font.pointSize: 9
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("Stereo:")
            Layout.alignment: Qt.AlignRight
        }
        ComboBox {
            id: stereoscopicModeForLayer
            enabled: true
            focusPolicy: Qt.NoFocus
            textRole: "mode"
            model: ListModel {
                id: stereoscopicModeForLayerList
                ListElement { mode: "2D (mono)"; value: 0 }
                ListElement { mode: "3D (side-by-side)"; value: 1}
                ListElement { mode: "3D (top-bottom)"; value: 2 }
                ListElement { mode: "3D (top-bottom+flip)"; value: 3 }
            }

            onActivated: {
            }

            Component.onCompleted: {
            }

            Layout.fillWidth: true
        }

        Label {
            text: qsTr("Grid:")
            Layout.alignment: Qt.AlignRight
        }
        ComboBox {
            id: gridModeForLayer
            enabled: true
            focusPolicy: Qt.NoFocus
            textRole: "mode"
            model: ListModel {
                id: gridModeForLayerList
                ListElement { mode: "None/Pre-split"; value: 0 }
                ListElement { mode: "Plane"; value: 1 }
                ListElement { mode: "Dome"; value: 2}
                ListElement { mode: "Sphere EQR"; value: 3 }
                ListElement { mode: "Sphere EAC"; value: 4 }
            }

            onActivated: {
            }

            Component.onCompleted: {
            }

            Layout.fillWidth: true
        }

        Item {
            // spacer item
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.columnSpan: 2
        }

        RowLayout {
            Button {
                text: qsTr("Add new layer")
                icon.name: "layer-new"
                onClicked: {
                    if(typeComboBox.currentIndex == 2)
                        layerView.layerItem.layerIdx = app.layersModel.addLayer(layerTitle.text, typeComboBox.currentIndex, ndiSenderComboBox.currentText, stereoscopicModeForLayer.currentIndex, gridModeForLayer.currentIndex)
                    else
                        layerView.layerItem.layerIdx = app.layersModel.addLayer(layerTitle.text, typeComboBox.currentIndex, fileForLayer.text, stereoscopicModeForLayer.currentIndex, gridModeForLayer.currentIndex)

                    layersAddNew.visible = false
                    mpv.focus = true
                }
                ToolTip {
                    text: qsTr("Add layer to bottom of list")
                }
                Layout.fillWidth: true
            }
            Item {
                // spacer item
                Layout.fillWidth: true
            }
            Layout.bottomMargin: 5
            Layout.columnSpan: 2
        }
    }
}
