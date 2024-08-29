/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform 1.0 as Platform

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

Rectangle {
    id: layersRoot

    property alias scrollPositionTimer: scrollPositionTimer
    property alias layersView: layersView
    property string position: PlaylistSettings.position
    property int rowHeight: PlaylistSettings.rowHeight
    property int bigFont: PlaylistSettings.bigFontFullscreen

    height: mpv.height
    width: {
            const w = Kirigami.Units.gridUnit * 15
            return (parent.width * 0.13) < w ? w : parent.width * 0.13
    }
    x: position !== "right" ? parent.width : -width
    y: 0
    state: "hidden"
    color: Kirigami.Theme.backgroundColor

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

    ColumnLayout {
        id: layersHeader
        spacing: 10

        Frame {
            id: addLayersFrame

            GridLayout {
                id: layersBuilder
                columns: 2
                Layout.preferredWidth: parent.width
                rowSpacing: 1

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
                        implicitWidth: layersRoot.width * 0.6
                        model: app.layersModel.ndiSendersModel
                        textRole: "typeName"

                        onVisibleChanged: {
                            if (visible)
                                updateSendersBox.clicked()
                        }

                        onActivated: {
                        }

                        Component.onCompleted: {
                        }
                    }
                    ToolButton {
                        id: updateSendersBox
                        text: ""
                        icon.name: "view-refresh"
                        icon.height: 16
                        focusPolicy: Qt.NoFocus

                        onClicked: {
                            ndiSenderComboBox.currentIndex = app.layersModel.ndiSendersModel.updateSendersList()
                        }
                    }
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
                    maximumLength: 20
                    Layout.preferredWidth: font.pointSize * 17
                    font.pointSize: 9
                    Layout.fillWidth: true
                }

                RowLayout {
                    Button {
                        icon.name: "list-add"
                        enabled: true
                        focus: true
                        onClicked: {
                            if(typeComboBox.currentIndex == 2)
                                app.layersModel.addLayer(layerTitle.text, typeComboBox.currentIndex, ndiSenderComboBox.currentText)
                            else
                                app.layersModel.addLayer(layerTitle.text, typeComboBox.currentIndex, fileForLayer.text)
                        }
                        ToolTip {
                            text: qsTr("Add layer to bottom of list")
                        }
                    }

                    Button {
                        icon.name: "list-remove"
                        onClicked: {
                           app.layersModel.removeLayer(layersView.currentIndex)
                        }
                        ToolTip {
                            text: qsTr("Remove selected layer")
                        }
                    }
                    Button {
                        icon.name: "document-replace"
                        onClicked: {
                            //mpv.playSectionsModel.replaceSection(layersView.currentIndex, sectionTitle.text, startTimeTextField.text, endTimeTextField.text, eosComboBox.currentIndex)
                        }
                        ToolTip {
                            text: qsTr("Replace selected layer")
                        }
                    }
                    Button {
                        icon.name: "edit-entry"
                        onClicked: {
                            //sectionTitle.text = mpv.playSectionsModel.sectionTitle(layersView.currentIndex)
                            //startTimeTextField.text = app.formatTime(mpv.playSectionsModel.sectionStartTime(layersView.currentIndex))
                            //endTimeTextField.text = app.formatTime(mpv.playSectionsModel.sectionEndTime(layersView.currentIndex))
                            //eosComboBox.currentIndex = mpv.playSectionsModel.sectionEOSMode(layersView.currentIndex)
                        }
                        ToolTip {
                            text: qsTr("Copy values from selected layer")
                        }
                    }
                    Button {
                        icon.name: "pan-up-symbolic"
                        onClicked: {
                            app.layersModel.moveLayerUp(layersView.currentIndex)
                        }
                        ToolTip {
                            text: qsTr("Move selected layer upwards")
                        }
                    }
                    Button {
                        icon.name: "pan-down-symbolic"
                        onClicked: {
                            app.layersModel.moveLayerDown(layersView.currentIndex)
                        }
                        ToolTip {
                            text: qsTr("Move selected layer downwards")
                        }
                    }
                    Layout.columnSpan: 2
                }
            }
        }

        Item {
            // spacer item
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        RowLayout {
            Rectangle {
                width: Kirigami.Units.gridUnit + 10
                height: 1
                color: Kirigami.Theme.alternateBackgroundColor
            }

            Label {
                id: mediaTitle
                text: qsTr("Layers")
                font.pointSize: 9
            }

            Rectangle {
                width: Kirigami.Units.gridUnit + 10
                height: 1
                color: Kirigami.Theme.alternateBackgroundColor
                Layout.fillWidth: true
            }
        }
    }

    ScrollView {
        id: layersScrollView

        z: 20
        anchors.fill: parent
        anchors.topMargin: layersHeader.height + 5
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ListView {
            id: layersView

            model: app.layersModel
            spacing: 1
            delegate: layersItemCompact
        }

        Connections {
            target: layerView.layerItem
            function onLayerValueChanged(){
                app.layersModel.updateLayer(layerView.layerItem.layerIdx)
            }
        }
    }

    Component {
        id: layersItemCompact
        LayersItemCompact {
            highlighted: layersView.currentIndex === index
            onClicked: {
                layersView.currentIndex = index
            }
        }
    }

    Timer {
        id: scrollPositionTimer
        interval: 50; running: true; repeat: true

        onTriggered: {
            scrollPositionTimer.stop()
        }
    }

    states: [
        State {
            name: "hidden"
            PropertyChanges { target: layersRoot; x: position !== "right" ? parent.width : -width }
            PropertyChanges { target: layersRoot; visible: false }
        },
        State {
            name : "visible"
            PropertyChanges { target: layersRoot; x: position !== "right" ? parent.width - layersRoot.width : 0 }
            PropertyChanges { target: layersRoot; visible: true }
        }
    ]

    transitions: [
        Transition {
            from: "visible"
            to: "hidden"

            SequentialAnimation {
                NumberAnimation {
                    target: layersRoot
                    property: "x"
                    duration: 120
                    easing.type: Easing.InQuad
                }
                PropertyAction {
                    target: layersRoot
                    property: "visible"
                    value: false
                }
            }
        },
        Transition {
            from: "hidden"
            to: "visible"

            SequentialAnimation {
                PropertyAction {
                    target: layersRoot
                    property: "visible"
                    value: true
                }
                NumberAnimation {
                    target: layersRoot
                    property: "x"
                    duration: 120
                    easing.type: Easing.OutQuad
                }
            }
        }
    ]

}
