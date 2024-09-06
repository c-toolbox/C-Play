/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

import org.kde.kirigami 2.15 as Kirigami
import org.ctoolbox.cplay 1.0
import Haruna.Components 1.0

SettingsBasePage {
    id: root

    GridLayout {
        id: content

        columns: 3

        // ------------------------------------
        // GRID PARAMETERS
        // --
        SettingsHeader {
            Layout.columnSpan: 3
            Layout.fillWidth: true
            text: qsTr("Grid/mapping parameters")
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
                    if (planeSizeBasedOnMode.get(i).value === GridSettings.plane_Calculate_Size_Based_on_Video) {
                        currentIndex = i;
                        break;
                    }
                }
            }
            onActivated: {
                GridSettings.plane_Calculate_Size_Based_on_Video = model.get(index).value;
                GridSettings.save();
                mpv.planeChanged;
            }
        }
        Label {
            text: qsTr("Plane width:")
        }
        RowLayout {
            Layout.columnSpan: 2

            SpinBox {
                id: planeWidthBox

                enabled: GridSettings.plane_Calculate_Size_Based_on_Video !== 1
                from: 0
                stepSize: 1
                to: 2000
                value: mpv.planeWidth
            }
            LabelWithTooltip {
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

                enabled: GridSettings.plane_Calculate_Size_Based_on_Video !== 2
                from: 0
                stepSize: 1
                to: 2000
                value: mpv.planeHeight
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
            text: qsTr("Plane elevation:")
        }
        RowLayout {
            Layout.columnSpan: 2

            SpinBox {
                id: planeElevationBox

                from: 0
                to: 360
                value: mpv.planeElevation

                onValueChanged: mpv.planeElevation = value
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
            text: qsTr("Plane distance:")
        }
        RowLayout {
            Layout.columnSpan: 2

            SpinBox {
                id: planeDistanceBox

                from: 0
                stepSize: 1
                to: 2000
                value: mpv.planeDistance

                onValueChanged: mpv.planeDistance = value
            }
            LabelWithTooltip {
                Layout.alignment: Qt.AlignLeft
                Layout.fillWidth: true
                elide: Text.ElideLeft
                text: {
                    qsTr("cm");
                }
            }
        }
        Label {
            Layout.alignment: Qt.AlignRight
            Layout.columnSpan: 3
            text: qsTr("Alternative Transition Scenario:")
        }

        // ------------------------------------
        // Dome/sphere radius
        // ------------------------------------
        Label {
            text: qsTr("Dome/Sphere radius:")
        }
        RowLayout {
            SpinBox {
                id: surfaceRadius

                from: 0
                stepSize: 1
                to: 2000
                value: mpv.radius

                onValueChanged: mpv.radius = value
            }
            LabelWithTooltip {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: {
                    qsTr("cm");
                }
            }
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight

            SpinBox {
                id: surfaceRadiusScenario

                from: 0
                stepSize: 1
                to: 2000
                value: GridSettings.surfaceRadius_2ndState
            }
        }

        // ------------------------------------
        // Dome fov
        // ------------------------------------
        Label {
            text: qsTr("Field of view (fov):")
        }
        RowLayout {
            SpinBox {
                id: surfaceFov

                from: 0
                to: 360
                value: mpv.fov

                onValueChanged: mpv.fov = value
            }
            LabelWithTooltip {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: {
                    qsTr("degrees");
                }
            }
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight

            SpinBox {
                id: surfaceFovScenario

                from: 0
                to: 360
                value: GridSettings.surfaceFov_2ndState
            }
        }

        // ------------------------------------
        // Dome angle
        // ------------------------------------
        Label {
            text: qsTr("Dome angle:")
        }
        RowLayout {
            SpinBox {
                id: surfaceAngle

                from: 0
                to: 360
                value: mpv.angle

                onValueChanged: mpv.angle = value
            }
            LabelWithTooltip {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: {
                    qsTr("degrees");
                }
            }
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight

            SpinBox {
                id: surfaceAngleScenario

                from: 0
                to: 360
                value: GridSettings.surfaceAngle_2ndState
            }
        }

        // ------------------------------------
        // Dome translate X
        // ------------------------------------
        Label {
            text: qsTr("Translate X:")
        }
        RowLayout {
            SpinBox {
                id: surfaceTranslateX

                from: -5000
                to: 5000
                value: mpv.translate.x

                onValueChanged: mpv.translate.x = value
            }
            LabelWithTooltip {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: {
                    qsTr("");
                }
            }
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight

            SpinBox {
                id: surfaceTranslateXScenario

                from: -5000
                to: 5000
                value: GridSettings.surfaceTranslateX_2ndState
            }
        }

        // ------------------------------------
        // Dome translate Y
        // ------------------------------------
        Label {
            text: qsTr("Translate Y:")
        }
        RowLayout {
            SpinBox {
                id: surfaceTranslateY

                from: -5000
                to: 5000
                value: mpv.translate.y

                onValueChanged: mpv.translate.y = value
            }
            LabelWithTooltip {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: {
                    qsTr("");
                }
            }
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight

            SpinBox {
                id: surfaceTranslateYScenario

                from: -5000
                to: 5000
                value: GridSettings.surfaceTranslateY_2ndState
            }
        }

        // ------------------------------------
        // Dome translate Z
        // ------------------------------------
        Label {
            text: qsTr("Translate Z:")
        }
        RowLayout {
            SpinBox {
                id: surfaceTranslateZ

                from: -5000
                to: 5000
                value: mpv.translate.z

                onValueChanged: mpv.translate.z = value
            }
            LabelWithTooltip {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: {
                    qsTr("");
                }
            }
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight

            SpinBox {
                id: surfaceTranslateZScenario

                from: -5000
                to: 5000
                value: GridSettings.surfaceTranslateZ_2ndState
            }
        }

        // ------------------------------------
        // RotateX
        // ------------------------------------
        Label {
            text: qsTr("Rotate around X: ")
        }
        RowLayout {
            Layout.columnSpan: 2

            Slider {
                id: rotateXSlider

                Layout.topMargin: Kirigami.Units.largeSpacing
                enabled: mpv.gridToMapOn < 3
                from: -18000
                to: 18000
                value: mpv.rotate.x * 100

                onMoved: {
                    rotateXSpinBox.value = value.toFixed(0);
                    mpv.rotate.x = rotateXSpinBox.realValue;
                }
            }
            SpinBox {
                id: rotateXSpinBox

                property int decimals: 2
                property real realValue: value / 100

                enabled: mpv.gridToMapOn < 3
                from: -18000
                stepSize: 10
                textFromValue: function (value, locale) {
                    return Number(value / 100).toLocaleString(locale, 'f', rotateXSpinBox.decimals);
                }
                to: 18000
                value: mpv.rotate.x * 100
                valueFromText: function (text, locale) {
                    return Number.fromLocaleString(locale, text) * 100;
                }

                validator: DoubleValidator {
                    bottom: Math.min(rotateXSpinBox.from, rotateXSpinBox.to)
                    top: Math.max(rotateXSpinBox.from, rotateXSpinBox.to)
                }

                onValueModified: {
                    mpv.rotate.x = realValue;
                    rotateXSlider.value = value;
                }

                Connections {
                    function onRotateChanged() {
                        if (rotateXSpinBox.realValue !== mpv.rotate.x)
                            rotateXSpinBox.value = mpv.rotate.x * 100;
                    }

                    target: mpv
                }
            }
            Label {
                Layout.alignment: Qt.AlignLeft
                Layout.fillWidth: true
                text: {
                    qsTr("degrees");
                }
            }
        }

        // ------------------------------------
        // RotateY
        // ------------------------------------
        Label {
            text: qsTr("Rotate around Y: ")
        }
        RowLayout {
            Layout.columnSpan: 2

            Slider {
                id: rotateYSlider

                Layout.topMargin: Kirigami.Units.largeSpacing
                enabled: mpv.gridToMapOn < 3
                from: -18000
                to: 18000
                value: mpv.rotate.y * 100

                onMoved: {
                    rotateYSpinBox.value = value.toFixed(0);
                    mpv.rotate.y = rotateYSpinBox.realValue;
                }
            }
            SpinBox {
                id: rotateYSpinBox

                property int decimals: 2
                property real realValue: value / 100

                enabled: mpv.gridToMapOn < 3
                from: -18000
                stepSize: 10
                textFromValue: function (value, locale) {
                    return Number(value / 100).toLocaleString(locale, 'f', rotateYSpinBox.decimals);
                }
                to: 18000
                value: mpv.rotate.y * 100
                valueFromText: function (text, locale) {
                    return Number.fromLocaleString(locale, text) * 100;
                }

                validator: DoubleValidator {
                    bottom: Math.min(rotateYSpinBox.from, rotateYSpinBox.to)
                    top: Math.max(rotateYSpinBox.from, rotateYSpinBox.to)
                }

                onValueModified: {
                    mpv.rotate.y = realValue;
                    rotateYSlider.value = value;
                }

                Connections {
                    function onRotateChanged() {
                        if (rotateYSpinBox.realValue !== mpv.rotate.y)
                            rotateYSpinBox.value = mpv.rotate.y * 100;
                    }

                    target: mpv
                }
            }
            Label {
                Layout.alignment: Qt.AlignLeft
                Layout.fillWidth: true
                text: {
                    qsTr("degrees");
                }
            }
        }

        // ------------------------------------
        // RotateZ
        // ------------------------------------
        Label {
            text: qsTr("Rotate around Z: ")
        }
        RowLayout {
            Layout.columnSpan: 2

            Slider {
                id: rotateZSlider

                Layout.topMargin: Kirigami.Units.largeSpacing
                enabled: mpv.gridToMapOn < 3
                from: -18000
                to: 18000
                value: mpv.rotate.z * 100

                onMoved: {
                    rotateZSpinBox.value = value.toFixed(0);
                    mpv.rotate.z = rotateZSpinBox.realValue;
                }
            }
            SpinBox {
                id: rotateZSpinBox

                property int decimals: 2
                property real realValue: value / 100

                enabled: mpv.gridToMapOn < 3
                from: -18000
                stepSize: 10
                textFromValue: function (value, locale) {
                    return Number(value / 100).toLocaleString(locale, 'f', rotateZSpinBox.decimals);
                }
                to: 18000
                value: mpv.rotate.z * 100
                valueFromText: function (text, locale) {
                    return Number.fromLocaleString(locale, text) * 100;
                }

                validator: DoubleValidator {
                    bottom: Math.min(rotateZSpinBox.from, rotateZSpinBox.to)
                    top: Math.max(rotateZSpinBox.from, rotateZSpinBox.to)
                }

                onValueModified: {
                    mpv.rotate.z = realValue;
                    rotateZSlider.value = value;
                }

                Connections {
                    function onRotateChanged() {
                        if (rotateZSpinBox.realValue !== mpv.rotate.z)
                            rotateZSpinBox.value = mpv.rotate.z * 100;
                    }

                    target: mpv
                }
            }
            Label {
                Layout.alignment: Qt.AlignLeft
                Layout.fillWidth: true
                text: {
                    qsTr("degrees");
                }
            }
        }

        // ------------------------------------
        // Reset
        // ------------------------------------

        RowLayout {
            Layout.columnSpan: 2
            Layout.fillWidth: true

            Button {
                text: "Reset radius/fov/rotation settings to startup values"

                onClicked: {
                    mpv.planeWidth = GridSettings.planeWidth;
                    mpv.planeHeight = GridSettings.planeHeight;
                    mpv.planeElevation = GridSettings.planeElevation;
                    mpv.planeDistance = GridSettings.planeDistance;
                    mpv.rotationSpeed = GridSettings.surfaceRotationSpeed;
                    mpv.radius = GridSettings.surfaceRadius;
                    mpv.fov = GridSettings.surfaceFov;
                    mpv.angle = GridSettings.surfaceAngle;
                    mpv.rotate = Qt.vector3d(GridSettings.surfaceRotateX, GridSettings.surfaceRotateY, GridSettings.surfaceRotateZ);
                    mpv.translate = Qt.vector3d(GridSettings.surfaceTranslateX, GridSettings.surfaceTranslateY, GridSettings.surfaceTranslateZ);
                    mpv.surfaceTransitionTime = GridSettings.surfaceTransitionTime;
                    surfaceRadiusScenario.value = GridSettings.surfaceRadius_2ndState;
                    surfaceFovScenario.value = GridSettings.surfaceFov_2ndState;
                    surfaceAngleScenario.value = GridSettings.surfaceAngle_2ndState;
                    surfaceTranslateXScenario.value = GridSettings.surfaceTranslateX_2ndState;
                    surfaceTranslateYScenario.value = GridSettings.surfaceTranslateY_2ndState;
                    surfaceTranslateZScenario.value = GridSettings.surfaceTranslateZ_2ndState;
                }
            }
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight

            Label {
                text: qsTr("Transition time:")
            }
            SpinBox {
                id: transitionTime

                from: 0
                stepSize: 1
                to: 20

                Component.onCompleted: transitionTime.value = mpv.surfaceTransitionTime
                onValueChanged: mpv.surfaceTransitionTime = value
            }
            LabelWithTooltip {
                elide: Text.ElideRight
                text: {
                    qsTr("s");
                }
            }
        }
        RowLayout {
            Layout.columnSpan: 2
            Layout.fillWidth: true

            Button {
                text: "Save current radius/fov/rotation settings to load on startup"

                onClicked: {
                    GridSettings.planeWidth = mpv.planeWidth;
                    GridSettings.planeHeight = mpv.planeHeight;
                    GridSettings.planeElevation = mpv.planeElevation;
                    GridSettings.planeDistance = mpv.planeDistance;
                    GridSettings.surfaceRotationSpeed = mpv.rotationSpeed;
                    GridSettings.surfaceRadius = mpv.radius;
                    GridSettings.surfaceFov = mpv.fov;
                    GridSettings.surfaceAngle = mpv.angle;
                    GridSettings.surfaceRotateX = mpv.rotate.x;
                    GridSettings.surfaceRotateY = mpv.rotate.y;
                    GridSettings.surfaceRotateZ = mpv.rotate.z;
                    GridSettings.surfaceTranslateX = mpv.translate.x;
                    GridSettings.surfaceTranslateY = mpv.translate.y;
                    GridSettings.surfaceTranslateZ = mpv.translate.z;
                    GridSettings.surfaceTransitionTime = mpv.surfaceTransitionTime;
                    GridSettings.surfaceRadius_2ndState = surfaceRadiusScenario.value;
                    GridSettings.surfaceFov_2ndState = surfaceFovScenario.value;
                    GridSettings.surfaceAngle_2ndState = surfaceAngleScenario.value;
                    GridSettings.surfaceTranslateX_2ndState = surfaceTranslateXScenario.value;
                    GridSettings.surfaceTranslateY_2ndState = surfaceTranslateYScenario.value;
                    GridSettings.surfaceTranslateZ_2ndState = surfaceTranslateZScenario.value;
                    GridSettings.save();
                }
            }
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight

            Button {
                id: startTransitionButton

                enabled: !mpv.surfaceTransitionOnGoing
                text: qsTr("Start transition")

                onClicked: {
                    surfaceRadiusAnimation.to = surfaceRadiusScenario.value;
                    surfaceRadiusAnimation.duration = transitionTime.value * 1000;
                    surfaceRadiusScenario.value = surfaceRadius.value;
                    surfaceFovAnimation.to = surfaceFovScenario.value;
                    surfaceFovAnimation.duration = transitionTime.value * 1000;
                    surfaceFovScenario.value = surfaceFov.value;
                    surfaceAngleAnimation.to = surfaceAngleScenario.value;
                    surfaceAngleAnimation.duration = transitionTime.value * 1000;
                    surfaceAngleScenario.value = surfaceAngle.value;
                    surfaceTranslateXAnimation.to = surfaceTranslateXScenario.value;
                    surfaceTranslateXAnimation.duration = transitionTime.value * 1000;
                    surfaceTranslateXScenario.value = surfaceTranslateX.value;
                    surfaceTranslateYAnimation.to = surfaceTranslateYScenario.value;
                    surfaceTranslateYAnimation.duration = transitionTime.value * 1000;
                    surfaceTranslateYScenario.value = surfaceTranslateY.value;
                    surfaceTranslateZAnimation.to = surfaceTranslateZScenario.value;
                    surfaceTranslateZAnimation.duration = transitionTime.value * 1000;
                    surfaceTranslateZScenario.value = surfaceTranslateZ.value;
                    gridAnimations.start();
                    mpv.surfaceTransitionOnGoing = true;
                }
                onEnabledChanged: {
                    if (enabled) {
                        startTransitionButton.text = qsTr("Start transition");
                    } else {
                        startTransitionButton.text = qsTr("Transition ongoing");
                    }
                }

                ParallelAnimation {
                    id: gridAnimations

                    onFinished: mpv.surfaceTransitionOnGoing = false

                    NumberAnimation {
                        id: surfaceRadiusAnimation

                        duration: 1000
                        property: "value"
                        target: surfaceRadius
                        to: 50
                    }
                    NumberAnimation {
                        id: surfaceFovAnimation

                        duration: 1000
                        property: "value"
                        target: surfaceFov
                        to: 50
                    }
                    NumberAnimation {
                        id: surfaceAngleAnimation

                        duration: 1000
                        property: "value"
                        target: surfaceAngle
                        to: 50
                    }
                    NumberAnimation {
                        id: surfaceTranslateXAnimation

                        duration: 1000
                        property: "value"
                        target: surfaceTranslateX
                        to: 50
                    }
                    NumberAnimation {
                        id: surfaceTranslateYAnimation

                        duration: 1000
                        property: "value"
                        target: surfaceTranslateY
                        to: 50
                    }
                    NumberAnimation {
                        id: surfaceTranslateZAnimation

                        duration: 1000
                        property: "value"
                        target: surfaceTranslateZ
                        to: 50
                    }
                }
            }
            Connections {
                function onSurfaceTransitionPerformed() {
                    startTransitionButton.clicked();
                }

                target: mpv
            }
        }
    }
}
