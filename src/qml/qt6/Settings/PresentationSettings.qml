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

    Platform.FileDialog {
        id: presentationToLoadOnStartupDialog

        fileMode: Platform.FileDialog.OpenFile
        folder: LocationSettings.cPlayFileLocation !== "" ? app.pathToUrl(LocationSettings.cPlayFileLocation) : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        nameFilters: ["C-Play presentation (*.cplaypres)"]
        title: "Choose presentation to load on startup"

        onAccepted: {
            var filePath = playerController.returnRelativeOrAbsolutePath(presentationToLoadOnStartupDialog.file.toString());
            PresentationSettings.presentationToLoadOnStartup = filePath;
            PresentationSettings.save();
            mpv.focus = true;
        }
        onRejected: mpv.focus = true
    }
    GridLayout {
        id: content

        columns: 3

        // ------------------------------------
        // PRESENTATION PARAMETERS
        // --

        SettingsHeader {
            Layout.columnSpan: 3
            Layout.fillWidth: true
            text: qsTr("Presentation (Slides and Layers) settings")
        }

        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Presentation to load on startup:")
        }
        RowLayout {
            Layout.fillWidth: true

            TextField {
                id: presentationToLoadOnStartupText

                placeholderText: "Path to presentation"
                text: PresentationSettings.presentationToLoadOnStartup

                onEditingFinished: {
                    PresentationSettings.presentationToLoadOnStartup = text;
                    PresentationSettings.save();
                }

                ToolTip {
                    text: qsTr("Path to presentation")
                }
            }
            ToolButton {
                id: presentationToLoadOnStartupButton

                focusPolicy: Qt.NoFocus
                icon.height: 16
                icon.name: "system-file-manager"
                text: ""

                onClicked: {
                    presentationToLoadOnStartupDialog.open();
                }
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Default stereoscopic mode for new layer:")
        }
        RowLayout {
            ComboBox {
                id: stereoscopicModeForNewLayerComboBoxBg

                enabled: true
                textRole: "mode"

                model: ListModel {
                    id: stereoscopicModeForNewLayer

                    ListElement {
                        mode: "2D (mono)"
                        value: 0
                    }
                    ListElement {
                        mode: "3D (side-by-side)"
                        value: 1
                    }
                    ListElement {
                        mode: "3D (top-bottom)"
                        value: 2
                    }
                    ListElement {
                        mode: "3D (top-bottom+flip)"
                        value: 3
                    }
                }

                Component.onCompleted: {
                    for (let i = 0; i < stereoscopicModeForNewLayer.count; ++i) {
                        if (stereoscopicModeForNewLayer.get(i).value === PresentationSettings.defaultStereoModeForLayers) {
                            currentIndex = i;
                            break;
                        }
                    }
                }
                onActivated: {
                    PresentationSettings.defaultStereoModeForLayers = model.get(index).value;
                    PresentationSettings.save();
                }
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Default grid mode for new layer:")
        }
        RowLayout {
            ComboBox {
                id: gridModeForNewLayerComboBox

                enabled: true
                textRole: "mode"

                model: ListModel {
                    id: gridModeForNewLayer

                    ListElement {
                        mode: "None/Pre-split"
                        value: 0
                    }
                    ListElement {
                        mode: "Plane"
                        value: 1
                    }
                    ListElement {
                        mode: "Dome"
                        value: 2
                    }
                    ListElement {
                        mode: "Sphere EQR"
                        value: 3
                    }
                    ListElement {
                        mode: "Sphere EAC"
                        value: 4
                    }
                }

                Component.onCompleted: {
                    for (let i = 0; i < gridModeForNewLayer.count; ++i) {
                        if (gridModeForNewLayer.get(i).value === PresentationSettings.defaultGridModeForLayers) {
                            currentIndex = i;
                            break;
                        }
                    }
                }
                onActivated: {
                    PresentationSettings.defaultGridModeForLayers = model.get(index).value;
                    PresentationSettings.save();
                }
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Default visibility for new layer:")
        }
        SpinBox {
            editable: true
            from: 0
            to: 100
            value: PresentationSettings.defaultLayerVisibility

            onValueChanged: {
                PresentationSettings.defaultLayerVisibility = value.toFixed(0);
                PresentationSettings.save();
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }
        
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("DPI (dots per inch) for rendering PDF pages:")
        }
        SpinBox {
            editable: true
            from: 0
            to: 1000
            value: PresentationSettings.pdfDpi

            onValueChanged: {
                PresentationSettings.pdfDpi = value.toFixed(0);
                PresentationSettings.save();
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Number of upcoming slides to preload:")
        }
        SpinBox {
            editable: true
            from: 0
            to: 10
            value: PresentationSettings.updateUpcomingSlideCount

            onValueChanged: {
                PresentationSettings.updateUpcomingSlideCount = value.toFixed(0);
                PresentationSettings.save();
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Slide fade duration (in msec) when moving to previous slide:")
        }
        SpinBox {
            editable: true
            from: 20
            to: 20000
            value: PresentationSettings.fadeDurationToPreviousSlide

            onValueChanged: {
                PresentationSettings.fadeDurationToPreviousSlide = value.toFixed(0);
                PresentationSettings.save();
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Slide fade duration (in msec) when moving to next slide:")
        }
        SpinBox {
            editable: true
            from: 20
            to: 20000
            value: PresentationSettings.fadeDurationToNextSlide

            onValueChanged: {
                PresentationSettings.fadeDurationToNextSlide = value.toFixed(0);
                PresentationSettings.save();
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Delay (in msec) between clearing and loading new presentation:")
        }
        SpinBox {
            editable: true
            from: 0
            to: 20000
            value: PresentationSettings.clearAndLoadDelay

            onValueChanged: {
                PresentationSettings.clearAndLoadDelay = value.toFixed(0);
                PresentationSettings.save();
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Delay (in msec) between presentation load and syncing:")
        }
        SpinBox {
            editable: true
            from: 0
            to: 20000
            value: PresentationSettings.syncAfterLoadDelay

            onValueChanged: {
                PresentationSettings.syncAfterLoadDelay = value.toFixed(0);
                PresentationSettings.save();
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Delay (in msec) between presentation load and start:")
        }
        SpinBox {
            editable: true
            from: 0
            to: 20000
            value: PresentationSettings.startAfterLoadDelay

            onValueChanged: {
                PresentationSettings.startAfterLoadDelay = value.toFixed(0);
                PresentationSettings.save();
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }
    }
}
