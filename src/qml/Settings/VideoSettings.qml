/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.12
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.12

import org.kde.kirigami 2.11 as Kirigami
import org.ctoolbox.cplay 1.0
import Haruna.Components 1.0

SettingsBasePage {
    id: root

    hasHelp: true
    helpFile: ":/VideoSettings.html"

    GridLayout {
        id: content

        columns: 3

        // ------------------------------------
        // GRID PARAMETERS
        // --
        SettingsHeader {
            text: qsTr("Mapping/grid parameters")
            Layout.columnSpan: 3
            Layout.fillWidth: true
        }

        // ------------------------------------
        // Plane parameters
        // ------------------------------------
        Label {
            text: qsTr("Determine plane size based on:")
        }
        ComboBox {
            id: planeSizeBasedOnComboBox
            enabled: true
            textRole: "mode"
            Layout.columnSpan: 2
            model: ListModel {
                id: planeSizeBasedOnMode
                ListElement { mode: "Values below"; value: 0 }
                ListElement { mode: "Height below and video aspect ratio"; value: 1 }
                ListElement { mode: "Width below and video aspect ratio"; value: 2 }
            }

            onActivated: {
                VideoSettings.plane_Calculate_Size_Based_on_Video = model.get(index).value
                VideoSettings.save()
                mpv.planeChanged
            }

            Component.onCompleted: {
                for (let i = 0; i < planeSizeBasedOnMode.count; ++i) {
                    if (planeSizeBasedOnMode.get(i).value === VideoSettings.plane_Calculate_Size_Based_on_Video) {
                        currentIndex = i
                        break
                    }
                }
            }
        }

        Label {
            text: qsTr("Plane width:")
        }

        RowLayout {
            SpinBox {
                id: planeWidthBox
                from: 0
                to: 2000
                enabled: VideoSettings.plane_Calculate_Size_Based_on_Video !== 1
                stepSize: 1
                value: mpv.planeWidth
            }

            LabelWithTooltip {
                text: {
                    qsTr("cm")
                }
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Layout.columnSpan: 2
        }

        Label {
            text: qsTr("Plane height:")
        }

        RowLayout {
            SpinBox {
                id: planeHeightBox
                from: 0
                to: 2000
                enabled: VideoSettings.plane_Calculate_Size_Based_on_Video !== 2
                stepSize: 1
                value: mpv.planeHeight
            }

            LabelWithTooltip {
                text: {
                    qsTr("cm")
                }
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Layout.columnSpan: 2
        }

        Label {
            text: qsTr("Plane elevation:")
        }

        RowLayout {
            SpinBox {
                id: planeElevationBox
                from: 0
                to: 360
                value: mpv.planeElevation
                onValueChanged: mpv.planeElevation = value
            }

            LabelWithTooltip {
                text: {
                    qsTr("degrees")
                }
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Layout.columnSpan: 2
        }

        Label {
            text: qsTr("Plane distance:")
        }

        RowLayout {
            SpinBox {
                id: planeDistanceBox
                from: 0
                to: 2000
                stepSize: 1
                value: mpv.planeDistance
                onValueChanged: mpv.planeDistance = value
            }

            LabelWithTooltip {
                text: {
                    qsTr("cm")
                }
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Layout.columnSpan: 2
        }

        Label {
            text: qsTr("Alternative Transition Scenario:")
            Layout.alignment: Qt.AlignRight
            Layout.columnSpan: 3
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
                to: 2000
                stepSize: 1
                value: mpv.radius
                onValueChanged: mpv.radius = value
            }

            LabelWithTooltip {
                text: {
                    qsTr("cm")
                }
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        RowLayout {
            SpinBox {
                id: surfaceRadiusScenario
                from: 0
                to: 2000
                stepSize: 1
                value: VideoSettings.surfaceRadius_2ndState
            }
            Layout.alignment: Qt.AlignRight
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
                text: {
                    qsTr("degrees")
                }
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        RowLayout {
            SpinBox {
                id: surfaceFovScenario
                from: 0
                to: 360
                value: VideoSettings.surfaceFov_2ndState
            }
            Layout.alignment: Qt.AlignRight
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
                text: {
                    qsTr("degrees")
                }
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        RowLayout {
            SpinBox {
                id: surfaceAngleScenario
                from: 0
                to: 360
                value: VideoSettings.surfaceAngle_2ndState
            }
            Layout.alignment: Qt.AlignRight
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
                text: {
                    qsTr("")
                }
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        RowLayout {
            SpinBox {
                id: surfaceTranslateXScenario
                from: -5000
                to: 5000
                value: VideoSettings.surfaceTranslateX_2ndState
            }
            Layout.alignment: Qt.AlignRight
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
                text: {
                    qsTr("")
                }
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        RowLayout {
            SpinBox {
                id: surfaceTranslateYScenario
                from: -5000
                to: 5000
                value: VideoSettings.surfaceTranslateY_2ndState
            }
            Layout.alignment: Qt.AlignRight
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
                text: {
                    qsTr("")
                }
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        RowLayout {
            SpinBox {
                id: surfaceTranslateZScenario
                from: -5000
                to: 5000
                value: VideoSettings.surfaceTranslateZ_2ndState
            }
            Layout.alignment: Qt.AlignRight
        }

        // ------------------------------------
        // RotateX
        // ------------------------------------
        Label {
            text: qsTr("Rotate around X: ")
        }

        RowLayout {
            Slider {
                id: rotateXSlider
                value: mpv.rotate.x * 100
                from: -18000
                to: 18000
                enabled: mpv.gridToMapOn < 3
                onMoved: {
                    rotateXSpinBox.value = value.toFixed(0)
                    mpv.rotate.x = rotateXSpinBox.realValue
                }
                Layout.topMargin: Kirigami.Units.largeSpacing
            }
            SpinBox {
                id: rotateXSpinBox
                from: -18000
                value: mpv.rotate.x * 100
                to: 18000
                enabled: mpv.gridToMapOn < 3
                stepSize: 10

                property int decimals: 2
                property real realValue: value / 100

                validator: DoubleValidator {
                    bottom: Math.min(rotateXSpinBox.from, rotateXSpinBox.to)
                    top:  Math.max(rotateXSpinBox.from, rotateXSpinBox.to)
                }

                textFromValue: function(value, locale) {
                    return Number(value / 100).toLocaleString(locale, 'f', rotateXSpinBox.decimals)
                }

                valueFromText: function(text, locale) {
                    return Number.fromLocaleString(locale, text) * 100
                }

                onValueModified: {
                    mpv.rotate.x = realValue
                    rotateXSlider.value = value
                }

                Connections {
                    target: mpv
                    function onRotateChanged() {
                        if(rotateXSpinBox.realValue !== mpv.rotate.x)
                            rotateXSpinBox.value = mpv.rotate.x * 100
                    }
                }
            }
        }
        RowLayout {
            LabelWithTooltip {
                text: {
                    qsTr("degrees")
                }
                elide: Text.ElideLeft
                Layout.fillWidth: true
                Layout.columnSpan: 2

            }
            /*SpinBox {
                id: rotateXSpinBoxScenario
                from: -180
                to: 180
                value: -90
            }*/
        }

        // ------------------------------------
        // RotateY
        // ------------------------------------
        Label {
            text: qsTr("Rotate around Y: ")
        }

        RowLayout {
            Slider {
                id: rotateYSlider
                value: mpv.rotate.y * 100
                from: -18000
                to: 18000
                enabled: mpv.gridToMapOn < 3
                onMoved: {
                    rotateYSpinBox.value = value.toFixed(0)
                    mpv.rotate.y = rotateYSpinBox.realValue
                }
                Layout.topMargin: Kirigami.Units.largeSpacing
            }
            SpinBox {
                id: rotateYSpinBox
                from: -18000
                value: mpv.rotate.y * 100
                to: 18000
                enabled: mpv.gridToMapOn < 3
                stepSize: 10

                property int decimals: 2
                property real realValue: value / 100

                validator: DoubleValidator {
                    bottom: Math.min(rotateYSpinBox.from, rotateYSpinBox.to)
                    top:  Math.max(rotateYSpinBox.from, rotateYSpinBox.to)
                }

                textFromValue: function(value, locale) {
                    return Number(value / 100).toLocaleString(locale, 'f', rotateYSpinBox.decimals)
                }

                valueFromText: function(text, locale) {
                    return Number.fromLocaleString(locale, text) * 100
                }

                onValueModified: {
                    mpv.rotate.y = realValue
                    rotateYSlider.value = value
                }

                Connections {
                    target: mpv
                    function onRotateChanged() {
                        if(rotateYSpinBox.realValue !== mpv.rotate.y)
                            rotateYSpinBox.value = mpv.rotate.y * 100
                    }
                }
            }
        }
        RowLayout {
            LabelWithTooltip {
                text: {
                    qsTr("degrees")
                }
                elide: Text.ElideLeft
                Layout.fillWidth: true
                Layout.columnSpan: 2

            }
            /*SpinBox {
                id: rotateYSpinBoxScenario
                from: -180
                to: 180
                value: -90
            }*/
        }

        // ------------------------------------
        // RotateZ
        // ------------------------------------
        Label {
            text: qsTr("Rotate around Z: ")
        }

        RowLayout {
            Slider {
                id: rotateZSlider
                value: mpv.rotate.z * 100
                from: -18000
                to: 18000
                enabled: mpv.gridToMapOn < 3
                onMoved: {
                    rotateZSpinBox.value = value.toFixed(0)
                    mpv.rotate.z = rotateZSpinBox.realValue
                }
                Layout.topMargin: Kirigami.Units.largeSpacing
            }
            SpinBox {
                id: rotateZSpinBox
                from: -18000
                value: mpv.rotate.z * 100
                to: 18000
                enabled: mpv.gridToMapOn < 3
                stepSize: 10

                property int decimals: 2
                property real realValue: value / 100

                validator: DoubleValidator {
                    bottom: Math.min(rotateZSpinBox.from, rotateZSpinBox.to)
                    top:  Math.max(rotateZSpinBox.from, rotateZSpinBox.to)
                }

                textFromValue: function(value, locale) {
                    return Number(value / 100).toLocaleString(locale, 'f', rotateZSpinBox.decimals)
                }

                valueFromText: function(text, locale) {
                    return Number.fromLocaleString(locale, text) * 100
                }

                onValueModified: {
                    mpv.rotate.z = realValue
                    rotateZSlider.value = value
                }

                Connections {
                    target: mpv
                    function onRotateChanged() {
                        if(rotateZSpinBox.realValue !== mpv.rotate.z)
                            rotateZSpinBox.value = mpv.rotate.z * 100
                    }
                }
            }
        }
        RowLayout {
            LabelWithTooltip {
                text: {
                    qsTr("degrees")
                }
                elide: Text.ElideLeft
                Layout.fillWidth: true
                Layout.columnSpan: 2

            }
            /*SpinBox {
                id: rotateZSpinBoxScenario
                from: -180
                to: 180
                value: -90
            }*/
        }

        // ------------------------------------
        // Reset
        // ------------------------------------

        RowLayout {
            Button {
                    text: "Reset radius/fov/rotation settings to startup values"
                    onClicked: {
                        mpv.planeWidth = VideoSettings.planeWidth
                        mpv.planeHeight = VideoSettings.planeHeight
                        mpv.planeElevation = VideoSettings.planeElevation
                        mpv.planeDistance = VideoSettings.planeDistance
                        mpv.rotationSpeed = VideoSettings.surfaceRotationSpeed
                        mpv.radius = VideoSettings.surfaceRadius
                        mpv.fov = VideoSettings.surfaceFov
                        mpv.angle = VideoSettings.surfaceAngle
                        mpv.rotate = Qt.vector3d(VideoSettings.surfaceRotateX, VideoSettings.surfaceRotateY, VideoSettings.surfaceRotateZ)
                        mpv.translate = Qt.vector3d(VideoSettings.surfaceTranslateX, VideoSettings.surfaceTranslateY, VideoSettings.surfaceTranslateZ)
                        mpv.surfaceTransitionTime = VideoSettings.surfaceTransitionTime
                        surfaceRadiusScenario.value = VideoSettings.surfaceRadius_2ndState
                        surfaceFovScenario.value = VideoSettings.surfaceFov_2ndState
                        surfaceAngleScenario.value = VideoSettings.surfaceAngle_2ndState
                        surfaceTranslateXScenario.value = VideoSettings.surfaceTranslateX_2ndState
                        surfaceTranslateYScenario.value = VideoSettings.surfaceTranslateY_2ndState
                        surfaceTranslateZScenario.value = VideoSettings.surfaceTranslateZ_2ndState
                    }
            }
            Layout.columnSpan: 2
            Layout.fillWidth: true
        }

        RowLayout {
            Label {
                text: qsTr("Transition time:")
            }
            SpinBox {
                id: transitionTime
                from: 0
                to: 20
                stepSize: 1
                value: mpv.surfaceTransitionTime
                onValueChanged: mpv.surfaceTransitionTime = value
            }
            LabelWithTooltip {
                text: {
                    qsTr("s")
                }
                elide: Text.ElideRight
            }
            Layout.alignment: Qt.AlignRight
        }

        RowLayout {
            Button {
                    text: "Save current radius/fov/rotation settings to load on startup"
                    onClicked: {
                        VideoSettings.planeWidth = mpv.planeWidth
                        VideoSettings.planeHeight = mpv.planeHeight
                        VideoSettings.planeElevation = mpv.planeElevation
                        VideoSettings.planeDistance = mpv.planeDistance
                        VideoSettings.surfaceRotationSpeed = mpv.rotationSpeed
                        VideoSettings.surfaceRadius = mpv.radius
                        VideoSettings.surfaceFov = mpv.fov
                        VideoSettings.surfaceAngle = mpv.angle
                        VideoSettings.surfaceRotateX = mpv.rotate.x
                        VideoSettings.surfaceRotateY = mpv.rotate.y
                        VideoSettings.surfaceRotateZ = mpv.rotate.z
                        VideoSettings.surfaceTranslateX = mpv.translate.x
                        VideoSettings.surfaceTranslateY = mpv.translate.y
                        VideoSettings.surfaceTranslateZ = mpv.translate.z
                        VideoSettings.surfaceTransitionTime = mpv.surfaceTransitionTime
                        VideoSettings.surfaceRadius_2ndState = surfaceRadiusScenario.value
                        VideoSettings.surfaceFov_2ndState = surfaceFovScenario.value
                        VideoSettings.surfaceAngle_2ndState = surfaceAngleScenario.value
                        VideoSettings.surfaceTranslateX_2ndState = surfaceTranslateXScenario.value
                        VideoSettings.surfaceTranslateY_2ndState = surfaceTranslateYScenario.value
                        VideoSettings.surfaceTranslateZ_2ndState = surfaceTranslateZScenario.value
                        VideoSettings.save()
                    }
            }
            Layout.columnSpan: 2
            Layout.fillWidth: true
        }

        RowLayout {
            Button {
                    id: startTransitionButton
                    text: qsTr("Start transition")
                    enabled: !mpv.surfaceTransitionOnGoing
                    onEnabledChanged: {
                        if(enabled){
                            startTransitionButton.text = qsTr("Start transition")
                        }
                        else{
                            startTransitionButton.text = qsTr("Transition ongoing")
                        }
                    }

                    ParallelAnimation {
                            id: gridAnimations
                            onFinished: mpv.surfaceTransitionOnGoing = false
                            NumberAnimation
                            {
                                id: surfaceRadiusAnimation
                                target: surfaceRadius
                                property: "value"
                                to: 50;
                                duration: 1000
                            }
                            NumberAnimation
                            {
                                id: surfaceFovAnimation
                                target: surfaceFov
                                property: "value"
                                to: 50;
                                duration: 1000
                            }
                            NumberAnimation
                            {
                                id: surfaceAngleAnimation
                                target: surfaceAngle
                                property: "value"
                                to: 50;
                                duration: 1000
                            }
                            NumberAnimation
                            {
                                id: surfaceTranslateXAnimation
                                target: surfaceTranslateX
                                property: "value"
                                to: 50;
                                duration: 1000
                            }
                            NumberAnimation
                            {
                                id: surfaceTranslateYAnimation
                                target: surfaceTranslateY
                                property: "value"
                                to: 50;
                                duration: 1000
                            }
                            NumberAnimation
                            {
                                id: surfaceTranslateZAnimation
                                target: surfaceTranslateZ
                                property: "value"
                                to: 50;
                                duration: 1000
                            }
                    }

                    onClicked: {
                        surfaceRadiusAnimation.to = surfaceRadiusScenario.value
                        surfaceRadiusAnimation.duration = transitionTime.value * 1000
                        surfaceRadiusScenario.value = surfaceRadius.value

                        surfaceFovAnimation.to = surfaceFovScenario.value
                        surfaceFovAnimation.duration = transitionTime.value * 1000
                        surfaceFovScenario.value = surfaceFov.value

                        surfaceAngleAnimation.to = surfaceAngleScenario.value
                        surfaceAngleAnimation.duration = transitionTime.value * 1000
                        surfaceAngleScenario.value = surfaceAngle.value

                        surfaceTranslateXAnimation.to = surfaceTranslateXScenario.value
                        surfaceTranslateXAnimation.duration = transitionTime.value * 1000
                        surfaceTranslateXScenario.value = surfaceTranslateX.value

                        surfaceTranslateYAnimation.to = surfaceTranslateYScenario.value
                        surfaceTranslateYAnimation.duration = transitionTime.value * 1000
                        surfaceTranslateYScenario.value = surfaceTranslateY.value

                        surfaceTranslateZAnimation.to = surfaceTranslateZScenario.value
                        surfaceTranslateZAnimation.duration = transitionTime.value * 1000
                        surfaceTranslateZScenario.value = surfaceTranslateZ.value

                        gridAnimations.start()

                        mpv.surfaceTransitionOnGoing = true
                    }
            }
            Layout.alignment: Qt.AlignRight

            Connections {
                target: mpv
                function onSurfaceTransitionPerformed() {
                    startTransitionButton.clicked()
                }
            }
        }

        SettingsHeader {
            text: qsTr("Image adjustments")
            Layout.columnSpan: 3
            Layout.fillWidth: true
        }


        // ------------------------------------
        // CONTRAST
        // ------------------------------------
        Label {
            text: qsTr("Contrast")
            Layout.alignment: Qt.AlignRight
        }

        ImageAdjustmentSlider {
            id: contrastSlider

            value: mpv.contrast
            onSliderValueChanged: mpv.contrast = value.toFixed(0)

            Layout.topMargin: Kirigami.Units.largeSpacing
            Layout.columnSpan: 2
        }

        // ------------------------------------
        // BRIGHTNESS
        // ------------------------------------
        Label {
            text: qsTr("Brightness")
            Layout.alignment: Qt.AlignRight
        }

        ImageAdjustmentSlider {
            id: brightnessSlider

            value: mpv.brightness
            onSliderValueChanged: mpv.brightness = value.toFixed(0)

            Layout.topMargin: Kirigami.Units.largeSpacing
            Layout.columnSpan: 2
        }

        // ------------------------------------
        // GAMMA
        // ------------------------------------
        Label {
            text: qsTr("Gamma")
            Layout.alignment: Qt.AlignRight
        }

        ImageAdjustmentSlider {
            id: gammaSlider

            value: mpv.gamma
            onSliderValueChanged: mpv.gamma = value.toFixed(0)

            Layout.topMargin: Kirigami.Units.largeSpacing
            Layout.columnSpan: 2
        }

        // ------------------------------------
        // SATURATION
        // ------------------------------------
        Label {
            text: qsTr("Saturation")
            Layout.alignment: Qt.AlignRight
        }

        ImageAdjustmentSlider {
            id: saturationSlider

            value: mpv.saturation
            onSliderValueChanged: mpv.saturation = value.toFixed(0)

            Layout.topMargin: Kirigami.Units.largeSpacing
            Layout.columnSpan: 2
        }

        Label {
            text: qsTr("Middle click on the sliders to reset them")
            Layout.columnSpan: 3
            Layout.topMargin: Kirigami.Units.largeSpacing
        }

    }
}
