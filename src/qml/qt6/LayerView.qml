/*
 * SPDX-FileCopyrightText:
 * 2024 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Dialogs
import Qt.labs.platform 1.0 as Platform

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

Kirigami.ApplicationWindow {
    id: layerWindow

    property var layerItem: layerViewItem

    color: Kirigami.Theme.alternateBackgroundColor
    height: 600
    title: qsTr("")
    visible: false
    width: 560

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
    }
}
