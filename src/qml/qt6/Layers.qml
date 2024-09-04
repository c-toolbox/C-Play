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
import QtQuick.Dialogs

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
    x: position !== "left" ? parent.width : -width
    y: 0
    z: position === "left" ? 41 : 40
    state: "hidden"
    color: Kirigami.Theme.backgroundColor

    ColumnLayout {
        id: layersHeader
        spacing: 10

        ColumnLayout {
            id: slidesMenu
            spacing: 1

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
                       app.slides.selected.removeLayer(layersView.currentIndex)
                       app.slides.updateSelectedSlide()
                    }
                    ToolTip {
                        text: qsTr("Remove selected layer")
                    }
                }
                Button {
                    icon.name: "layer-top"
                    onClicked: {
                        app.slides.selected.moveLayerTop(layersView.currentIndex)
                    }
                    ToolTip {
                        text: qsTr("Move selected layer to top")
                    }
                }
                Button {
                    icon.name: "layer-raise"
                    onClicked: {
                        app.slides.selected.moveLayerUp(layersView.currentIndex)
                    }
                    ToolTip {
                        text: qsTr("Move selected layer upwards")
                    }
                }
                Button {
                    icon.name: "layer-lower"
                    onClicked: {
                        app.slides.selected.moveLayerDown(layersView.currentIndex)
                    }
                    ToolTip {
                        text: qsTr("Move selected layer downwards")
                    }
                }
                Button {
                    icon.name: "layer-bottom"
                    onClicked: {
                        app.slides.selected.moveLayerBottom(layersView.currentIndex)
                        layersView.currentIndex = layersView.count - 1
                    }
                    ToolTip {
                        text: qsTr("Move selected layer to bottom")
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
                        buttons: MessageDialog.Yes | MessageDialog.No
                        onAccepted: {
                            app.slides.selected.clearLayers()
                            app.slides.updateSelectedSlide()
                        }
                        Component.onCompleted: visible = false
                    }
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
                id: layersTitle
                text: app.slides.selected.layersName + qsTr(" Layers")
                font.pointSize: 9
            }

            Rectangle {
                width: Kirigami.Units.gridUnit + 10
                height: 1
                color: Kirigami.Theme.alternateBackgroundColor
                Layout.fillWidth: true
            }

            Button {
                icon.name: "document-edit-decrypt-verify"
                anchors.right: layersRoot.right
                checkable: true
                checked: layerView.visible
                text: qsTr("Layer View")
                onClicked: {
                    layerView.visible = checked
                }
                ToolTip {
                    text: qsTr("Layer View (showing the selected layer)")
                }
            }
            Layout.preferredWidth: layersRoot.width
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

            model: app.slides.selected
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
                app.slides.selected.updateLayer(layerView.layerItem.layerIdx)
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
            PropertyChanges { target: layersRoot; x: position === "left" ? parent.width : -width }
            PropertyChanges { target: layersRoot; visible: false }
        },
        State {
            name : "visible-without-partner"
            PropertyChanges { target: layersRoot; x: position === "left" ? parent.width - layersRoot.width : 0 }
            PropertyChanges { target: layersRoot; visible: true }
        },
        State {
            name : "visible-with-partner"
            PropertyChanges { target: layersRoot; x: position === "left" ? parent.width - layersRoot.width : layersRoot.width }
            PropertyChanges { target: layersRoot; visible: true }
        }
    ]

    transitions: [
        Transition {
            from: "visible-without-partner"
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
            to: "visible-without-partner"

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
        },
        Transition {
            from: "visible-with-partner"
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
            to: "visible-with-partner"

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
        },
        Transition {
            from: "visible-without-partner"
            to: "visible-with-partner"

            SequentialAnimation {
                NumberAnimation {
                    target: layersRoot
                    property: "x"
                    duration: 120
                    easing.type: Easing.OutQuad
                }
            }
        },
        Transition {
            from: "visible-with-partner"
            to: "visible-without-partner"

            SequentialAnimation {
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
