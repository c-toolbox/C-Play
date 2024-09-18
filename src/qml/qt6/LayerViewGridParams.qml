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
    id: root

    function resetValues() {
        planeWidthBox.value = layerView.layerItem.layerPlaneWidth;
        planeHeightBox.value = layerView.layerItem.layerPlaneHeight;
        planeDistanceBox.value = layerView.layerItem.layerPlaneDistance;
        planeElevationSpinBox.value = layerView.layerItem.layerPlaneElevation*100;
        planeAzimuthSpinBox.value = layerView.layerItem.layerPlaneAzimuth*100;
        planeRollSpinBox.value = layerView.layerItem.layerPlaneRoll*100;
        domeRotateYawSpinBox.value = layerView.layerItem.layerRotateYaw*100;
        sphereRotatePitchSpinBox.value = layerView.layerItem.layerRotatePitch*100;
        sphereRotateYawSpinBox.value = layerView.layerItem.layerRotateYaw*100;
        sphereRotateRollSpinBox.value = layerView.layerItem.layerRotateRoll*100;
        planeGridLayout.visible = (layerView.layerItem.layerGridMode === 1);
        domeGridLayout.visible = (layerView.layerItem.layerGridMode === 2);
        sphereGridLayout.visible = (layerView.layerItem.layerGridMode > 2);
    }

    color: Kirigami.Theme.alternateBackgroundColor
    height: 370
    title: qsTr("Layer Grid Parameters")
    visible: false
    width: 500

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

    Connections {
        function onLayerChanged() {
            resetValues();
        }

        target: layerView.layerItem
    }

    Label {
        id: noLayerLabel

        anchors.fill: parent
        font.family: "Helvetica"
        font.pointSize: 18
        horizontalAlignment: Text.AlignHCenter
        text: "Select a layer to view parameters here..."
        verticalAlignment: Text.AlignVCenter
        visible: layers.layersView.currentIndex === -1
        wrapMode: Text.WordWrap
    }

    Row {
        visible: layers.layersView.currentIndex !== -1
        anchors.fill: parent
        anchors.margins: 15

        GridLayout {
            id: splitGridLayout
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
            id: planeGridLayout
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
                Layout.alignment: Qt.AlignRight
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
                Layout.alignment: Qt.AlignRight
            }
            RowLayout {
                Layout.columnSpan: 2

                SpinBox {
                    id: planeWidthBox

                    enabled: layerView.layerItem.layerPlaneAspectRatio !== 1
                    from: 0
                    stepSize: 1
                    to: 2000

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
                Layout.alignment: Qt.AlignRight
            }
            RowLayout {
                Layout.columnSpan: 2

                SpinBox {
                    id: planeHeightBox

                    enabled: layerView.layerItem.layerPlaneAspectRatio !== 2
                    from: 0
                    stepSize: 1
                    to: 2000

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
                Layout.alignment: Qt.AlignRight
            }
            RowLayout {
                Layout.columnSpan: 2

                SpinBox {
                    id: planeDistanceBox

                    from: 0
                    stepSize: 1
                    to: 2000

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
                Layout.alignment: Qt.AlignRight
            }
            RowLayout {
                Layout.columnSpan: 2

                SpinBox {
                    id: planeElevationSpinBox

                    property int decimals: 2
                    property real realValue: value / 100

                    from: -18000
                    stepSize: 10
                    textFromValue: function (value, locale) {
                        return Number(value / 100).toLocaleString(locale, 'f', planeElevationSpinBox.decimals);
                    }
                    to: 18000
                    value: layerView.layerItem.layerPlaneElevation * 100
                    valueFromText: function (text, locale) {
                        return Number.fromLocaleString(locale, text) * 100;
                    }

                    validator: DoubleValidator {
                        bottom: Math.min(planeElevationSpinBox.from, planeElevationSpinBox.to)
                        top: Math.max(planeElevationSpinBox.from, planeElevationSpinBox.to)
                    }

                    onValueModified: {
                        layerView.layerItem.layerPlaneElevation = realValue;
                        planeElevationSlider.value = value;
                    }
                }

                Label {
                    Layout.alignment: Qt.AlignLeft
                    Layout.fillWidth: true
                    text: {
                        qsTr("degrees");
                    }
                }

                Slider {
                    id: planeElevationSlider

                    Layout.topMargin: Kirigami.Units.largeSpacing

                    implicitWidth: 180
                    from: -18000
                    to: 18000
                    value: layerView.layerItem.layerPlaneElevation * 100

                    onMoved: {
                        planeElevationSpinBox.value = value.toFixed(0);
                        layerView.layerItem.layerPlaneElevation = planeElevationSpinBox.realValue;
                    }

                    MouseArea {
                        acceptedButtons: Qt.MiddleButton
                        anchors.fill: parent

                        onClicked: layerView.layerItem.layerPlaneElevation = 0
                    }

                    Layout.fillWidth: true
                }

                Connections {
                    function onLayerValueChanged() {
                        if (planeElevationSpinBox.realValue !== layerView.layerItem.layerPlaneElevation)
                            planeElevationSpinBox.value = layerView.layerItem.layerPlaneElevation * 100;
                    }

                    target: layerView.layerItem
                }
            }

            Label {
                text: qsTr("Plane azimuth:")
                Layout.alignment: Qt.AlignRight
            }
            RowLayout {
                Layout.columnSpan: 2

                SpinBox {
                    id: planeAzimuthSpinBox

                    property int decimals: 2
                    property real realValue: value / 100

                    from: -18000
                    stepSize: 10
                    textFromValue: function (value, locale) {
                        return Number(value / 100).toLocaleString(locale, 'f', planeAzimuthSpinBox.decimals);
                    }
                    to: 18000
                    value: layerView.layerItem.layerPlaneAzimuth * 100
                    valueFromText: function (text, locale) {
                        return Number.fromLocaleString(locale, text) * 100;
                    }

                    validator: DoubleValidator {
                        bottom: Math.min(planeAzimuthSpinBox.from, planeAzimuthSpinBox.to)
                        top: Math.max(planeAzimuthSpinBox.from, planeAzimuthSpinBox.to)
                    }

                    onValueModified: {
                        layerView.layerItem.layerPlaneAzimuth = realValue;
                        planeAzimuthSlider.value = value;
                    }
                }

                Label {
                    Layout.alignment: Qt.AlignLeft
                    Layout.fillWidth: true
                    text: {
                        qsTr("degrees");
                    }
                }

                Slider {
                    id: planeAzimuthSlider

                    Layout.topMargin: Kirigami.Units.largeSpacing

                    implicitWidth: 180
                    from: -18000
                    to: 18000
                    value: layerView.layerItem.layerPlaneAzimuth * 100

                    onMoved: {
                        planeAzimuthSpinBox.value = value.toFixed(0);
                        layerView.layerItem.layerPlaneAzimuth = planeAzimuthSpinBox.realValue;
                    }

                    MouseArea {
                        acceptedButtons: Qt.MiddleButton
                        anchors.fill: parent

                        onClicked: layerView.layerItem.layerPlaneAzimuth = 0
                    }

                    Layout.fillWidth: true
                }

                Connections {
                    function onLayerValueChanged() {
                        if (planeAzimuthSpinBox.realValue !== layerView.layerItem.layerPlaneAzimuth)
                            planeAzimuthSpinBox.value = layerView.layerItem.layerPlaneAzimuth * 100;
                    }

                    target: layerView.layerItem
                }
            }

            Label {
                text: qsTr("Plane roll:")
                Layout.alignment: Qt.AlignRight
            }
            RowLayout {
                Layout.columnSpan: 2

                SpinBox {
                    id: planeRollSpinBox

                    property int decimals: 2
                    property real realValue: value / 100

                    from: -18000
                    stepSize: 10
                    textFromValue: function (value, locale) {
                        return Number(value / 100).toLocaleString(locale, 'f', planeRollSpinBox.decimals);
                    }
                    to: 18000
                    value: layerView.layerItem.layerPlaneRoll * 100
                    valueFromText: function (text, locale) {
                        return Number.fromLocaleString(locale, text) * 100;
                    }

                    validator: DoubleValidator {
                        bottom: Math.min(planeRollSpinBox.from, planeRollSpinBox.to)
                        top: Math.max(planeRollSpinBox.from, planeRollSpinBox.to)
                    }

                    onValueModified: {
                        layerView.layerItem.layerPlaneRoll = realValue;
                        planeRollSlider.value = value;
                    }
                }

                Label {
                    Layout.alignment: Qt.AlignLeft
                    Layout.fillWidth: true
                    text: {
                        qsTr("degrees");
                    }
                }

                Slider {
                    id: planeRollSlider

                    Layout.topMargin: Kirigami.Units.largeSpacing

                    implicitWidth: 180
                    from: -18000
                    to: 18000
                    value: layerView.layerItem.layerPlaneRoll * 100

                    onMoved: {
                        planeRollSpinBox.value = value.toFixed(0);
                        layerView.layerItem.layerPlaneRoll = planeRollSpinBox.realValue;
                    }

                    MouseArea {
                        acceptedButtons: Qt.MiddleButton
                        anchors.fill: parent

                        onClicked: layerView.layerItem.layerPlaneRoll = 0
                    }

                    Layout.fillWidth: true
                }

                Connections {
                    function onLayerValueChanged() {
                        if (planeRollSpinBox.realValue !== layerView.layerItem.layerPlaneRoll)
                            planeRollSpinBox.value = layerView.layerItem.layerPlaneRoll * 100;
                    }

                    target: layerView.layerItem
                }
            }
            Item {
                height: 1
                width: 1
            }
            Label {
                Layout.topMargin: Kirigami.Units.largeSpacing
                text: qsTr("Middle click on the sliders to reset them")
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                Layout.fillWidth: true
                Layout.columnSpan: 2
            }
        }
        GridLayout {
            id: domeGridLayout
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

            Label {
                text: qsTr("Yaw:")
                Layout.alignment: Qt.AlignRight
            }
            RowLayout {
                Layout.columnSpan: 2

                SpinBox {
                    id: domeRotateYawSpinBox

                    property int decimals: 2
                    property real realValue: value / 100

                    from: -18000
                    stepSize: 10
                    textFromValue: function (value, locale) {
                        return Number(value / 100).toLocaleString(locale, 'f', domeRotateYawSpinBox.decimals);
                    }
                    to: 18000
                    value: layerView.layerItem.layerRotateYaw * 100
                    valueFromText: function (text, locale) {
                        return Number.fromLocaleString(locale, text) * 100;
                    }

                    validator: DoubleValidator {
                        bottom: Math.min(domeRotateYawSpinBox.from, domeRotateYawSpinBox.to)
                        top: Math.max(domeRotateYawSpinBox.from, domeRotateYawSpinBox.to)
                    }

                    onValueModified: {
                        layerView.layerItem.layerRotateYaw = realValue;
                        domeRotateYawSlider.value = value;
                    }
                }

                Label {
                    Layout.alignment: Qt.AlignLeft
                    Layout.fillWidth: true
                    text: {
                        qsTr("degrees");
                    }
                }

                Slider {
                    id: domeRotateYawSlider

                    Layout.topMargin: Kirigami.Units.largeSpacing

                    implicitWidth: 180
                    from: -18000
                    to: 18000
                    value: layerView.layerItem.layerRotateYaw * 100

                    onMoved: {
                        domeRotateYawSpinBox.value = value.toFixed(0);
                        layerView.layerItem.layerRotateYaw = domeRotateYawSpinBox.realValue;
                    }

                    MouseArea {
                        acceptedButtons: Qt.MiddleButton
                        anchors.fill: parent

                        onClicked: layerView.layerItem.layerRotateYaw = 0
                    }

                    Layout.fillWidth: true
                }

                Connections {
                    function onLayerValueChanged() {
                        if (domeRotateYawSpinBox.realValue !== layerView.layerItem.layerRotateYaw)
                            domeRotateYawSpinBox.value = layerView.layerItem.layerRotateYaw * 100;
                    }

                    target: layerView.layerItem
                }
            }

            Item {
                height: 1
                width: 1
            }
            Label {
                Layout.topMargin: Kirigami.Units.largeSpacing
                text: qsTr("Middle click on the sliders to reset them")
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                Layout.fillWidth: true
                Layout.columnSpan: 2
            }
        }
        GridLayout {
            id: sphereGridLayout
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

            Label {
                text: qsTr("Pitch:")
                Layout.alignment: Qt.AlignRight
            }
            RowLayout {
                Layout.columnSpan: 2

                SpinBox {
                    id: sphereRotatePitchSpinBox

                    property int decimals: 2
                    property real realValue: value / 100

                    from: -18000
                    stepSize: 10
                    textFromValue: function (value, locale) {
                        return Number(value / 100).toLocaleString(locale, 'f', sphereRotatePitchSpinBox.decimals);
                    }
                    to: 18000
                    value: layerView.layerItem.layerRotatePitch * 100
                    valueFromText: function (text, locale) {
                        return Number.fromLocaleString(locale, text) * 100;
                    }

                    validator: DoubleValidator {
                        bottom: Math.min(sphereRotatePitchSpinBox.from, sphereRotatePitchSpinBox.to)
                        top: Math.max(sphereRotatePitchSpinBox.from, sphereRotatePitchSpinBox.to)
                    }

                    onValueModified: {
                        layerView.layerItem.layerRotatePitch = realValue;
                        sphereRotatePitchSlider.value = value;
                    }
                }

                Label {
                    Layout.alignment: Qt.AlignLeft
                    Layout.fillWidth: true
                    text: {
                        qsTr("degrees");
                    }
                }

                Slider {
                    id: sphereRotatePitchSlider

                    Layout.topMargin: Kirigami.Units.largeSpacing

                    implicitWidth: 180
                    from: -18000
                    to: 18000
                    value: layerView.layerItem.layerRotatePitch * 100

                    onMoved: {
                        sphereRotatePitchSpinBox.value = value.toFixed(0);
                        layerView.layerItem.layerRotatePitch = sphereRotatePitchSpinBox.realValue;
                    }

                    MouseArea {
                        acceptedButtons: Qt.MiddleButton
                        anchors.fill: parent

                        onClicked: layerView.layerItem.layerRotatePitch = 0
                    }

                    Layout.fillWidth: true
                }

                Connections {
                    function onLayerValueChanged() {
                        if (sphereRotatePitchSpinBox.realValue !== layerView.layerItem.layerRotatePitch)
                            sphereRotatePitchSpinBox.value = layerView.layerItem.layerRotatePitch * 100;
                    }

                    target: layerView.layerItem
                }
            }

            Label {
                text: qsTr("Yaw:")
                Layout.alignment: Qt.AlignRight
            }
            RowLayout {
                Layout.columnSpan: 2

                SpinBox {
                    id: sphereRotateYawSpinBox

                    property int decimals: 2
                    property real realValue: value / 100

                    from: -18000
                    stepSize: 10
                    textFromValue: function (value, locale) {
                        return Number(value / 100).toLocaleString(locale, 'f', sphereRotateYawSpinBox.decimals);
                    }
                    to: 18000
                    value: layerView.layerItem.layerRotateYaw * 100
                    valueFromText: function (text, locale) {
                        return Number.fromLocaleString(locale, text) * 100;
                    }

                    validator: DoubleValidator {
                        bottom: Math.min(sphereRotateYawSpinBox.from, sphereRotateYawSpinBox.to)
                        top: Math.max(sphereRotateYawSpinBox.from, sphereRotateYawSpinBox.to)
                    }

                    onValueModified: {
                        layerView.layerItem.layerRotateYaw = realValue;
                        sphereRotateYawSlider.value = value;
                    }
                }

                Label {
                    Layout.alignment: Qt.AlignLeft
                    Layout.fillWidth: true
                    text: {
                        qsTr("degrees");
                    }
                }

                Slider {
                    id: sphereRotateYawSlider

                    Layout.topMargin: Kirigami.Units.largeSpacing

                    implicitWidth: 180
                    from: -18000
                    to: 18000
                    value: layerView.layerItem.layerRotateYaw * 100

                    onMoved: {
                        sphereRotateYawSpinBox.value = value.toFixed(0);
                        layerView.layerItem.layerRotateYaw = sphereRotateYawSpinBox.realValue;
                    }

                    MouseArea {
                        acceptedButtons: Qt.MiddleButton
                        anchors.fill: parent

                        onClicked: layerView.layerItem.layerRotateYaw = 0
                    }

                    Layout.fillWidth: true
                }

                Connections {
                    function onLayerValueChanged() {
                        if (sphereRotateYawSpinBox.realValue !== layerView.layerItem.layerRotateYaw)
                            sphereRotateYawSpinBox.value = layerView.layerItem.layerRotateYaw * 100;
                    }

                    target: layerView.layerItem
                }
            }

            Label {
                text: qsTr("Roll:")
                Layout.alignment: Qt.AlignRight
            }
            RowLayout {
                Layout.columnSpan: 2

                SpinBox {
                    id: sphereRotateRollSpinBox

                    property int decimals: 2
                    property real realValue: value / 100

                    from: -18000
                    stepSize: 10
                    textFromValue: function (value, locale) {
                        return Number(value / 100).toLocaleString(locale, 'f', sphereRotateRollSpinBox.decimals);
                    }
                    to: 18000
                    value: layerView.layerItem.layerRotateRoll * 100
                    valueFromText: function (text, locale) {
                        return Number.fromLocaleString(locale, text) * 100;
                    }

                    validator: DoubleValidator {
                        bottom: Math.min(sphereRotateRollSpinBox.from, sphereRotateRollSpinBox.to)
                        top: Math.max(sphereRotateRollSpinBox.from, sphereRotateRollSpinBox.to)
                    }

                    onValueModified: {
                        layerView.layerItem.layerRotateRoll = realValue;
                        sphereRotateRollSlider.value = value;
                    }
                }

                Label {
                    Layout.alignment: Qt.AlignLeft
                    Layout.fillWidth: true
                    text: {
                        qsTr("degrees");
                    }
                }

                Slider {
                    id: sphereRotateRollSlider

                    Layout.topMargin: Kirigami.Units.largeSpacing

                    implicitWidth: 180
                    from: -18000
                    to: 18000
                    value: layerView.layerItem.layerRotateRoll * 100

                    onMoved: {
                        sphereRotateRollSpinBox.value = value.toFixed(0);
                        layerView.layerItem.layerRotateRoll = sphereRotateRollSpinBox.realValue;
                    }

                    MouseArea {
                        acceptedButtons: Qt.MiddleButton
                        anchors.fill: parent

                        onClicked: layerView.layerItem.layerRotateRoll = 0
                    }

                    Layout.fillWidth: true
                }

                Connections {
                    function onLayerValueChanged() {
                        if (sphereRotateRollSpinBox.realValue !== layerView.layerItem.layerRotateRoll)
                            sphereRotateRollSpinBox.value = layerView.layerItem.layerRotateRoll * 100;
                    }

                    target: layerView.layerItem
                }
            }

            Item {
                height: 1
                width: 1
            }
            Label {
                Layout.topMargin: Kirigami.Units.largeSpacing
                text: qsTr("Middle click on the sliders to reset them")
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                Layout.fillWidth: true
                Layout.columnSpan: 2
            }
        }
    }
}
