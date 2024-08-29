/*
 * SPDX-FileCopyrightText:
 * 2024 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Qt.labs.platform 1.0 as Platform

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

SettingsBasePage {
    id: root

    GridLayout {
        id: content

        columns: 3

        // ------------------------------------
        // LAYERS PARAMETERS
        // --

        SettingsHeader {
            text: qsTr("Layer settings")
            Layout.columnSpan: 3
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("Default stereoscopic mode for new layer:")
            Layout.alignment: Qt.AlignRight
        }
        RowLayout {
            ComboBox {
                id: stereoscopicModeForNewLayerComboBoxBg
                enabled: true
                textRole: "mode"
                model: ListModel {
                    id: stereoscopicModeForNewLayer
                    ListElement { mode: "2D (mono)"; value: 0 }
                    ListElement { mode: "3D (side-by-side)"; value: 1}
                    ListElement { mode: "3D (top-bottom)"; value: 2 }
                    ListElement { mode: "3D (top-bottom+flip)"; value: 3 }
                }

                onActivated: {
                    LayerSettings.defaultStereoModeForLayers = model.get(index).value
                    LayerSettings.save()
                }

                Component.onCompleted: {
                    for (let i = 0; i < stereoscopicModeForNewLayer.count; ++i) {
                        if (stereoscopicModeForNewLayer.get(i).value === LayerSettings.defaultStereoModeForLayers) {
                            currentIndex = i
                            break
                        }
                    }
                }
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("Default grid mode for new layer:")
            Layout.alignment: Qt.AlignRight
        }
        RowLayout {
            ComboBox {
                id: gridModeForNewLayerComboBox
                enabled: true
                textRole: "mode"
                model: ListModel {
                    id: gridModeForNewLayer
                    ListElement { mode: "None/Pre-split"; value: 0 }
                    ListElement { mode: "Plane"; value: 1 }
                    ListElement { mode: "Dome"; value: 2}
                    ListElement { mode: "Sphere EQR"; value: 3 }
                    ListElement { mode: "Sphere EAC"; value: 4 }
                }

                onActivated: {
                    LayerSettings.defaultGridModeForLayers = model.get(index).value
                    LayerSettings.save()
                }

                Component.onCompleted: {
                    for (let i = 0; i < gridModeForNewLayer.count; ++i) {
                        if (gridModeForNewLayer.get(i).value === LayerSettings.defaultGridModeForLayers) {
                            currentIndex = i
                            break
                        }
                    }
                }
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("Default visibility for new layer:")
            Layout.alignment: Qt.AlignRight
        }
        SpinBox {
            from: 0
            to: 100
            value: LayerSettings.defaultLayerVisibility
            editable: true
            onValueChanged: {
                LayerSettings.defaultLayerVisibility = value.toFixed(0)
                LayerSettings.save()
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }
    }
}
