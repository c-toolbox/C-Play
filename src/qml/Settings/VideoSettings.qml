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
            text: qsTr("Transistion Scenario:")
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
                id: domeRadius
                from: 0
                to: 2000
                stepSize: 1
                value: mpv.radius

                onValueChanged: {
                    mpv.radius = value
                }
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
                id: domeRadiusScenario
                from: 0
                to: 2000
                stepSize: 1
                value: 400
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
                id: domeFov
                from: 0
                to: 360
                value: mpv.fov

                onValueChanged: {
                    mpv.fov = value
                }
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
                id: domeFovScenario
                from: 0
                to: 360
                value: mpv.fov
            }
            Layout.alignment: Qt.AlignRight
        }

        // ------------------------------------
        // Dome translate Y
        // ------------------------------------
        Label {
            text: qsTr("Translate up-down:")
        }

        RowLayout {
            SpinBox {
                id: domeTranslate
                from: -5000
                to: 5000
                value: mpv.translateY

                onValueChanged: {
                    mpv.translateY = value
                }
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
                id: domeTranslateScenario
                from: -5000
                to: 5000
                value: 750
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
                onValueChanged:
                    if(rotateXSpinBox.value !== value.toFixed(0)){
                        rotateXSpinBox.value = value.toFixed(0)
                    }
                Layout.topMargin: Kirigami.Units.largeSpacing
            }
            SpinBox {
                id: rotateXSpinBox
                from: -18000
                value: mpv.rotateX * 100
                to: 18000
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

                onValueChanged: {
                    if(mpv.rotateX !== realValue){
                        mpv.rotateX = realValue
                        rotateXSlider.value = value
                    }
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
            }
            SpinBox {
                id: rotateXSpinBoxScenario
                from: -180
                to: 180
                value: -90
            }
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

                value: mpv.rotateY
                from: -180
                to: 180
                onValueChanged:
                    if(mpv.rotateY !== value.toFixed(0)){
                        mpv.rotateY = value.toFixed(0)
                        rotateYSpinBox.value = value.toFixed(0)
                    }
                Layout.topMargin: Kirigami.Units.largeSpacing
            }
            SpinBox {
                id: rotateYSpinBox
                from: -180
                to: 180
                value: mpv.rotateY

                onValueChanged: {
                    if(mpv.rotateY !== value){
                        mpv.rotateY = value
                        rotateYSlider.value = value
                    }
                }
            }
            LabelWithTooltip {
                text: {
                    qsTr(" degrees")
                }
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }
        RowLayout {
            SpinBox {
                id: rotateYSpinBoxScenario
                from: -180
                to: 180
                value: 90
            }
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

                value: mpv.rotateZ
                from: -180
                to: 180
                onValueChanged:
                    if(mpv.rotateZ !== value.toFixed(0)){
                        mpv.rotateZ = value.toFixed(0)
                        rotateZSpinBox.value = value.toFixed(0)
                    }
                Layout.topMargin: Kirigami.Units.largeSpacing
            }
            SpinBox {
                id: rotateZSpinBox
                from: -180
                to: 180
                value: mpv.rotateZ

                onValueChanged: {
                    if(mpv.rotateZ !== value){
                        mpv.rotateZ = value
                        rotateZSlider.value = value
                    }
                }
            }
            LabelWithTooltip {
                text: {
                    qsTr(" degrees")
                }
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }
        RowLayout {
            SpinBox {
                id: rotateZSpinBoxScenario
                from: -180
                to: 180
                value: 0
            }
            Layout.alignment: Qt.AlignRight
        }

        RowLayout {
            Button {
                    text: "Reset radius/fov/rotation settings to startup values"
                    onClicked: {
                        mpv.radius = VideoSettings.domeRadius
                        mpv.fov = VideoSettings.domeFov
                        mpv.rotateX = VideoSettings.domeRotateX
                        mpv.rotateY = VideoSettings.domeRotateY
                        mpv.rotateZ = VideoSettings.domeRotateZ
                        mpv.translateY = VideoSettings.domeTranslateY
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

                onValueChanged: {
                }
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
                        VideoSettings.domeRadius = mpv.radius
                        VideoSettings.domeFov = mpv.fov
                        VideoSettings.domeRotateX = mpv.rotateX
                        VideoSettings.domeRotateY = mpv.rotateY
                        VideoSettings.domeRotateZ = mpv.rotateZ
                        VideoSettings.domeTranslateY = mpv.translateY
                        VideoSettings.save()
                    }
            }
            Layout.columnSpan: 2
            Layout.fillWidth: true
        }

        RowLayout {
            Button {
                    text: "Start transition"

                    ParallelAnimation {
                            id: gridAnimations
                            NumberAnimation
                            {
                                id: domeRadiusAnimation
                                target: domeRadius
                                property: "value"
                                to: 50;
                                duration: 1000
                            }
                            NumberAnimation
                            {
                                id: domeFovAnimation
                                target: domeFov
                                property: "value"
                                to: 50;
                                duration: 1000
                            }
                            NumberAnimation
                            {
                                id: domeTranslateAnimation
                                target: domeTranslate
                                property: "value"
                                to: 50;
                                duration: 1000
                            }
                            NumberAnimation
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
                            }
                    }

                    onClicked: {
                        domeRadiusAnimation.to = domeRadiusScenario.value
                        domeRadiusAnimation.duration = transitionTime.value * 1000
                        domeRadiusScenario.value = domeRadius.value

                        domeFovAnimation.to = domeFovScenario.value
                        domeFovAnimation.duration = transitionTime.value * 1000
                        domeFovScenario.value = domeFov.value

                        domeTranslateAnimation.to = domeTranslateScenario.value
                        domeTranslateAnimation.duration = transitionTime.value * 1000
                        domeTranslateScenario.value = domeTranslate.value

                        rotateXAnimation.to = rotateXSpinBoxScenario.value
                        rotateXAnimation.duration = transitionTime.value * 1000
                        rotateXSpinBoxScenario.value = rotateXSpinBox.value

                        rotateYAnimation.to = rotateYSpinBoxScenario.value
                        rotateYAnimation.duration = transitionTime.value * 1000
                        rotateYSpinBoxScenario.value = rotateYSpinBox.value

                        rotateZAnimation.to = rotateZSpinBoxScenario.value
                        rotateZAnimation.duration = transitionTime.value * 1000
                        rotateZSpinBoxScenario.value = rotateZSpinBox.value

                        gridAnimations.start()
                    }
            }
            Layout.alignment: Qt.AlignRight
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
