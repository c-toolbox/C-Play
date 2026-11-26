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
    // Grows only when the content (e.g. with the missing-properties warning) needs more room than the default.
    height: Math.max(612, mainLayout.implicitHeight + 2 * mainLayout.anchors.margins)
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
    // Set by a click on "Add new layer"; from then on the warning follows the fields as they are filled in.
    property bool showMissingProperties: false

    // Names of the properties that must be filled in before the layer can be added (empty when complete).
    function missingProperties() {
        var p = layerCoreProps;
        var type = p.typeComboBox.currentText;
        var missing = [];
        // A TCP layer without a title is named after its command.
        if (p.layerTitle.text === "" && type !== "TCP")
            missing.push(qsTr("Title"));
        switch (type) {
        case "NDI":
        case "Spout":
        case "OMT":
        case "Stream":
        case "Text":
        case "Control":
        case "REST":
            break;
        case "DirectShow":
            if (!p.captureNodeTargeted && !p.directShowPresetsLayout.customEntry && p.directShowPresetAvailable) {
                if (!p.getDirectShowPresetDevices())
                    missing.push(qsTr("Setup"));
            } else if (p.directShowVideoDeviceComboBox.currentText === "" && p.fileForLayer.text === "") {
                missing.push(qsTr("Video device or File"));
            }
            break;
        case "Capture":
            if (p.getCaptureSourceString() === "" && !p.captureUsesPreset())
                missing.push(qsTr("Input"));
            break;
        case "YouTube":
            if (p.youtubeUrlField.text.trim() === "")
                missing.push(qsTr("URL"));
            break;
        case "TCP":
            if (p.tcpCommandsComboBox.currentIndex < 0)
                missing.push(qsTr("TCP command"));
            break;
        case "WebRTC":
            if (p.whepUrlField.text.trim() === "")
                missing.push(qsTr("WHEP URL"));
            break;
        default:
            if (p.fileForLayer.text === "")
                missing.push(qsTr("File"));
            break;
        }
        return missing;
    }

    onVisibilityChanged: {
        // Cleared on close too, so the warning (and the extra height) is gone before the next open.
        showMissingProperties = false;
        if (visible) {
            layerCoreProps.resetValues();
        } else {
            // Cancel any in-flight YouTube metadata fetch when the dialog closes.
            if (app.ytdlpMetadataModel) {
                app.ytdlpMetadataModel.clear();
            }
        }
    }

    GridLayout {
        id: mainLayout
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

        Kirigami.InlineMessage {
            id: missingPropertiesMessage
            Layout.columnSpan: 2
            Layout.fillWidth: true
            type: Kirigami.MessageType.Warning
            visible: root.showMissingProperties && root.missingProperties().length > 0
            text: qsTr("Missing: %1").arg(root.missingProperties().join(", "))
        }

        RowLayout {
            Layout.bottomMargin: 5
            Layout.columnSpan: 2

            Button {
                Layout.fillWidth: true
                icon.name: "layer-new"
                text: qsTr("Add new layer")

                onClicked: {
                    root.showMissingProperties = true;
                    if (root.missingProperties().length === 0) {
                        if (layerCoreProps.typeComboBox.currentText === "NDI") {
                            layerView.layerItem.layerIdx = app.slides.selected.addLayer(layerCoreProps.layerTitle.text, layerCoreProps.typeComboBox.currentIndex + 1, layerCoreProps.ndiSenderComboBox.currentText, layerCoreProps.stereoscopicModeForLayer.currentIndex, layerCoreProps.gridModeForLayer.currentIndex);
                            layersAddNew.visible = false;
                            app.slides.updateSelectedSlide();
                            mpv.focus = true;
                        } else if (layerCoreProps.typeComboBox.currentText === "Spout") {
                            layerView.layerItem.layerIdx = app.slides.selected.addLayer(layerCoreProps.layerTitle.text, layerCoreProps.typeComboBox.currentIndex + 1, layerCoreProps.spoutSenderComboBox.currentText, layerCoreProps.stereoscopicModeForLayer.currentIndex, layerCoreProps.gridModeForLayer.currentIndex);
                            layerView.layerItem.layerCaptureTargetNode = layerCoreProps.captureTargetNode();
                            // "Master sends content to nodes" enables Node stream on the new layer immediately.
                            if (!layerCoreProps.captureNodeTargeted && !layerCoreProps.captureMasterOnly && layerCoreProps.nodeSourceComboBox.currentIndex === 1) {
                                layerView.layerItem.layerNodeStreamOutputEnabled = true;
                            } else if (layerCoreProps.captureMasterOnly) {
                                // "Master only": the layer captures on the master and is not synced to the nodes.
                                layerView.layerItem.layerExistOnMasterOnly = true;
                                app.slides.needsSync = true;
                            }
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
                            // "videoDevice|audioDevice" (the audio part may be empty for a video-only
                            // layer, i.e. "No audio capture"). Without a video device the layer plays
                            // back the media file instead.
                            var directShowParam = "";
                            var directShowPresetKey = "";
                            if (!layerCoreProps.captureNodeTargeted && !layerCoreProps.directShowPresetsLayout.customEntry && layerCoreProps.directShowPresetAvailable) {
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
                                // directShowAudioDeviceSelection is empty when "No audio capture" is selected.
                                directShowParam = layerCoreProps.directShowVideoDeviceComboBox.currentText + "|" + layerCoreProps.directShowAudioDeviceSelection;
                            } else if (layerCoreProps.fileForLayer.text !== "") {
                                directShowParam = layerCoreProps.fileForLayer.text;
                            }
                            if (directShowParam !== "" || directShowPresetKey !== "") {
                                layerView.layerItem.layerIdx = app.slides.selected.addLayer(layerCoreProps.layerTitle.text, layerCoreProps.typeComboBox.currentIndex + 1, directShowParam, layerCoreProps.stereoscopicModeForLayer.currentIndex, layerCoreProps.gridModeForLayer.currentIndex);
                                layerView.layerItem.layerCaptureTargetNode = layerCoreProps.captureTargetNode();
                                if (directShowPresetKey !== "") {
                                    layerView.layerItem.layerDirectShowPresetKey = directShowPresetKey;
                                }
                                // "Master sends content to nodes" enables Node stream on the new layer immediately.
                                if (!layerCoreProps.captureNodeTargeted && !layerCoreProps.captureMasterOnly && layerCoreProps.nodeSourceComboBox.currentIndex === 1) {
                                    layerView.layerItem.layerNodeStreamOutputEnabled = true;
                                } else if (layerCoreProps.captureMasterOnly) {
                                    // "Master only": the layer captures on the master and is not synced to the nodes.
                                    layerView.layerItem.layerExistOnMasterOnly = true;
                                    app.slides.needsSync = true;
                                }
                                layersAddNew.visible = false;
                                app.slides.updateSelectedSlide();
                                mpv.focus = true;
                            }
                        } else if (layerCoreProps.typeComboBox.currentText === "Capture") {
                            // The file path holds the default capture source; a predefined setup also stores its
                            // title, which every machine resolves against its own data/predefined-captures.json.
                            var captureParam = layerCoreProps.getCaptureSourceString();
                            var capturePresetKey = layerCoreProps.captureUsesPreset() ? layerCoreProps.capturePresetsComboBox.currentText : "";
                            if (captureParam !== "" || capturePresetKey !== "") {
                                layerView.layerItem.layerIdx = app.slides.selected.addLayer(layerCoreProps.layerTitle.text, layerCoreProps.typeComboBox.currentIndex + 1, captureParam, layerCoreProps.stereoscopicModeForLayer.currentIndex, layerCoreProps.gridModeForLayer.currentIndex);
                                if (capturePresetKey !== "") {
                                    layerView.layerItem.layerCapturePresetKey = capturePresetKey;
                                }
                                if (layerCoreProps.captureTargetNode() !== "") {
                                    layerView.layerItem.layerCaptureTargetNode = layerCoreProps.captureTargetNode();
                                }
                                // "Master sends content to nodes" enables Node stream on the new layer immediately.
                                if (!layerCoreProps.captureNodeTargeted && !layerCoreProps.captureMasterOnly && layerCoreProps.nodeSourceComboBox.currentIndex === 1) {
                                    layerView.layerItem.layerNodeStreamOutputEnabled = true;
                                } else if (layerCoreProps.captureMasterOnly) {
                                    // "Master only": the layer captures on the master and is not synced to the nodes.
                                    layerView.layerItem.layerExistOnMasterOnly = true;
                                    app.slides.needsSync = true;
                                }
                                layersAddNew.visible = false;
                                app.slides.updateSelectedSlide();
                                mpv.focus = true;
                            }
                        } else if (layerCoreProps.typeComboBox.currentText === "Stream") {
                            if(layerCoreProps.streamsLayout.customEntry){
                                layerView.layerItem.layerIdx = app.slides.selected.addLayer(layerCoreProps.layerTitle.text, layerCoreProps.typeComboBox.currentIndex + 1, layerCoreProps.streamCustomEntryField.text, layerCoreProps.stereoscopicModeForLayer.currentIndex, layerCoreProps.gridModeForLayer.currentIndex);
                            }
                            else if(layerCoreProps.streamsLayout.mediaMtxEntry){
                                // MediaMTX streams resolve to a plain RTSP URL, so they behave like a custom path.
                                layerView.layerItem.layerIdx = app.slides.selected.addLayer(layerCoreProps.layerTitle.text, layerCoreProps.typeComboBox.currentIndex + 1, layerCoreProps.mediaMtxStreamsComboBox.currentValue, layerCoreProps.stereoscopicModeForLayer.currentIndex, layerCoreProps.gridModeForLayer.currentIndex);
                            }
                            else {
                                layerView.layerItem.layerIdx = app.slides.selected.addLayer(layerCoreProps.layerTitle.text, layerCoreProps.typeComboBox.currentIndex + 1, layerCoreProps.streamsComboBox.currentValue, layerCoreProps.stereoscopicModeForLayer.currentIndex, layerCoreProps.gridModeForLayer.currentIndex);
                                // Remember which predefined entry this layer was created from (the entry title), so each machine in the cluster can resolve its own local path from data/predefined-streams.json. Custom paths keep an empty key and use the file path verbatim.
                                layerView.layerItem.layerStreamKey = layerCoreProps.streamsComboBox.currentText;
                            }
                            // "Master sends content to nodes" enables Node stream on the new layer immediately.
                            if (layerCoreProps.nodeSourceComboBox.currentIndex === 1) {
                                layerView.layerItem.layerNodeStreamOutputEnabled = true;
                            }
                            layersAddNew.visible = false;
                            app.slides.updateSelectedSlide();
                            mpv.focus = true;
                        } else if (layerCoreProps.typeComboBox.currentText === "YouTube") {
                            if (layerCoreProps.youtubeUrlField.text.trim() !== "") {
                                layerView.layerItem.layerIdx = app.slides.selected.addLayer(layerCoreProps.layerTitle.text, layerCoreProps.typeComboBox.currentIndex + 1, layerCoreProps.youtubeUrlField.text.trim(), layerCoreProps.stereoscopicModeForLayer.currentIndex, layerCoreProps.gridModeForLayer.currentIndex);
                                // "Master sends content to nodes" enables Node stream on the new layer immediately.
                                if (layerCoreProps.nodeSourceComboBox.currentIndex === 1) {
                                    layerView.layerItem.layerNodeStreamOutputEnabled = true;
                                }
                                layersAddNew.visible = false;
                                app.slides.updateSelectedSlide();
                                mpv.focus = true;
                            }
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
                        } else if (layerCoreProps.typeComboBox.currentText === "TCP") {
                            if (layerCoreProps.tcpCommandsComboBox.currentIndex < 0)
                                return;
                            layerView.layerItem.layerIdx = app.slides.selected.addTcpLayer(
                                layerCoreProps.layerTitle.text || layerCoreProps.tcpCommandsComboBox.currentText,
                                layerCoreProps.tcpCommandsComboBox.currentValue);
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
                                // The relay is on by default; only an unchecked box changes that.
                                layerView.layerItem.layerMasterRelay = layerCoreProps.masterRelayCheckBox.checked;
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
