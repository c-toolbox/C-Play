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

    property int bigFont: PlaylistSettings.bigFontFullscreen
    property alias layersView: layersView
    property string position: PlaylistSettings.position
    property int rowHeight: PlaylistSettings.rowHeight
    property alias scrollPositionTimer: scrollPositionTimer

    color: Kirigami.Theme.backgroundColor
    height: mpv.height
    state: "hidden"
    width: {
        const w = Kirigami.Units.gridUnit * 15;
        return (parent.width * 0.13) < w ? w : parent.width * 0.13;
    }
    x: position !== "left" ? parent.width : -width
    y: 0
    z: position === "left" ? 41 : 40

    states: [
        State {
            name: "hidden"

            PropertyChanges {
                target: layersRoot
                x: position === "left" ? parent.width : -width
            }
            PropertyChanges {
                target: layersRoot
                visible: false
            }
        },
        State {
            name: "visible-without-partner"

            PropertyChanges {
                target: layersRoot
                x: position === "left" ? parent.width - layersRoot.width : 0
            }
            PropertyChanges {
                target: layersRoot
                visible: true
            }
        },
        State {
            name: "visible-with-partner"

            PropertyChanges {
                target: layersRoot
                x: position === "left" ? parent.width - layersRoot.width : layersRoot.width
            }
            PropertyChanges {
                target: layersRoot
                visible: true
            }
        }
    ]
    transitions: [
        Transition {
            from: "visible-without-partner"
            to: "hidden"

            SequentialAnimation {
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.InQuad
                    property: "x"
                    target: layersRoot
                }
                PropertyAction {
                    property: "visible"
                    target: layersRoot
                    value: false
                }
            }
        },
        Transition {
            from: "hidden"
            to: "visible-without-partner"

            SequentialAnimation {
                PropertyAction {
                    property: "visible"
                    target: layersRoot
                    value: true
                }
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.OutQuad
                    property: "x"
                    target: layersRoot
                }
            }
        },
        Transition {
            from: "visible-with-partner"
            to: "hidden"

            SequentialAnimation {
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.InQuad
                    property: "x"
                    target: layersRoot
                }
                PropertyAction {
                    property: "visible"
                    target: layersRoot
                    value: false
                }
            }
        },
        Transition {
            from: "hidden"
            to: "visible-with-partner"

            SequentialAnimation {
                PropertyAction {
                    property: "visible"
                    target: layersRoot
                    value: true
                }
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.OutQuad
                    property: "x"
                    target: layersRoot
                }
            }
        },
        Transition {
            from: "visible-without-partner"
            to: "visible-with-partner"

            SequentialAnimation {
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.OutQuad
                    property: "x"
                    target: layersRoot
                }
            }
        },
        Transition {
            from: "visible-with-partner"
            to: "visible-without-partner"

            SequentialAnimation {
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.OutQuad
                    property: "x"
                    target: layersRoot
                }
            }
        }
    ]

    ColumnLayout {
        id: layersHeader

        spacing: 10

        ColumnLayout {
            id: slidesMenu

            spacing: 1

            RowLayout {
                Layout.preferredWidth: parent.width
                anchors.rightMargin: Kirigami.Units.largeSpacing
                spacing: 1

                Button {
                    icon.name: "layer-new"

                    onClicked: {
                        layersAddNew.visible = true;
                    }

                    ToolTip {
                        text: qsTr("Add layer to bottom of list")
                    }
                }
                Button {
                    icon.name: "layer-delete"

                    onClicked: {
                        app.slides.selected.removeLayer(layersView.currentIndex);
                        app.slides.updateSelectedSlide();
                    }

                    ToolTip {
                        text: qsTr("Remove selected layer")
                    }
                }
                Button {
                    icon.name: "layer-top"

                    onClicked: {
                        app.slides.selected.moveLayerTop(layersView.currentIndex);
                    }

                    ToolTip {
                        text: qsTr("Move selected layer to top")
                    }
                }
                Button {
                    icon.name: "layer-raise"

                    onClicked: {
                        app.slides.selected.moveLayerUp(layersView.currentIndex);
                    }

                    ToolTip {
                        text: qsTr("Move selected layer upwards")
                    }
                }
                Button {
                    icon.name: "layer-lower"

                    onClicked: {
                        app.slides.selected.moveLayerDown(layersView.currentIndex);
                    }

                    ToolTip {
                        text: qsTr("Move selected layer downwards")
                    }
                }
                Button {
                    icon.name: "layer-bottom"

                    onClicked: {
                        app.slides.selected.moveLayerBottom(layersView.currentIndex);
                        layersView.currentIndex = layersView.count - 1;
                    }

                    ToolTip {
                        text: qsTr("Move selected layer to bottom")
                    }
                }
                Button {
                    icon.color: "crimson"
                    icon.name: "trash-empty"

                    onClicked: {
                        clearLayersDialog.open();
                    }

                    ToolTip {
                        text: qsTr("Clear layers list")
                    }
                    MessageDialog {
                        id: clearLayersDialog

                        buttons: MessageDialog.Yes | MessageDialog.No
                        text: "Confirm clearing of all items in layers list."
                        title: "Clear layers list"

                        Component.onCompleted: visible = false
                        onAccepted: {
                            app.slides.selected.clearLayers();
                            app.slides.updateSelectedSlide();
                        }
                    }
                }
            }
        }
        Item {
            Layout.fillHeight: true
            // spacer item
            Layout.fillWidth: true
        }
        RowLayout {
            Layout.preferredWidth: layersRoot.width

            Rectangle {
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
                width: Kirigami.Units.gridUnit + 10
            }
            Label {
                id: layersTitle

                font.pointSize: 9
                text: app.slides.selected.layersName + qsTr(" Layers")
            }
            Rectangle {
                Layout.fillWidth: true
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
                width: Kirigami.Units.gridUnit + 10
            }
            Button {
                anchors.right: layersRoot.right
                checkable: true
                checked: layerView.visible
                icon.name: "document-edit-decrypt-verify"
                text: qsTr("Layer View")

                onClicked: {
                    layerView.visible = checked;
                }

                ToolTip {
                    text: qsTr("Layer View (showing the selected layer)")
                }
            }
        }
    }
    ScrollView {
        id: layersScrollView

        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        anchors.fill: parent
        anchors.topMargin: layersHeader.height + 5
        z: 20

        ListView {
            id: layersView

            delegate: layersItemCompact
            model: app.slides.selected
            spacing: 1

            onCurrentIndexChanged: {
                layerView.layerItem.layerIdx = layersView.currentIndex;
            }
        }
        Connections {
            function onLayerChanged() {
                if (layerView.layerItem.layerIdx !== layersView.currentIndex) {
                    layersView.currentIndex = layerView.layerItem.layerIdx;
                }
            }
            function onLayerValueChanged() {
                app.slides.selected.updateLayer(layerView.layerItem.layerIdx);
            }

            target: layerView.layerItem
        }
    }
    Component {
        id: layersItemCompact

        LayersItemCompact {
        }
    }
    Timer {
        id: scrollPositionTimer

        interval: 50
        repeat: true
        running: true

        onTriggered: {
            scrollPositionTimer.stop();
        }
    }
}
