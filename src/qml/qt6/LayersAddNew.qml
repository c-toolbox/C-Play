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
import Qt.labs.platform as Platform

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

Kirigami.ApplicationWindow {
    id: root

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
            layerCoreProps.resetValues();
        }
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

        LayerCoreProperties {
            id: layerCoreProps
            Layout.columnSpan: 2
        }
        
        RowLayout {
            Layout.bottomMargin: 5
            Layout.columnSpan: 2

            Button {
                Layout.fillWidth: true
                icon.name: "layer-new"
                text: qsTr("Add new layer")

                onClicked: {
                    if (layerCoreProps.layerTitle.text !== "") {
                        if (layerCoreProps.typeComboBox.currentText === "NDI") {
                            layerView.layerItem.layerIdx = app.slides.selected.addLayer(layerCoreProps.layerTitle.text, layerCoreProps.typeComboBox.currentIndex + 1, layerCoreProps.ndiSenderComboBox.currentText, layerCoreProps.stereoscopicModeForLayer.currentIndex, layerCoreProps.gridModeForLayer.currentIndex);
                            layersAddNew.visible = false;
                            app.slides.updateSelectedSlide();
                            mpv.focus = true;
                        } else if (layerCoreProps.typeComboBox.currentText === "Spout") {
                            layerView.layerItem.layerIdx = app.slides.selected.addLayer(layerCoreProps.layerTitle.text, layerCoreProps.typeComboBox.currentIndex + 1, layerCoreProps.spoutSenderComboBox.currentText, layerCoreProps.stereoscopicModeForLayer.currentIndex, layerCoreProps.gridModeForLayer.currentIndex);
                            layersAddNew.visible = false;
                            app.slides.updateSelectedSlide();
                            mpv.focus = true;
                        } else if (layerCoreProps.typeComboBox.currentText === "Stream") {
                            if(layerCoreProps.streamsLayout.customEntry){
                                layerView.layerItem.layerIdx = app.slides.selected.addLayer(layerCoreProps.layerTitle.text, layerCoreProps.typeComboBox.currentIndex + 1, layerCoreProps.streamCustomEntryField.text, layerCoreProps.stereoscopicModeForLayer.currentIndex, layerCoreProps.gridModeForLayer.currentIndex);
                            }
                            else {
                                layerView.layerItem.layerIdx = app.slides.selected.addLayer(layerCoreProps.layerTitle.text, layerCoreProps.typeComboBox.currentIndex + 1, layerCoreProps.streamsComboBox.currentValue, layerCoreProps.stereoscopicModeForLayer.currentIndex, layerCoreProps.gridModeForLayer.currentIndex);
                            }
                            layersAddNew.visible = false;
                            app.slides.updateSelectedSlide();
                            mpv.focus = true;
                        } else if (layerCoreProps.typeComboBox.currentText === "Text") {
                            layerView.layerItem.layerIdx = app.slides.selected.addLayer(layerCoreProps.layerTitle.text, layerCoreProps.typeComboBox.currentIndex + 1, layerCoreProps.textForLayer.text, layerCoreProps.stereoscopicModeForLayer.currentIndex, layerCoreProps.gridModeForLayer.currentIndex);
                            layersAddNew.visible = false;
                            app.slides.updateSelectedSlide();
                            mpv.focus = true;
                        } else if (layerCoreProps.fileForLayer.text !== "") {
                            layerView.layerItem.layerIdx = app.slides.selected.addLayer(layerCoreProps.layerTitle.text, layerCoreProps.typeComboBox.currentIndex + 1, layerCoreProps.fileForLayer.text, layerCoreProps.stereoscopicModeForLayer.currentIndex, layerCoreProps.gridModeForLayer.currentIndex);
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
