/*
 * SPDX-FileCopyrightText:
 * 2024-2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Dialogs

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

Kirigami.ApplicationWindow {
    id: root

    property var restObsActionNames: [qsTr("Set Profile"), qsTr("Set Scene"), qsTr("Set Scene Collection"), qsTr("Custom")]

    function resizeForGridMode() {
        if (layerView.layerItem.layerTypeName === "Control") {
            root.height = 180;
            return;
        }
        if (layerView.layerItem.layerTypeName === "REST") {
            root.height = restGridLayout.isObsCommand() ? 500 : 320;
            return;
        }
        var mode = layerView.layerItem.layerGridMode;
        if (mode === 1) {
            root.height = 450;
        } else if (mode === 2) {
            root.height = 150;
        } else if (mode > 2) {
            root.height = 250;
        } else {
            root.height = 150;
        }
    }

    function resetValues() {
        controlGridLayout.visible = (layerView.layerItem.layerTypeName === "Control");
        restGridLayout.visible = (layerView.layerItem.layerTypeName === "REST");
        if (controlGridLayout.visible) {
            controlOperationComboBox.currentIndex = controlOperationComboBox.find(layerView.layerItem.layerOperation);
            controlParameterField.text = layerView.layerItem.layerParameter;
            resizeForGridMode();
            return;
        }
        if (restGridLayout.visible) {
            restUrlField.text = layerView.layerItem.layerRestUrl;
            restGridMethodComboBox.currentIndex = layerView.layerItem.layerRestMethod;
            restGridIgnoreStatusCheckBox.checked = layerView.layerItem.layerRestIgnoreStatus;
            restGridLayout.loadParametersFromJson(layerView.layerItem.layerRestParameters);
            restGridLayout.updateObsOptions();
            resizeForGridMode();
            return;
        }
        planeWidthBox.value = layerView.layerItem.layerPlaneWidth;
        planeHeightBox.value = layerView.layerItem.layerPlaneHeight;
        planeDistanceBox.value = layerView.layerItem.layerPlaneDistance;
        planeHorizontalMoveBox.value = layerView.layerItem.layerPlaneHorizontal;
        planeVerticalMoveBox.value = layerView.layerItem.layerPlaneVertical;
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
        resizeForGridMode();
    }

    color: Kirigami.Theme.alternateBackgroundColor
    height: 420
    title: qsTr("Layer Grid Parameters")
    visible: false

                function isObsCommand() {
                    return restGridMethodComboBox.currentIndex === 4 || restGridMethodComboBox.currentIndex === 5
                        || restUrlField.text.indexOf("ws://") === 0 || restUrlField.text.indexOf("wss://") === 0;
                }

                function updateLayerParameters() {
                    layerView.layerItem.layerRestParameters = getParametersAsJson();
                }
    width: 520

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

        function onLayerValueChanged() {
            controlGridLayout.visible = (layerView.layerItem.layerTypeName === "Control");
            restGridLayout.visible = (layerView.layerItem.layerTypeName === "REST");
            if (!controlGridLayout.visible && !restGridLayout.visible) {
                planeGridLayout.visible = (layerView.layerItem.layerGridMode === 1);
                domeGridLayout.visible = (layerView.layerItem.layerGridMode === 2);
                sphereGridLayout.visible = (layerView.layerItem.layerGridMode > 2);
            } else {
                planeGridLayout.visible = false;
                domeGridLayout.visible = false;
                sphereGridLayout.visible = false;
            }
            resizeForGridMode();
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

        ColumnLayout {
            width: parent.width
            spacing: 10

            // Sub-layer selection combobox (visible only in division mode)
            RowLayout {
                Layout.fillWidth: true
                visible: layerView.layerItem.layerTextureDivisionMode === 2
                         && layerView.layerItem.layerSubLayerCount > 0

                Label {
                    text: qsTr("Editing Sub-Layer:")
                    font.pointSize: 9
                }
                ComboBox {
                    id: gridParamsSubLayerComboBox

                    Layout.fillWidth: true
                    focusPolicy: Qt.NoFocus
                    model: layerView.layerItem.layerSubLayerNames
                    currentIndex: layerView.layerItem.layerSelectedSubLayer

                    onActivated: {
                        layerView.layerItem.layerSelectedSubLayer = index;
                    }
                }

                Connections {
                    function onLayerValueChanged() {
                        gridParamsSubLayerComboBox.currentIndex = layerView.layerItem.layerSelectedSubLayer;
                    }
                    function onLayerChanged() {
                        gridParamsSubLayerComboBox.currentIndex = layerView.layerItem.layerSelectedSubLayer;
                    }

                    target: layerView.layerItem
                }
            }

            // Grid layouts below
            GridLayout {
                id: splitGridLayout
                Layout.alignment: Qt.AlignHCenter | Qt.AlignTop
                columnSpacing: 2
                columns: 3
                rowSpacing: 8
                visible: layerView.layerItem.layerGridMode === 0 && layerView.layerItem.layerTypeName !== "Control" && layerView.layerItem.layerTypeName !== "REST"

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
                visible: layerView.layerItem.layerGridMode === 1 && layerView.layerItem.layerTypeName !== "Control" && layerView.layerItem.layerTypeName !== "REST"

                RowLayout {
                    Layout.bottomMargin: 5
                    Layout.columnSpan: 3

                    Rectangle {
                        color: Kirigami.Theme.alternateBackgroundColor
                        height: 1
                        width: Kirigami.Units.gridUnit
                    }
                    Kirigami.Heading {
                        text: {
                            if (layerView.layerItem.layerSelectedSubLayer > 0) {
                                var names = layerView.layerItem.layerSubLayerNames;
                                var idx = layerView.layerItem.layerSelectedSubLayer;
                                var name = (idx >= 0 && idx < names.length) ? names[idx] : "";
                                return qsTr("Plane properties for sub-layer: %1").arg(name);
                            }
                            return qsTr("Plane properties for the layer");
                        }
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
                    text: qsTr("Plane size based on:")
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
                            qsTr("deg");
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

                        Layout.fillWidth: true
                    }

                    Button {
                        text: qsTr("Reset")
                        onClicked: layerView.layerItem.layerPlaneElevation = GridSettings.defaultPlane_Elevation_DegreesValue
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
                            qsTr("deg");
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

                        Layout.fillWidth: true
                    }

                    Button {
                        text: qsTr("Reset")
                        onClicked: layerView.layerItem.layerPlaneAzimuth = 0
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
                            qsTr("deg");
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

                        Layout.fillWidth: true
                    }

                    Button {
                        text: qsTr("Reset")
                        onClicked: layerView.layerItem.layerPlaneRoll = 0
                    }

                    Connections {
                        function onLayerValueChanged() {
                            if (planeRollSpinBox.realValue !== layerView.layerItem.layerPlaneRoll)
                                planeRollSpinBox.value = layerView.layerItem.layerPlaneRoll * 100;
                        }

                        target: layerView.layerItem
                    }
                }

                Label {
                    text: qsTr("Plane horizontal move:")
                    Layout.alignment: Qt.AlignRight
                }
                RowLayout {
                    Layout.columnSpan: 2

                    SpinBox {
                        id: planeHorizontalMoveBox

                        from: -GridSettings.plane_Horizontal_Range_CM
                        stepSize: 1
                        to: GridSettings.plane_Horizontal_Range_CM

                        onValueChanged: layerView.layerItem.layerPlaneHorizontal = value
                    }
                    Label {
                        Layout.alignment: Qt.AlignLeft
                        Layout.fillWidth: true
                        elide: Text.ElideLeft
                        text: {
                            qsTr("cm");
                        }
                    }
                    Slider {
                        id: planeHorizontalSlider

                        Layout.topMargin: Kirigami.Units.largeSpacing

                        implicitWidth: 180
                        from: -GridSettings.plane_Horizontal_Range_CM
                        to: GridSettings.plane_Horizontal_Range_CM
                        value: layerView.layerItem.layerPlaneHorizontal

                        onMoved: {
                            planeHorizontalMoveBox.value = value;
                            layerView.layerItem.layerPlaneHorizontal = planeHorizontalMoveBox.value;
                        }

                        Layout.fillWidth: true
                    }

                    Button {
                        text: qsTr("Reset")
                        onClicked: layerView.layerItem.layerPlaneHorizontal = 0
                    }

                    Connections {
                        function onLayerValueChanged() {
                            if (planeHorizontalMoveBox.value !== layerView.layerItem.layerPlaneHorizontal)
                                planeHorizontalMoveBox.value = layerView.layerItem.layerPlaneHorizontal;
                        }

                        target: layerView.layerItem
                    }
                }
                Label {
                    text: qsTr("Plane vertical move:")
                    Layout.alignment: Qt.AlignRight
                }
                RowLayout {
                    Layout.columnSpan: 2

                    SpinBox {
                        id: planeVerticalMoveBox

                        from: -GridSettings.plane_Vertical_Range_CM
                        stepSize: 1
                        to: GridSettings.plane_Vertical_Range_CM

                        onValueChanged: layerView.layerItem.layerPlaneVertical = value
                    }
                    Label {
                        Layout.alignment: Qt.AlignLeft
                        Layout.fillWidth: true
                        elide: Text.ElideLeft
                        text: {
                            qsTr("cm");
                        }
                    }
                    Slider {
                        id: planeVerticalSlider

                        Layout.topMargin: Kirigami.Units.largeSpacing

                        implicitWidth: 180
                        from: -GridSettings.plane_Vertical_Range_CM
                        to: GridSettings.plane_Vertical_Range_CM
                        value: layerView.layerItem.layerPlaneVertical

                        onMoved: {
                            planeVerticalMoveBox.value = value;
                            layerView.layerItem.layerPlaneVertical = planeVerticalMoveBox.value;
                        }

                        Layout.fillWidth: true
                    }

                    Button {
                        text: qsTr("Reset")
                        onClicked: layerView.layerItem.layerPlaneVertical = 0
                    }

                    Connections {
                        function onLayerValueChanged() {
                            if (planeVerticalMoveBox.value !== layerView.layerItem.layerPlaneVertical)
                                planeVerticalMoveBox.value = layerView.layerItem.layerPlaneVertical;
                        }

                        target: layerView.layerItem
                    }
                }
            }
            GridLayout {
                id: domeGridLayout
                Layout.alignment: Qt.AlignHCenter | Qt.AlignTop
                columnSpacing: 2
                columns: 3
                rowSpacing: 8
                visible: layerView.layerItem.layerGridMode === 2 && layerView.layerItem.layerTypeName !== "Control" && layerView.layerItem.layerTypeName !== "REST"

                RowLayout {
                    Layout.bottomMargin: 5
                    Layout.columnSpan: 3

                    Rectangle {
                        color: Kirigami.Theme.alternateBackgroundColor
                        height: 1
                        width: Kirigami.Units.gridUnit
                    }
                    Kirigami.Heading {
                        text: {
                            if (layerView.layerItem.layerSelectedSubLayer > 0) {
                                var names = layerView.layerItem.layerSubLayerNames;
                                var idx = layerView.layerItem.layerSelectedSubLayer;
                                var name = (idx >= 0 && idx < names.length) ? names[idx] : "";
                                return qsTr("Dome properties for sub-layer: %1").arg(name);
                        }
                        return qsTr("Dome properties for the layer");
                    }
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
                        qsTr("deg");
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

                    Layout.fillWidth: true
                }

                Button {
                    text: qsTr("Reset")
                    onClicked: layerView.layerItem.layerRotateYaw = 0
                }

                Connections {
                    function onLayerValueChanged() {
                        if (domeRotateYawSpinBox.realValue !== layerView.layerItem.layerRotateYaw)
                            domeRotateYawSpinBox.value = layerView.layerItem.layerRotateYaw * 100;
                    }

                    target: layerView.layerItem
                }
            }
        }
        GridLayout {
            id: sphereGridLayout
            Layout.alignment: Qt.AlignHCenter | Qt.AlignTop
            columnSpacing: 2
            columns: 3
            rowSpacing: 8
            visible: layerView.layerItem.layerGridMode > 2 && layerView.layerItem.layerTypeName !== "Control" && layerView.layerItem.layerTypeName !== "REST"

            RowLayout {
                Layout.bottomMargin: 5
                Layout.columnSpan: 3

                Rectangle {
                    color: Kirigami.Theme.alternateBackgroundColor
                    height: 1
                    width: Kirigami.Units.gridUnit
                }
                Kirigami.Heading {
                    text: {
                        if (layerView.layerItem.layerSelectedSubLayer > 0) {
                            var names = layerView.layerItem.layerSubLayerNames;
                            var idx = layerView.layerItem.layerSelectedSubLayer;
                            var name = (idx >= 0 && idx < names.length) ? names[idx] : "";
                            return qsTr("Sphere properties for sub-layer: %1").arg(name);
                        }
                        return qsTr("Sphere properties for the layer");
                    }
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
                        qsTr("deg");
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

                    Layout.fillWidth: true
                }

                Button {
                    text: qsTr("Reset")
                    onClicked: layerView.layerItem.layerRotatePitch = 0
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
                        qsTr("deg");
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

                    Layout.fillWidth: true
                }

                Button {
                    text: qsTr("Reset")
                    onClicked: layerView.layerItem.layerRotateYaw = 0
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
                        qsTr("deg");
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

                    Layout.fillWidth: true
                }

                Button {
                    text: qsTr("Reset")
                    onClicked: layerView.layerItem.layerRotateRoll = 0
                }

                Connections {
                    function onLayerValueChanged() {
                        if (sphereRotateRollSpinBox.realValue !== layerView.layerItem.layerRotateRoll)
                            sphereRotateRollSpinBox.value = layerView.layerItem.layerRotateRoll * 100;
                    }

                    target: layerView.layerItem
                }
            }
        }
        GridLayout {
            id: controlGridLayout
            Layout.alignment: Qt.AlignHCenter | Qt.AlignTop
            columnSpacing: 2
            columns: 3
            rowSpacing: 8
            width: parent.width
            visible: layerView.layerItem.layerTypeName === "Control"

            RowLayout {
                Layout.bottomMargin: 5
                Layout.columnSpan: 3

                Rectangle {
                    color: Kirigami.Theme.alternateBackgroundColor
                    height: 1
                    width: Kirigami.Units.gridUnit
                }
                Kirigami.Heading {
                    text: qsTr("Control properties for the layer")
                }
                Rectangle {
                    Layout.fillWidth: true
                    color: Kirigami.Theme.alternateBackgroundColor
                    height: 1
                }
                Item {
                    Layout.fillWidth: true
                }
            }

            Label {
                text: qsTr("Operation:")
                Layout.alignment: Qt.AlignRight
            }
            ComboBox {
                id: controlOperationComboBox

                Layout.fillWidth: true
                Layout.columnSpan: 2

                model: ["Play", "Pause", "Stop", "Rewind", "Seek", 
                        "SetPosition", "SetSpeed", "SetVolume", "SetSyncVolumeVisibilityFading",
                        "SetBackgroundVisibility", "SetForegroundVisibility", "SetNodeWindowsOpacity",
                        "LoadFromAudioTracks", "LoadFromPlaylist", "LoadFromSections", "LoadFromSlides", 
                        "FadeVolumeDown", "FadeVolumeUp", "FadeImageDown", "FadeImageUp", 
                        "SpinPitchUp", "SpinPitchDown", "SpinYawLeft", "SpinYawRight",
                        "SpinRollCW", "SpinRollCCW", "OrientationAndSpinReset", "RunSurfaceTransition"]

                Component.onCompleted: {
                    currentIndex = find(layerView.layerItem.layerOperation);
                }
                onActivated: {
                    layerView.layerItem.layerOperation = currentText;
                }

                Connections {
                    function onLayerChanged() {
                        controlOperationComboBox.currentIndex = controlOperationComboBox.find(layerView.layerItem.layerOperation);
                    }
                    target: layerView.layerItem
                }
            }

            Label {
                text: qsTr("Parameter:")
                Layout.alignment: Qt.AlignRight
            }
            TextField {
                id: controlParameterField

                Layout.fillWidth: true
                Layout.columnSpan: 2
                placeholderText: "Parameter value"
                text: layerView.layerItem.layerParameter

                onEditingFinished: {
                    layerView.layerItem.layerParameter = text;
                }

                Connections {
                    function onLayerChanged() {
                        controlParameterField.text = layerView.layerItem.layerParameter;
                    }
                    target: layerView.layerItem
                }
            }
        }
        GridLayout {
            id: restGridLayout
            Layout.alignment: Qt.AlignHCenter | Qt.AlignTop
            columnSpacing: 2
            columns: 3
            rowSpacing: 8
            width: parent.width
            visible: layerView.layerItem.layerTypeName === "REST"

            function isObsCommand() {
                return restGridObsConnectCheckBox.checked && restGridMethodComboBox.currentIndex === 4;
            }

            function obsRequestType() {
                if (!isObsCommand())
                    return "";
                if (restGridObsActionComboBox.currentIndex === 0)
                    return "SetCurrentProfile";
                if (restGridObsActionComboBox.currentIndex === 1)
                    return "SetCurrentProgramScene";
                if (restGridObsActionComboBox.currentIndex === 2)
                    return "SetCurrentSceneCollection";
                return restGridObsCustomRequestType.text;
            }

            function obsOptionParameterName() {
                if (restGridObsActionComboBox.currentIndex === 0)
                    return "profileName";
                if (restGridObsActionComboBox.currentIndex === 1)
                    return "sceneName";
                if (restGridObsActionComboBox.currentIndex === 2)
                    return "sceneCollectionName";
                return "";
            }

            function ensureCustomObsExample() {
                if (!isObsCommand() || restGridObsActionComboBox.currentIndex !== 3)
                    return;
                if (restGridObsCustomRequestType.text === "")
                    restGridObsCustomRequestType.text = "SetCurrentProgramScene";
                for (var i = 0; i < restParamsModel.count; i++) {
                    if (restParamsModel.get(i).paramName === "sceneName")
                        return;
                }
                restParamsModel.append({"paramName": "sceneName", "paramValue": "Scene"});
            }

            function setObsActionFromRequestType(requestType) {
                if (requestType === "SetCurrentProfile") {
                    restGridObsActionComboBox.currentIndex = 0;
                } else if (requestType === "SetCurrentProgramScene") {
                    restGridObsActionComboBox.currentIndex = 1;
                } else if (requestType === "SetCurrentSceneCollection") {
                    restGridObsActionComboBox.currentIndex = 2;
                } else {
                    restGridObsActionComboBox.currentIndex = 3;
                    restGridObsCustomRequestType.text = requestType;
                    ensureCustomObsExample();
                }
            }

            function updateObsOptions() {
                if (!isObsCommand() || restGridObsActionComboBox.currentIndex === 3 || restUrlField.text === "")
                    return;
                app.wwsClientModel.updateObsOptions(restUrlField.text, restGridObsActionComboBox.currentIndex);
            }

            function loadParametersFromJson(jsonStr) {
                restParamsModel.clear();
                restGridObsConnectCheckBox.checked = false;
                restGridObsActionComboBox.currentIndex = 0;
                restGridObsOptionComboBox.currentIndex = -1;
                restGridObsCustomRequestType.text = "SetCurrentProgramScene";
                if (!jsonStr || jsonStr === "")
                    return;
                try {
                    var arr = JSON.parse(jsonStr);
                    if (Array.isArray(arr)) {
                        for (var i = 0; i < arr.length; i++) {
                                var name = arr[i].name || "";
                                var value = arr[i].value || "";
                                if (name === "requestType") {
                                    restGridObsConnectCheckBox.checked = true;
                                    setObsActionFromRequestType(value);
                                } else if (name.indexOf("requestData.") === 0) {
                                    var dataName = name.substring(12);
                                    if (dataName === "profileName" || dataName === "sceneName" || dataName === "sceneCollectionName") {
                                        restGridObsOptionComboBox.editText = value;
                                    } else {
                                        restParamsModel.append({"paramName": dataName, "paramValue": value});
                                    }
                                } else {
                                    restParamsModel.append({"paramName": name, "paramValue": value});
                                }
                        }
                    }
                } catch(e) {
                    var pairs = jsonStr.split("&");
                    for (var j = 0; j < pairs.length; j++) {
                        var eqIdx = pairs[j].indexOf("=");
                        if (eqIdx >= 0) {
                            restParamsModel.append({"paramName": pairs[j].substring(0, eqIdx), "paramValue": pairs[j].substring(eqIdx + 1)});
                        } else if (pairs[j] !== "") {
                            restParamsModel.append({"paramName": pairs[j], "paramValue": ""});
                        }
                    }
                }
            }

            function getParametersAsJson() {
                var arr = [];
                if (isObsCommand()) {
                    var requestType = obsRequestType();
                    if (requestType !== "") {
                        arr.push({"name": "requestType", "value": requestType});
                        var optionParameterName = obsOptionParameterName();
                        if (optionParameterName !== "" && restGridObsOptionComboBox.currentText !== "") {
                            arr.push({"name": "requestData." + optionParameterName, "value": restGridObsOptionComboBox.currentText});
                        }
                    }
                }
                for (var i = 0; i < restParamsModel.count; i++) {
                    var item = restParamsModel.get(i);
                    if (item.paramName !== "") {
                        var paramName = isObsCommand() && obsRequestType() !== "" ? "requestData." + item.paramName : item.paramName;
                        arr.push({"name": paramName, "value": item.paramValue});
                    }
                }
                if (arr.length === 0)
                    return "";
                return JSON.stringify(arr);
            }

            ListModel {
                id: restParamsModel
            }

            RowLayout {
                Layout.bottomMargin: 5
                Layout.columnSpan: 3

                Rectangle {
                    color: Kirigami.Theme.alternateBackgroundColor
                    height: 1
                    width: Kirigami.Units.gridUnit
                }
                Kirigami.Heading {
                    text: qsTr("REST properties for the layer")
                }
                Rectangle {
                    Layout.fillWidth: true
                    color: Kirigami.Theme.alternateBackgroundColor
                    height: 1
                }
                Item {
                    Layout.fillWidth: true
                }
            }

            Label {
                text: qsTr("URL:")
                Layout.alignment: Qt.AlignRight
            }
            TextField {
                id: restUrlField

                Layout.fillWidth: true
                Layout.columnSpan: 2
                placeholderText: "http://host:port/path or ws://host:port/path"
                text: layerView.layerItem.layerRestUrl

                onEditingFinished: {
                    layerView.layerItem.layerRestUrl = text;
                }

                Connections {
                    function onLayerChanged() {
                        restUrlField.text = layerView.layerItem.layerRestUrl;
                    }
                    target: layerView.layerItem
                }
            }

            Label {
                text: qsTr("Method:")
                Layout.alignment: Qt.AlignRight
            }
            ComboBox {
                id: restGridMethodComboBox

                Layout.fillWidth: true
                Layout.columnSpan: 2
                model: ["GET", "POST", "PUT", "DELETE", "WS", "WSS"]
                currentIndex: layerView.layerItem.layerRestMethod

                onActivated: {
                    layerView.layerItem.layerRestMethod = currentIndex;
                    restGridLayout.updateObsOptions();
                    root.resizeForGridMode();
                }

                Connections {
                    function onLayerChanged() {
                        restGridMethodComboBox.currentIndex = layerView.layerItem.layerRestMethod;
                    }
                    target: layerView.layerItem
                }
            }

            Label {
                text: qsTr("OBS:")
                Layout.alignment: Qt.AlignRight
                visible: restGridMethodComboBox.currentIndex === 4
            }
            CheckBox {
                id: restGridObsConnectCheckBox

                Layout.columnSpan: 2
                visible: restGridMethodComboBox.currentIndex === 4
                text: qsTr("Connecting to OBS")

                onToggled: {
                    restGridLayout.updateObsOptions();
                    root.resizeForGridMode();
                }
            }

            Label {
                text: qsTr("OBS command:")
                Layout.alignment: Qt.AlignRight
                visible: restGridLayout.isObsCommand()
            }
            ComboBox {
                id: restGridObsActionComboBox
                Layout.fillWidth: true
                Layout.columnSpan: 2
                visible: restGridLayout.isObsCommand()
                model: root.restObsActionNames
                onActivated: {
                    restGridObsOptionComboBox.currentIndex = -1;
                    restGridObsOptionComboBox.editText = "";
                    if (restGridObsActionComboBox.currentIndex === 3)
                        restGridLayout.ensureCustomObsExample();
                    restGridLayout.updateObsOptions();
                    root.resizeForGridMode();
                }
            }

            Label {
                text: qsTr("OBS target:")
                Layout.alignment: Qt.AlignRight
                visible: restGridLayout.isObsCommand() && restGridObsActionComboBox.currentIndex !== 3
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.columnSpan: 2
                visible: restGridLayout.isObsCommand() && restGridObsActionComboBox.currentIndex !== 3

                ComboBox {
                    id: restGridObsOptionComboBox
                    Layout.fillWidth: true
                    editable: true
                    model: app.wwsClientModel.obsOptions
                    enabled: !app.wwsClientModel.obsOptionsInProgress
                }
                ToolButton {
                    icon.name: "view-refresh"
                    icon.height: 16
                    enabled: restUrlField.text !== "" && !app.wwsClientModel.obsOptionsInProgress
                    onClicked: restGridLayout.updateObsOptions()
                }
            }

            Label {
                text: qsTr("OBS request:")
                Layout.alignment: Qt.AlignRight
                visible: restGridLayout.isObsCommand() && restGridObsActionComboBox.currentIndex === 3
            }
            TextField {
                id: restGridObsCustomRequestType

                Layout.fillWidth: true
                Layout.columnSpan: 2
                visible: restGridLayout.isObsCommand() && restGridObsActionComboBox.currentIndex === 3
                text: "SetCurrentProgramScene"
            }

            Label {
                text: qsTr("Ignore Status:")
                Layout.alignment: Qt.AlignRight
            }
            CheckBox {
                id: restGridIgnoreStatusCheckBox

                Layout.columnSpan: 2
                checked: layerView.layerItem.layerRestIgnoreStatus
                text: qsTr("Do not wait for response")

                onToggled: {
                    layerView.layerItem.layerRestIgnoreStatus = checked;
                }

                Connections {
                    function onLayerChanged() {
                        restGridIgnoreStatusCheckBox.checked = layerView.layerItem.layerRestIgnoreStatus;
                    }
                    target: layerView.layerItem
                }
            }

            Label {
                text: restGridLayout.isObsCommand() && restGridLayout.obsRequestType() !== "" ? qsTr("Request data:") : qsTr("Parameters:")
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.columnSpan: 2
                spacing: 4

                Repeater {
                    model: restParamsModel
                    delegate: RowLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        TextField {
                            Layout.preferredWidth: 120
                            placeholderText: "Name"
                            text: paramName
                            onTextChanged: restParamsModel.setProperty(index, "paramName", text)
                        }
                        Label { text: "=" }
                        TextField {
                            Layout.fillWidth: true
                            placeholderText: "Value"
                            text: paramValue
                            onTextChanged: restParamsModel.setProperty(index, "paramValue", text)
                        }
                        ToolButton {
                            icon.name: "list-remove"
                            icon.height: 16
                            onClicked: {
                                restParamsModel.remove(index);
                                restGridLayout.updateLayerParameters();
                            }
                        }
                    }
                }

                RowLayout {
                    Button {
                        text: qsTr("+ Add")
                        icon.name: "list-add"
                        onClicked: restParamsModel.append({"paramName": "", "paramValue": ""})
                    }
                    Button {
                        text: qsTr("Apply")
                        icon.name: "document-save"
                        onClicked: {
                            restGridLayout.updateLayerParameters();
                        }
                    }
                }
            }

            Connections {
                function onLayerChanged() {
                    restGridLayout.loadParametersFromJson(layerView.layerItem.layerRestParameters);
                }
                target: layerView.layerItem
            }
        }
        } // ColumnLayout
    }
}
