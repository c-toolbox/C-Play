/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.15
import Qt.labs.platform 1.0 as Platform
import QtQuick.Dialogs 1.3

import org.kde.kirigami 2.15 as Kirigami
import org.ctoolbox.cplay 1.0

Rectangle {
    id: layersRoot

    property alias scrollPositionTimer: scrollPositionTimer
    property alias layersView: layersView
    property string position: PlaylistSettings.position
    property int rowHeight: PlaylistSettings.rowHeight
    property int bigFont: PlaylistSettings.bigFontFullscreen

    height: mpv.height
    width: {
            const w = Kirigami.Units.gridUnit * 19
            return (parent.width * 0.17) < w ? w : parent.width * 0.17
    }
    x: position !== "right" ? parent.width : -width
    y: 0
    state: "hidden"
    color: Kirigami.Theme.backgroundColor

    ColumnLayout {
        id: layersHeader
        spacing: 10

        RowLayout {
            spacing: 1
            anchors.rightMargin: Kirigami.Units.largeSpacing
            Layout.preferredWidth: parent.width

            Button {
                icon.name: "layer-new"
                onClicked: {
                    layersAddNew.visible = true
                }
                ToolTip {
                    text: qsTr("Add layer to bottom of list")
                }
            }

            Button {
                icon.name: "layer-delete"
                onClicked: {
                   app.layersModel.removeLayer(layersView.currentIndex)
                }
                ToolTip {
                    text: qsTr("Remove selected layer")
                }
            }
            Button {
                icon.name: "layer-raise"
                onClicked: {
                    app.layersModel.moveLayerUp(layersView.currentIndex)
                }
                ToolTip {
                    text: qsTr("Move selected layer upwards")
                }
            }
            Button {
                icon.name: "layer-lower"
                onClicked: {
                    app.layersModel.moveLayerDown(layersView.currentIndex)
                }
                ToolTip {
                    text: qsTr("Move selected layer downwards")
                }
            }
            Button {
                icon.name: "document-edit-decrypt-verify"
                onClicked: {
                    layerView.visible = true
                }
                ToolTip {
                    text: qsTr("Layer Inspector (showing the selected layer)")
                }
            }
            Button {
                icon.name: "document-open"
                onClicked: {
                }
                ToolTip {
                    text: qsTr("Open layers list")
                }
            }
            Button {
                icon.name: "system-save-session"
                icon.color: app.layersModel.layersNeedsSave ? "orange" : "lime"
                onClicked: {
                }
                ToolTip {
                    text: qsTr("Save layers list")
                }
            }
            Button {
                icon.name: "trash-empty"
                icon.color: "crimson"
                onClicked: {
                    clearLayersDialog.open()
                }
                ToolTip {
                    text: qsTr("Clear layers list")
                }

                MessageDialog {
                    id: clearLayersDialog
                    title: "Clear layers list"
                    text: "Confirm clearing of all items in layers list."
                    standardButtons: StandardButton.Yes | StandardButton.No
                    onAccepted: {
                        app.layersModel.clearLayers();
                    }
                    Component.onCompleted: visible = false
                }
            }
            Layout.columnSpan: 2
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

            onCurrentIndexChanged: {
                layerView.layerItem.layerIdx = layersView.currentIndex
            }
        }

        Connections {
            target: layerView.layerItem
            function onLayerChanged(){
                if(layerView.layerItem.layerIdx !== layersView.currentIndex){
                   layersView.currentIndex = layerView.layerItem.layerIdx
                }
            }
            function onLayerValueChanged(){
                app.layersModel.updateLayer(layerView.layerItem.layerIdx)
            }
        }
    }

    Component {
        id: layersItemCompact
        LayersItemCompact {}
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
