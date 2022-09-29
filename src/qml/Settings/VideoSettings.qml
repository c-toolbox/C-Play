/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.12
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.12

import org.kde.kirigami 2.11 as Kirigami
import com.georgefb.haruna 1.0
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

        Label {
            text: qsTr("Stereoscopic mode on startup:")
        }
        ComboBox {
            id: stereoscopicModeOnStartupComboBox
            enabled: true
            textRole: "mode"
            Layout.columnSpan: 2
            model: ListModel {
                id: stereoscopicModeOnStartupMode
                ListElement { mode: "2D (mono)"; value: 0 }
                ListElement { mode: "3D (side-by-side)"; value: 1}
                ListElement { mode: "3D (top-bottom)"; value: 2 }
                ListElement { mode: "3D (top-bottom+flip)"; value: 3 }
            }

            onActivated: {
                PlaybackSettings.stereoModeOnStartup = model.get(index).value
                PlaybackSettings.save()
            }

            Component.onCompleted: {
                for (let i = 0; i < stereoscopicModeOnStartupMode.count; ++i) {
                    if (stereoscopicModeOnStartupMode.get(i).value === PlaybackSettings.stereoModeOnStartup) {
                        currentIndex = i
                        break
                    }
                }
            }
        }

        RowLayout {
            Label {
                text: qsTr("Rotation speed:")
            }
            SpinBox {
                id: rotationSpeed
                from: 0
                value: mpv.rotationSpeed * 1000
                to: 10000
                enabled: mpv.gridToMapOn >= 2
                stepSize: 1

                property int decimals: 3
                property real realValue: value / 1000

                validator: DoubleValidator {
                    bottom: Math.min(rotationSpeed.from, rotationSpeed.to)
                    top:  Math.max(rotationSpeed.from, rotationSpeed.to)
                }

                textFromValue: function(value, locale) {
                    return Number(value / 1000).toLocaleString(locale, 'f', rotationSpeed.decimals)
                }

                valueFromText: function(text, locale) {
                    return Number.fromLocaleString(locale, text) * 1000
                }

                onValueModified: {
                    mpv.rotationSpeed = realValue
                }
            }
            Layout.alignment: Qt.AlignRight
        }

        Label {
            text: qsTr("Load grid on startup:")
        }
        ComboBox {
            id: loadGridOnStartupComboBox
            enabled: true
            textRole: "mode"
            Layout.columnSpan: 2
            model: ListModel {
                id: loadGridOnStartupMode
                ListElement { mode: "Flat/Pre-split"; value: 0 }
                ListElement { mode: "Dome"; value: 1}
                ListElement { mode: "Sphere"; value: 2 }
            }

            onActivated: {
                PlaybackSettings.gridToMapOn = model.get(index).value
                PlaybackSettings.save()
            }

            Component.onCompleted: {
                for (let i = 0; i < loadGridOnStartupMode.count; ++i) {
                    if (loadGridOnStartupMode.get(i).value === PlaybackSettings.gridToMapOn) {
                        currentIndex = i
                        break
                    }
                }
            }
        }

        Label {
            text: qsTr("Alternative Transistion Scenario:")
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
                enabled: mpv.gridToMapOn < 2
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
                enabled: mpv.gridToMapOn < 2
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
                    onRotateChanged: {
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
                enabled: mpv.gridToMapOn < 2
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
                enabled: mpv.gridToMapOn < 2
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
                    onRotateChanged: {
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
                enabled: mpv.gridToMapOn < 2
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
                enabled: mpv.gridToMapOn < 2
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
                    onRotateChanged: {
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
                        mpv.rotationSpeed = VideoSettings.surfaceRotationSpeed
                        mpv.radius = VideoSettings.surfaceRadius
                        mpv.fov = VideoSettings.surfaceFov
                        mpv.angle = VideoSettings.surfaceAngle
                        mpv.rotate = Qt.vector3d(VideoSettings.surfaceRotateX, VideoSettings.surfaceRotateY, VideoSettings.surfaceRotateZ)
                        mpv.translate = Qt.vector3d(VideoSettings.surfaceTranslateX, VideoSettings.surfaceTranslateY, VideoSettings.surfaceTranslateZ)
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
                value: 10
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
                    enabled: !mpv.surfaceTransistionOnGoing
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
                            onFinished: mpv.surfaceTransistionOnGoing = false
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

                        mpv.surfaceTransistionOnGoing = true
                    }
            }
            Layout.alignment: Qt.AlignRight

            Connections {
                target: mpv
                onSurfaceTransistionPerformed: startTransitionButton.clicked()
            }
        }

        // ------------------------------------
        // Screenshot Format
        // ------------------------------------
        SettingsHeader {
            text: qsTr("Screenshots")
            topMargin: 0
            Layout.columnSpan: 3
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("Format")
            Layout.alignment: Qt.AlignRight
        }

        Item {
            height: screenshotFormat.height
            ComboBox {
                id: screenshotFormat
                textRole: "key"
                model: ListModel {
                    ListElement { key: "PNG"; }
                    ListElement { key: "JPG"; }
                    ListElement { key: "WebP"; }
                }

                onActivated: {
                    VideoSettings.screenshotFormat = model.get(index).key
                    VideoSettings.save()
                    mpv.setProperty("screenshot-format", VideoSettings.screenshotFormat)
                }

                Component.onCompleted: {
                    if (VideoSettings.screenshotFormat === "PNG") {
                        currentIndex = 0
                    }
                    if (VideoSettings.screenshotFormat === "JPG") {
                        currentIndex = 1
                    }
                    if (VideoSettings.screenshotFormat === "WebP") {
                        currentIndex = 2
                    }
                }
            }
            Layout.fillWidth: true
            Layout.columnSpan: 2
        }

        // ------------------------------------
        // Screenshot template
        // ------------------------------------
        Label {
            text: qsTr("Template")
            Layout.alignment: Qt.AlignRight
        }

        Item {
            height: screenshotTemplate.height
            TextField {
                id: screenshotTemplate
                text: VideoSettings.screenshotTemplate
                onEditingFinished: {
                    VideoSettings.screenshotTemplate = text
                    VideoSettings.save()
                    mpv.setProperty("screenshot-template", VideoSettings.screenshotTemplate)
                }
                Layout.fillWidth: true
                Layout.columnSpan: 2
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
