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

    function resetValues() {
        planeWidthBox.value = layerView.layerItem.layerPlaneWidth;
        planeHeightBox.value = layerView.layerItem.layerPlaneHeight;
        planeDistanceBox.value = layerView.layerItem.layerPlaneDistance;
        planeElevationBox.value = layerView.layerItem.layerPlaneElevation;
        planeAzimuthBox.value = layerView.layerItem.layerPlaneAzimuth;
        planeRollBox.value = layerView.layerItem.layerPlaneRoll;
    }

    color: Kirigami.Theme.alternateBackgroundColor
    height: 300
    title: qsTr("Layer Grid Parameters")
    visible: false
    width: 300

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
            resetValues();
        }
    }

    Row {
        anchors.fill: parent
        anchors.margins: 15

        GridLayout {
            Layout.alignment: Qt.AlignHCenter | Qt.AlignTop
            columnSpacing: 2
            columns: 3
            rowSpacing: 8
            visible: layerView.layerItem.layerGridMode === 0

            RowLayout {
                Layout.bottomMargin: 5
                Layout.columnSpan: 3

                Rectangle {
                    color: Kirigami.Theme.alternateBackgroundColor
                    height: 1
                    width: Kirigami.Units.gridUnit
                }
                Kirigami.Heading {
                    text: qsTr("Pre-split properties for the layer")
                }
                Rectangle {
                    Layout.fillWidth: true
                    color: Kirigami.Theme.alternateBackgroundColor
                    height: 1
                }
                Item {
                    // spacer item
                    Layout.fillWidth: true
                }
            }
        }
        GridLayout {
            Layout.alignment: Qt.AlignHCenter | Qt.AlignTop
            columnSpacing: 2
            columns: 3
            rowSpacing: 8
            visible: layerView.layerItem.layerGridMode === 1

            RowLayout {
                Layout.bottomMargin: 5
                Layout.columnSpan: 3

                Rectangle {
                    color: Kirigami.Theme.alternateBackgroundColor
                    height: 1
                    width: Kirigami.Units.gridUnit
                }
                Kirigami.Heading {
                    text: qsTr("Plane properties for the layer")
                }
                Rectangle {
                    Layout.fillWidth: true
                    color: Kirigami.Theme.alternateBackgroundColor
                    height: 1
                }
                Item {
                    // spacer item
                    Layout.fillWidth: true
                }
            }

            // ------------------------------------
            // Plane parameters
            // ------------------------------------
            Label {
                text: qsTr("Determine plane size based on:")
            }
            ComboBox {
                id: planeSizeBasedOnComboBox

                Layout.columnSpan: 2
                enabled: true
                textRole: "mode"

                model: ListModel {
                    id: planeSizeBasedOnMode

                    ListElement {
                        mode: "Values below"
                        value: 0
                    }
                    ListElement {
                        mode: "Height below and video aspect ratio"
                        value: 1
                    }
                    ListElement {
                        mode: "Width below and video aspect ratio"
                        value: 2
                    }
                }

                Component.onCompleted: {
                    for (let i = 0; i < planeSizeBasedOnMode.count; ++i) {
                        if (planeSizeBasedOnMode.get(i).value === layerView.layerItem.layerPlaneAspectRatio) {
                            currentIndex = i;
                            break;
                        }
                    }
                }
                onActivated: {
                    layerView.layerItem.layerPlaneAspectRatio = model.get(index).value;
                }
            }
            Label {
                text: qsTr("Plane width:")
            }
            RowLayout {
                Layout.columnSpan: 2

                SpinBox {
                    id: planeWidthBox

                    enabled: layerView.layerItem.layerPlaneAspectRatio !== 1
                    from: 0
                    stepSize: 1
                    to: 2000
                    value: layerView.layerItem.layerPlaneWidth

                    onValueChanged: layerView.layerItem.layerPlaneWidth = value
                }
                Label {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    text: {
                        qsTr("cm");
                    }
                }
            }
            Label {
                text: qsTr("Plane height:")
            }
            RowLayout {
                Layout.columnSpan: 2

                SpinBox {
                    id: planeHeightBox

                    enabled: layerView.layerItem.layerPlaneAspectRatio !== 2
                    from: 0
                    stepSize: 1
                    to: 2000
                    value: layerView.layerItem.layerPlaneHeight

                    onValueChanged: layerView.layerItem.layerPlaneHeight = value
                }
                Label {
                    Layout.alignment: Qt.AlignLeft
                    Layout.fillWidth: true
                    text: {
                        qsTr("cm");
                    }
                }
            }
            Label {
                text: qsTr("Plane distance:")
            }
            RowLayout {
                Layout.columnSpan: 2

                SpinBox {
                    id: planeDistanceBox

                    from: 0
                    stepSize: 1
                    to: 2000
                    value: layerView.layerItem.layerPlaneDistance

                    onValueChanged: layerView.layerItem.layerPlaneDistance = value
                }
                Label {
                    Layout.alignment: Qt.AlignLeft
                    Layout.fillWidth: true
                    elide: Text.ElideLeft
                    text: {
                        qsTr("cm");
                    }
                }
            }
            Label {
                text: qsTr("Plane elevation:")
            }
            RowLayout {
                Layout.columnSpan: 2

                SpinBox {
                    id: planeElevationBox

                    from: -180
                    to: 180
                    value: layerView.layerItem.layerPlaneElevation

                    onValueChanged: layerView.layerItem.layerPlaneElevation = value
                }
                Label {
                    Layout.alignment: Qt.AlignLeft
                    Layout.fillWidth: true
                    text: {
                        qsTr("degrees");
                    }
                }
            }
            Label {
                text: qsTr("Plane azimuth:")
            }
            RowLayout {
                Layout.columnSpan: 2

                SpinBox {
                    id: planeAzimuthBox

                    from: -180
                    to: 180
                    value: layerView.layerItem.layerPlaneAzimuth

                    onValueChanged: layerView.layerItem.layerPlaneAzimuth = value
                }
                Label {
                    Layout.alignment: Qt.AlignLeft
                    Layout.fillWidth: true
                    elide: Text.ElideLeft
                    text: {
                        qsTr("degrees");
                    }
                }
            }
            Label {
                text: qsTr("Plane roll:")
            }
            RowLayout {
                Layout.columnSpan: 2

                SpinBox {
                    id: planeRollBox

                    from: -180
                    to: 180
                    value: layerView.layerItem.layerPlaneRoll

                    onValueChanged: layerView.layerItem.layerPlaneRoll = value
                }
                Label {
                    Layout.alignment: Qt.AlignLeft
                    Layout.fillWidth: true
                    elide: Text.ElideLeft
                    text: {
                        qsTr("degrees");
                    }
                }
            }
        }
        GridLayout {
            Layout.alignment: Qt.AlignHCenter | Qt.AlignTop
            columnSpacing: 2
            columns: 3
            rowSpacing: 8
            visible: layerView.layerItem.layerGridMode === 2

            RowLayout {
                Layout.bottomMargin: 5
                Layout.columnSpan: 3

                Rectangle {
                    color: Kirigami.Theme.alternateBackgroundColor
                    height: 1
                    width: Kirigami.Units.gridUnit
                }
                Kirigami.Heading {
                    text: qsTr("Dome properties for the layer")
                }
                Rectangle {
                    Layout.fillWidth: true
                    color: Kirigami.Theme.alternateBackgroundColor
                    height: 1
                }
                Item {
                    // spacer item
                    Layout.fillWidth: true
                }
            }
        }
        GridLayout {
            Layout.alignment: Qt.AlignHCenter | Qt.AlignTop
            columnSpacing: 2
            columns: 3
            rowSpacing: 8
            visible: layerView.layerItem.layerGridMode > 2

            RowLayout {
                Layout.bottomMargin: 5
                Layout.columnSpan: 3

                Rectangle {
                    color: Kirigami.Theme.alternateBackgroundColor
                    height: 1
                    width: Kirigami.Units.gridUnit
                }
                Kirigami.Heading {
                    text: qsTr("Sphere properties for the layer")
                }
                Rectangle {
                    Layout.fillWidth: true
                    color: Kirigami.Theme.alternateBackgroundColor
                    height: 1
                }
                Item {
                    // spacer item
                    Layout.fillWidth: true
                }
            }
        }
    }
}
