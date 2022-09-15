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

        CheckBox {
            text: qsTr("Use 3D Mode on startup")
            enabled: enabled
            checked: PlaybackSettings.stereoModeOnStartup
            onCheckedChanged: {
                PlaybackSettings.stereoModeOnStartup = checked
                PlaybackSettings.save()
            }
            Layout.columnSpan: 3
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
                value: VideoSettings.surfacegAngle_2ndState
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
                value: mpv.translateX
                onValueChanged: mpv.translateX = value
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
                value: mpv.translateY
                onValueChanged: mpv.translateY = value
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
                value: mpv.translateZ
                onValueChanged: mpv.translateZ = value
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
            Layout.alignment: Qt.AlignRight
        }

        RowLayout {
            Slider {
                id: rotateXSlider
                value: mpv.rotateX * 100
                from: -18000
                to: 18000
                enabled: mpv.gridToMapOn !== 2
                onMoved: {
                    mpv.rotateX = value.toFixed(0)
                    rotateXSpinBox.value = value.toFixed(0)
                }
                Layout.topMargin: Kirigami.Units.largeSpacing
            }
            SpinBox {
                id: rotateXSpinBox
                from: -18000
                value: mpv.rotateX * 100
                to: 18000
                enabled: mpv.gridToMapOn !== 2
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
                    mpv.rotateX = realValue
                    rotateXSlider.value = value
                }

                Connections {
                    target: mpv
                    onRotateXChanged: {
                        if(rotateXSpinBox.realValue !== mpv.rotateX)
                            rotateXSpinBox.value = mpv.rotateX * 100
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
            Layout.alignment: Qt.AlignRight
        }

        // ------------------------------------
        // RotateY
        // ------------------------------------
        Label {
            text: qsTr("Rotate around Y: ")
            Layout.alignment: Qt.AlignRight
        }

        RowLayout {
            Slider {
                id: rotateYSlider
                value: mpv.rotateY * 100
                from: -18000
                to: 18000
                enabled: mpv.gridToMapOn !== 2
                onMoved: {
                    mpv.rotateY = value.toFixed(0)
                    rotateYSpinBox.value = value.toFixed(0)
                }
                Layout.topMargin: Kirigami.Units.largeSpacing
            }
            SpinBox {
                id: rotateYSpinBox
                from: -18000
                value: mpv.rotateY * 100
                to: 18000
                enabled: mpv.gridToMapOn !== 2
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
                    mpv.rotateY = realValue
                    rotateYSlider.value = value
                }

                Connections {
                    target: mpv
                    onRotateYChanged: {
                        if(rotateYSpinBox.realValue !== mpv.rotateY)
                            rotateYSpinBox.value = mpv.rotateY * 100
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
            Layout.alignment: Qt.AlignRight
        }

        // ------------------------------------
        // RotateZ
        // ------------------------------------
        Label {
            text: qsTr("Rotate around Z: ")
            Layout.alignment: Qt.AlignRight
        }

        RowLayout {
            Slider {
                id: rotateZSlider
                value: mpv.rotateZ * 100
                from: -18000
                to: 18000
                enabled: mpv.gridToMapOn !== 2
                onMoved: {
                    mpv.rotateZ = value.toFixed(0)
                    rotateZSpinBox.value = value.toFixed(0)
                }
                Layout.topMargin: Kirigami.Units.largeSpacing
            }
            SpinBox {
                id: rotateZSpinBox
                from: -18000
                value: mpv.rotateZ * 100
                to: 18000
                enabled: mpv.gridToMapOn !== 2
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
                    mpv.rotateZ = realValue
                    rotateZSlider.value = value
                }

                Connections {
                    target: mpv
                    onRotateZChanged: {
                        if(rotateZSpinBox.realValue !== mpv.rotateZ)
                            rotateZSpinBox.value = mpv.rotateZ * 100
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
            Layout.alignment: Qt.AlignRight
        }

        // ------------------------------------
        // Reset
        // ------------------------------------

        RowLayout {
            Button {
                    text: "Reset radius/fov/rotation settings to startup values"
                    onClicked: {
                        mpv.radius = VideoSettings.surfaceRadius
                        mpv.fov = VideoSettings.surfaceFov
                        mpv.angle = VideoSettings.surfaceAngle
                        mpv.rotateX = VideoSettings.surfaceRotateX
                        mpv.rotateY = VideoSettings.surfaceRotateY
                        mpv.rotateZ = VideoSettings.surfaceRotateZ
                        mpv.translateX = VideoSettings.surfaceTranslateX
                        mpv.translateY = VideoSettings.surfaceTranslateY
                        mpv.translateZ = VideoSettings.surfaceTranslateZ
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
                        VideoSettings.surfaceRadius = mpv.radius
                        VideoSettings.surfaceFov = mpv.fov
                        VideoSettings.surfaceAngle = mpv.angle
                        VideoSettings.surfaceRotateX = mpv.rotateX
                        VideoSettings.surfaceRotateY = mpv.rotateY
                        VideoSettings.surfaceRotateZ = mpv.rotateZ
                        VideoSettings.surfaceTranslateX = mpv.translateX
                        VideoSettings.surfaceTranslateY = mpv.translateY
                        VideoSettings.surfaceTranslateZ = mpv.translateZ
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
                            /*NumberAnimation
                            {
                                id: rotateXAnimation
                                target: rotateXSpinBox
                                property: "value"
                                to: 50;
                                duration: 1000
                            }
                            NumberAnimation
                            {
                                id: rotateYAnimation
                                target: rotateYSpinBox
                                property: "value"
                                to: 50;
                                duration: 1000
                            }
                            NumberAnimation
                            {
                                id: rotateZAnimation
                                target: rotateZSpinBox
                                property: "value"
                                to: 50;
                                duration: 1000
                            }*/
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

                        /*rotateXAnimation.to = rotateXSpinBoxScenario.value
                        rotateXAnimation.duration = transitionTime.value * 1000
                        rotateXSpinBoxScenario.value = rotateXSpinBox.value

                        rotateYAnimation.to = rotateYSpinBoxScenario.value
                        rotateYAnimation.duration = transitionTime.value * 1000
                        rotateYSpinBoxScenario.value = rotateYSpinBox.value

                        rotateZAnimation.to = rotateZSpinBoxScenario.value
                        rotateZAnimation.duration = transitionTime.value * 1000
                        rotateZSpinBoxScenario.value = rotateZSpinBox.value*/

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
