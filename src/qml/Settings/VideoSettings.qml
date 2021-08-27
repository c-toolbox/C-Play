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

        columns: 2

        SettingsHeader {
            text: qsTr("Screenshots")
            topMargin: 0
            Layout.columnSpan: 2
            Layout.fillWidth: true
        }

        // ------------------------------------
        // Screenshot Format
        // ------------------------------------
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
            }
        }

        SettingsHeader {
            text: qsTr("Image adjustments")
            Layout.columnSpan: 2
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
        }

        Label {
            text: qsTr("Middle click on the sliders to reset them")
            Layout.columnSpan: 2
            Layout.topMargin: Kirigami.Units.largeSpacing
        }


        // ------------------------------------
        // GRID PARAMETERS
        // --
        SettingsHeader {
            text: qsTr("Mapping/grid parameters")
            Layout.columnSpan: 2
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
            Layout.columnSpan: 2
        }

        Label {
            text: qsTr("Load grid on startup:")
        }
        ComboBox {
            id: loadGridOnStartupComboBox
            enabled: true
            textRole: "mode"
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

        // ------------------------------------
        // Dome fov
        // ------------------------------------
        Label {
            text: qsTr("Dome field of view (fov):")
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

        // ------------------------------------
        // RotateX
        // ------------------------------------
        Label {
            text: qsTr("Dome/Sphere rotate around X")
            Layout.alignment: Qt.AlignRight
        }

        RowLayout {
            Slider {
                id: rotateXSlider

                value: mpv.rotateX
                from: 0
                to: 360
                onValueChanged: mpv.rotateX = value.toFixed(0)

                Layout.topMargin: Kirigami.Units.largeSpacing
            }
            LabelWithTooltip {
                text: {
                    qsTr("%1 degrees").arg(Number(mpv.rotateX))
                }
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        // ------------------------------------
        // RotateY
        // ------------------------------------
        Label {
            text: qsTr("Dome/Sphere rotate around Y")
            Layout.alignment: Qt.AlignRight
        }

        RowLayout {
            Slider {
                id: rotateYSlider

                value: mpv.rotateY
                from: 0
                to: 360
                onValueChanged: mpv.rotateY = value.toFixed(0)

                Layout.topMargin: Kirigami.Units.largeSpacing
            }
            LabelWithTooltip {
                text: {
                    qsTr("%1 degrees").arg(Number(mpv.rotateY))
                }
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        // ------------------------------------
        // RotateZ
        // ------------------------------------
        Label {
            text: qsTr("Dome/Sphere rotate around Z")
            Layout.alignment: Qt.AlignRight
        }

        RowLayout {
            Slider {
                id: rotateZSlider

                value: mpv.rotateZ
                from: 0
                to: 360
                onValueChanged: mpv.rotateZ = value.toFixed(0)

                Layout.topMargin: Kirigami.Units.largeSpacing
            }
            LabelWithTooltip {
                text: {
                    qsTr("%1 degrees").arg(Number(mpv.rotateZ))
                }
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
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
                    }
            }
            Layout.columnSpan: 2
            Layout.fillWidth: true
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
                        VideoSettings.save()
                    }
            }
            Layout.columnSpan: 2
            Layout.fillWidth: true
        }

    }
}
