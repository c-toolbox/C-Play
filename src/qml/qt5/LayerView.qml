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
    id: layerWindow

    property var layerItem: layerViewItem
    property var selection: undefined

    color: Kirigami.Theme.alternateBackgroundColor
    height: 600
    title: qsTr("")
    visible: false
    width: 550

    Component.onCompleted: {
        if (window.x > width) {
            x = window.x - width;
        } else {
            x = Screen.width / 2 - width;
        }
        y = Screen.height / 2 - height / 2;
    }

    ToolBar {
        id: toolBarLayerView

        visible: layers.layersView.currentIndex !== -1

        RowLayout {
            id: layerHeaderRow

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
                onActivated: {
                    layerView.layerItem.layerStereoMode = model.get(index).value;
                }
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
                onActivated: {
                    layerView.layerItem.layerGridMode = model.get(index).value;
                }
            }
            PropertyAnimation {
                id: visibility_fade_out_animation

                duration: PlaybackSettings.fadeDuration
                property: "value"
                target: visibilitySlider
                to: 0
            }
            ToolButton {
                id: fade_image_out

                enabled: layerView.layerItem.layerVisibility !== 0
                focusPolicy: Qt.NoFocus
                icon.name: "view-hidden"

                onClicked: {
                    if (!visibility_fade_out_animation.running) {
                        visibility_fade_out_animation.start();
                    }
                }
            }
            VisibilitySlider {
                id: visibilitySlider

                overlayLabel: qsTr("Layer visibility: ")

                onValueChanged: {
                    if (value.toFixed(0) !== layerView.layerItem.layerVisibility) {
                        layerView.layerItem.layerVisibility = value.toFixed(0);
                    }
                }
            }
            PropertyAnimation {
                id: visibility_fade_in_animation

                duration: PlaybackSettings.fadeDuration
                property: "value"
                target: visibilitySlider
                to: 100
            }
            ToolButton {
                id: fade_image_in

                enabled: layerView.layerItem.layerVisibility !== 100
                focusPolicy: Qt.NoFocus
                icon.name: "view-visible"

                onClicked: {
                    if (!visibility_fade_in_animation.running) {
                        visibility_fade_in_animation.start();
                    }
                }
            }
            Connections {
                function onLayerChanged() {
                    layerWindow.title = layerView.layerItem.layerTitle;
                    visibilitySlider.value = layerView.layerItem.layerVisibility;
                    for (let sm = 0; sm < stereoscopicModeForLayerList.count; ++sm) {
                        if (stereoscopicModeForLayerList.get(sm).value === layerView.layerItem.layerStereoMode) {
                            stereoscopicModeForLayer.currentIndex = sm;
                            break;
                        }
                    }
                    for (let gm = 0; gm < gridModeForLayerList.count; ++gm) {
                        if (gridModeForLayerList.get(gm).value === layerView.layerItem.layerGridMode) {
                            gridModeForLayer.currentIndex = gm;
                            break;
                        }
                    }
                }
                function onLayerValueChanged() {
                    if (visibilitySlider.value !== layerView.layerItem.layerVisibility)
                        visibilitySlider.value = layerView.layerItem.layerVisibility;
                }

                target: layerView.layerItem
            }
        }
    }
    LayerQtItem {
        id: layerViewItem

        anchors.top: toolBarLayerView.bottom
        height: parent.height - toolBarLayerView.height
        width: parent.width

        Label {
            id: noLayerLabel

            anchors.fill: parent
            font.family: "Helvetica"
            font.pointSize: 18
            horizontalAlignment: Text.AlignHCenter
            text: "Select a layer in the list to view it here..."
            verticalAlignment: Text.AlignVCenter
            visible: layers.layersView.currentIndex === -1
            wrapMode: Text.WordWrap
        }
        MouseArea {
            anchors.fill: parent
            visible: layers.layersView.currentIndex !== -1

            onClicked: {
                if (!selection)
                    selection = selectionComponent.createObject(parent, {
                        "x": parent.width / 4,
                        "y": parent.height / 4,
                        "width": parent.width / 2,
                        "height": parent.width / 2
                    });
            }
        }
    }
    Component {
        id: selectionComponent

        Rectangle {
            id: selComp

            property int rulersSize: 18

            color: "#354682B4"

            border {
                color: "steelblue"
                width: 2
            }
            MouseArea {
                // drag mouse area
                anchors.fill: parent

                onDoubleClicked: {
                    parent.destroy();        // destroy component
                }

                drag {
                    maximumX: parent.width
                    maximumY: parent.height
                    minimumX: 0
                    minimumY: 0
                    smoothed: true
                    target: layerViewItem
                }
            }
            Rectangle {
                anchors.horizontalCenter: parent.left
                anchors.verticalCenter: parent.verticalCenter
                color: "steelblue"
                height: rulersSize
                radius: rulersSize
                width: rulersSize

                MouseArea {
                    anchors.fill: parent

                    onMouseXChanged: {
                        if (drag.active) {
                            selComp.width = selComp.width - mouseX;
                            selComp.x = selComp.x + mouseX;
                            if (selComp.width < 30)
                                selComp.width = 30;
                        }
                    }

                    drag {
                        axis: Drag.XAxis
                        target: parent
                    }
                }
            }
            Rectangle {
                anchors.horizontalCenter: parent.right
                anchors.verticalCenter: parent.verticalCenter
                color: "steelblue"
                height: rulersSize
                radius: rulersSize
                width: rulersSize

                MouseArea {
                    anchors.fill: parent

                    onMouseXChanged: {
                        if (drag.active) {
                            selComp.width = selComp.width + mouseX;
                            if (selComp.width < 50)
                                selComp.width = 50;
                        }
                    }

                    drag {
                        axis: Drag.XAxis
                        target: parent
                    }
                }
            }
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.top
                color: "steelblue"
                height: rulersSize
                radius: rulersSize
                width: rulersSize
                x: parent.x / 2
                y: 0

                MouseArea {
                    anchors.fill: parent

                    onMouseYChanged: {
                        if (drag.active) {
                            selComp.height = selComp.height - mouseY;
                            selComp.y = selComp.y + mouseY;
                            if (selComp.height < 50)
                                selComp.height = 50;
                        }
                    }

                    drag {
                        axis: Drag.YAxis
                        target: parent
                    }
                }
            }
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.bottom
                color: "steelblue"
                height: rulersSize
                radius: rulersSize
                width: rulersSize
                x: parent.x / 2
                y: parent.y

                MouseArea {
                    anchors.fill: parent

                    onMouseYChanged: {
                        if (drag.active) {
                            selComp.height = selComp.height + mouseY;
                            if (selComp.height < 50)
                                selComp.height = 50;
                        }
                    }

                    drag {
                        axis: Drag.YAxis
                        target: parent
                    }
                }
            }
        }
    }
}
