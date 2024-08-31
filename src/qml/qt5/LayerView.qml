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
    width: 550
    height: 600
    title: qsTr("")
    visible: false
    color: Kirigami.Theme.alternateBackgroundColor

    Component.onCompleted: {
        if(window.x > width) {
            x = window.x - width
        }
        else {
            x = Screen.width / 2 - width
        }
        y = Screen.height / 2 - height / 2
    }

    property var layerItem : layerViewItem

    ToolBar {
        id: toolBarLayerView
        visible: layers.layersView.currentIndex !== -1

        RowLayout {
            id: layerHeaderRow

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
                    layerView.layerItem.layerStereoMode = model.get(index).value
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
                    layerView.layerItem.layerGridMode = model.get(index).value
                }

                Component.onCompleted: {
                }

                Layout.fillWidth: true
            }

            PropertyAnimation {
                id: visibility_fade_out_animation;
                target: visibilitySlider;
                property: "value";
                to: 0;
                duration: PlaybackSettings.fadeDuration;
            }
            ToolButton {
                id: fade_image_out
                icon.name: "view-hidden"
                focusPolicy: Qt.NoFocus
                enabled: layerView.layerItem.layerVisibility !== 0
                onClicked: {
                    if(!visibility_fade_out_animation.running){
                        visibility_fade_out_animation.start()
                    }
                }
            }

            VisibilitySlider {
                id: visibilitySlider
                overlayLabel: qsTr("Layer visibility: ")
                onValueChanged: {
                    if(value.toFixed(0) !== layerView.layerItem.layerVisibility) {
                        layerView.layerItem.layerVisibility = value.toFixed(0)
                    }
                }
            }

            PropertyAnimation {
                id: visibility_fade_in_animation;
                target: visibilitySlider;
                property: "value";
                to: 100;
                duration: PlaybackSettings.fadeDuration;
            }
            ToolButton {
                id: fade_image_in
                icon.name: "view-visible"
                focusPolicy: Qt.NoFocus
                enabled: layerView.layerItem.layerVisibility !== 100
                onClicked: {
                    if(!visibility_fade_in_animation.running){
                        visibility_fade_in_animation.start()
                    }
                }
            }
            Connections {
                target: layerView.layerItem
                function onLayerChanged(){
                    layerWindow.title = layerView.layerItem.layerTitle
                    visibilitySlider.value = layerView.layerItem.layerVisibility
                    for (let sm = 0; sm < stereoscopicModeForLayerList.count; ++sm) {
                        if (stereoscopicModeForLayerList.get(sm).value === layerView.layerItem.layerStereoMode) {
                            stereoscopicModeForLayer.currentIndex = sm
                            break
                        }
                    }
                    for (let gm = 0; gm < gridModeForLayerList.count; ++gm) {
                        if (gridModeForLayerList.get(gm).value === layerView.layerItem.layerGridMode) {
                            gridModeForLayer.currentIndex = gm
                            break
                        }
                    }
                }
                function onLayerValueChanged(){
                    if(visibilitySlider.value !== layerView.layerItem.layerVisibility)
                        visibilitySlider.value = layerView.layerItem.layerVisibility
                }
            }
        }
    }

    LayerQtItem {
        id: layerViewItem

        width: parent.width
        height: parent.height - toolBarLayerView.height
        anchors.top: toolBarLayerView.bottom

        Label {
            id: noLayerLabel
            anchors.fill: parent
            visible: layers.layersView.currentIndex === -1
            text: "Select a layer in the list to view it here..."
            font.family: "Helvetica"
            font.pointSize: 18
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }
}
