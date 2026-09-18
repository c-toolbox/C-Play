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
import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

Kirigami.ApplicationWindow {
    id: root

    color: Kirigami.Theme.alternateBackgroundColor
    height: 430
    title: qsTr("Add new layer")
    visible: false
    width: 400

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
            layerCoreProps.resetValues();
        }
    }

    GridLayout {
        anchors.fill: parent
        anchors.margins: 15
        columnSpacing: 2
        columns: 2
        rowSpacing: 8

        RowLayout {
            Layout.bottomMargin: 5
            Layout.columnSpan: 2

            Rectangle {
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
                width: Kirigami.Units.gridUnit
            }
            Label {
                text: qsTr("Properties for the new layer")
            }
            Rectangle {
                Layout.fillWidth: true
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
            }
        }

        LayerCoreProperties {
            id: layerCoreProps
            Layout.columnSpan: 2
        }
        
        RowLayout {
            Layout.bottomMargin: 5
            Layout.columnSpan: 2

            Button {
                Layout.fillWidth: true
                icon.name: "layer-new"
                text: qsTr("Add new layer")

                onClicked: {
                    if (layerCoreProps.layerTitle.text !== "") {
                        if (layerCoreProps.typeComboBox.currentText === "NDI") {
                            layerView.layerItem.layerIdx = app.slides.selected.addLayer(layerCoreProps.layerTitle.text, layerCoreProps.typeComboBox.currentIndex + 1, layerCoreProps.ndiSenderComboBox.currentText, layerCoreProps.stereoscopicModeForLayer.currentIndex, layerCoreProps.gridModeForLayer.currentIndex);
                            layersAddNew.visible = false;
                            app.slides.updateSelectedSlide();
                            mpv.focus = true;
                        } else if (layerCoreProps.typeComboBox.currentText === "Spout") {
                            layerView.layerItem.layerIdx = app.slides.selected.addLayer(layerCoreProps.layerTitle.text, layerCoreProps.typeComboBox.currentIndex + 1, layerCoreProps.spoutSenderComboBox.currentText, layerCoreProps.stereoscopicModeForLayer.currentIndex, layerCoreProps.gridModeForLayer.currentIndex);
                            layersAddNew.visible = false;
                            app.slides.updateSelectedSlide();
                            mpv.focus = true;
                        } else if (layerCoreProps.typeComboBox.currentText === "OMT") {
                            layerView.layerItem.layerIdx = app.slides.selected.addLayer(layerCoreProps.layerTitle.text, layerCoreProps.typeComboBox.currentIndex + 1, layerCoreProps.omtSenderComboBox.currentText, layerCoreProps.stereoscopicModeForLayer.currentIndex, layerCoreProps.gridModeForLayer.currentIndex);
                            layersAddNew.visible = false;
                            app.slides.updateSelectedSlide();
                            mpv.focus = true;
                        } else if (layerCoreProps.typeComboBox.currentText === "DirectShow") {
                            // A chosen video device means capture mode - the parameter string carries
                            // "videoDevice|audioDevice" (the audio part may be empty). Without a video
                            // device the layer plays back the media file instead.
                            var directShowParam = "";
                            var directShowPresetKey = "";
                            if (!layerCoreProps.directShowPresetsLayout.customEntry && layerCoreProps.directShowPresetAvailable) {
                                // A predefined setup from data/predefined-directshows.json was chosen - it carries its own
                                // video/audio device combination (either part may be empty for audio-only/video-only setups,
                                // and both may be empty when the entry only defines per-machine "devices" overrides).
                                var preset = layerCoreProps.getDirectShowPresetDevices();
                                if (preset) {
                                    directShowParam = preset.video + "|" + preset.audio;
                                    // Remember which predefined entry this layer was created from (the entry title), so each machine in the cluster can resolve its own local capture devices from data/predefined-directshows.json. Custom selections keep an empty key and use the chosen devices verbatim.
                                    directShowPresetKey = layerCoreProps.directShowPresetsComboBox.currentText;
                                }
                            } else if (layerCoreProps.directShowVideoDeviceComboBox.currentText !== "") {
                                directShowParam = layerCoreProps.directShowVideoDeviceComboBox.currentText + "|" + layerCoreProps.directShowAudioDeviceComboBox.currentText;
                            } else if (layerCoreProps.fileForLayer.text !== "") {
                                directShowParam = layerCoreProps.fileForLayer.text;
                            }
                            if (directShowParam !== "" || directShowPresetKey !== "") {
                                layerView.layerItem.layerIdx = app.slides.selected.addLayer(layerCoreProps.layerTitle.text, layerCoreProps.typeComboBox.currentIndex + 1, directShowParam, layerCoreProps.stereoscopicModeForLayer.currentIndex, layerCoreProps.gridModeForLayer.currentIndex);
                                if (directShowPresetKey !== "") {
                                    layerView.layerItem.layerDirectShowPresetKey = directShowPresetKey;
                                }
                                layersAddNew.visible = false;
                                app.slides.updateSelectedSlide();
                                mpv.focus = true;
                            }
                        } else if (layerCoreProps.typeComboBox.currentText === "Stream") {
                            if(layerCoreProps.streamsLayout.customEntry){
                                layerView.layerItem.layerIdx = app.slides.selected.addLayer(layerCoreProps.layerTitle.text, layerCoreProps.typeComboBox.currentIndex + 1, layerCoreProps.streamCustomEntryField.text, layerCoreProps.stereoscopicModeForLayer.currentIndex, layerCoreProps.gridModeForLayer.currentIndex);
                            }
                            else {
                                layerView.layerItem.layerIdx = app.slides.selected.addLayer(layerCoreProps.layerTitle.text, layerCoreProps.typeComboBox.currentIndex + 1, layerCoreProps.streamsComboBox.currentValue, layerCoreProps.stereoscopicModeForLayer.currentIndex, layerCoreProps.gridModeForLayer.currentIndex);
                                // Remember which predefined entry this layer was created from (the entry title), so each machine in the cluster can resolve its own local path from data/predefined-streams.json. Custom paths keep an empty key and use the file path verbatim.
                                layerView.layerItem.layerStreamKey = layerCoreProps.streamsComboBox.currentText;
                            }
                            layersAddNew.visible = false;
                            app.slides.updateSelectedSlide();
                            mpv.focus = true;
                        } else if (layerCoreProps.typeComboBox.currentText === "Text") {
                            layerView.layerItem.layerIdx = app.slides.selected.addLayer(layerCoreProps.layerTitle.text, layerCoreProps.typeComboBox.currentIndex + 1, layerCoreProps.textForLayer.text, layerCoreProps.stereoscopicModeForLayer.currentIndex, layerCoreProps.gridModeForLayer.currentIndex);
                            layersAddNew.visible = false;
                            app.slides.updateSelectedSlide();
                            mpv.focus = true;
                        } else if (layerCoreProps.typeComboBox.currentText === "MultiVideo") {
                            if (layerCoreProps.fileForLayer.text !== "") {
                                layerView.layerItem.layerIdx = app.slides.selected.addLayer(layerCoreProps.layerTitle.text, layerCoreProps.typeComboBox.currentIndex + 1, layerCoreProps.fileForLayer.text, 0, 0);
                                layersAddNew.visible = false;
                                app.slides.updateSelectedSlide();
                                mpv.focus = true;
                            }
                        } else if (layerCoreProps.typeComboBox.currentText === "Control") {
                            var controlPath = layerCoreProps.controlOperationComboBox.currentText + ":" + layerCoreProps.controlParameterField.text;
                            layerView.layerItem.layerIdx = app.slides.selected.addLayer(layerCoreProps.layerTitle.text, layerCoreProps.typeComboBox.currentIndex + 1, controlPath, 0, 0);
                            layersAddNew.visible = false;
                            app.slides.updateSelectedSlide();
                            mpv.focus = true;
                        } else if (layerCoreProps.typeComboBox.currentText === "REST") {
                            var restUrl;
                            if(layerCoreProps.restCommandsLayout.customEntry) {
                                restUrl = layerCoreProps.restCustomUrlField.text;
                            } else {
                                restUrl = layerCoreProps.restCommandsComboBox.currentValue;
                            }
                            layerView.layerItem.layerIdx = app.slides.selected.addLayer(layerCoreProps.layerTitle.text, layerCoreProps.typeComboBox.currentIndex + 1, restUrl, 0, 0);
                            // Set method, parameters, ignoreStatus after creation
                            layerView.layerItem.layerRestMethod = layerCoreProps.restMethodComboBox.currentIndex;
                            layerView.layerItem.layerRestParameters = layerCoreProps.getRestParametersJson();
                            layerView.layerItem.layerRestIgnoreStatus = layerCoreProps.restIgnoreStatusCheckBox.checked;
                            layersAddNew.visible = false;
                            app.slides.updateSelectedSlide();
                            mpv.focus = true;
                        } else if (layerCoreProps.typeComboBox.currentText === "WebRTC") {
                            if (layerCoreProps.whepUrlField.text.trim() !== "") {
                                layerView.layerItem.layerIdx = app.slides.selected.addLayer(layerCoreProps.layerTitle.text, layerCoreProps.typeComboBox.currentIndex + 1, layerCoreProps.whepUrlField.text.trim(), layerCoreProps.stereoscopicModeForLayer.currentIndex, layerCoreProps.gridModeForLayer.currentIndex);
                                layersAddNew.visible = false;
                                app.slides.updateSelectedSlide();
                                mpv.focus = true;
                            }
                        } else if (layerCoreProps.fileForLayer.text !== "") {
                            layerView.layerItem.layerIdx = app.slides.selected.addLayer(layerCoreProps.layerTitle.text, layerCoreProps.typeComboBox.currentIndex + 1, layerCoreProps.fileForLayer.text, layerCoreProps.stereoscopicModeForLayer.currentIndex, layerCoreProps.gridModeForLayer.currentIndex);
                            // If an image sequence was detected and user did not opt to load only this image
                            if (layerCoreProps.typeComboBox.currentText === "Image" && layerCoreProps.imageSequenceDetected && !layerCoreProps.imageSequenceLoadOnlyThis) {
                                var scanResult = playerController.scanImageSequence(layerCoreProps.fileForLayer.text);
                                if (scanResult.ok) {
                                    var dir = layerCoreProps.fileForLayer.text;
                                    var lastSep = dir.lastIndexOf("/");
                                    if (lastSep < 0) lastSep = dir.lastIndexOf("\\");
                                    if (lastSep >= 0) dir = dir.substring(0, lastSep);
                                    layerView.layerItem.setLayerImageSequence(
                                        dir,
                                        scanResult.prefix,
                                        scanResult.digitCount,
                                        scanResult.suffix,
                                        layerCoreProps.imageSequenceStartIndex,
                                        layerCoreProps.imageSequenceStopIndex,
                                        layerCoreProps.imageSequenceStep,
                                        33,
                                        true
                                    );
                                }
                            }
                            layersAddNew.visible = false;
                            app.slides.updateSelectedSlide();
                            mpv.focus = true;
                        }
                    }
                }

                ToolTip {
                    text: qsTr("Add layer to bottom of list")
                }
            }
            Item {
                // spacer item
                Layout.fillWidth: true
            }
        }
    }
}
