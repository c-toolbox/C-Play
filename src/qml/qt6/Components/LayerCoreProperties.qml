/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Shapes
import QtQuick.Dialogs
import Qt5Compat.GraphicalEffects

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

GridLayout {
    id: root
    columnSpacing: 2
    columns: 2
    rowSpacing: 8

    property bool showSpacers: false
    property bool showTitleParams: true
    property bool showGridParams: true
    property bool showStereoParams: true

    property alias typeComboBox: typeComboBox
    property alias fileForLayer: fileForLayer
    property alias layerTitle: layerTitle
    property alias streamsLayout: streamsLayout
    property alias streamsComboBox: streamsComboBox
    property alias streamCustomEntryField: streamCustomEntryField
    property alias mediaMtxServerComboBox: mediaMtxServerComboBox
    property alias mediaMtxStreamsComboBox: mediaMtxStreamsComboBox
    property alias ndiSenderComboBox: ndiSenderComboBox
    property alias spoutSenderComboBox: spoutSenderComboBox
    property alias omtSenderComboBox: omtSenderComboBox
    property alias directShowVideoDeviceComboBox: directShowVideoDeviceComboBox
    property alias directShowAudioDeviceComboBox: directShowAudioDeviceComboBox
    property alias directShowPresetsLayout: directShowPresetsLayout
    property alias directShowPresetsComboBox: directShowPresetsComboBox
    property alias whepUrlField: whepUrlField
    property alias stereoscopicModeForLayer: stereoscopicModeForLayer
    property alias gridModeForLayer: gridModeForLayer
    property alias textForLayer: textForLayer
    property alias controlOperationComboBox: controlOperationComboBox
    property alias controlParameterField: controlParameterField
    property alias restCommandsLayout: restCommandsLayout
    property alias restCommandsComboBox: restCommandsComboBox
    property alias restCustomUrlField: restCustomUrlField
    property alias restMethodComboBox: restMethodComboBox
    property alias restIgnoreStatusCheckBox: restIgnoreStatusCheckBox

    // True when data/predefined-directshows.json provides at least one enabled capture setup.
    property bool directShowPresetAvailable: app.directShowPresetsModel && app.directShowPresetsModel.numberOfPresets > 0

    // MediaMTX support is a build option (BUILD_CPLAY_WITH_MEDIA_MTX); in builds without it the
    // models simply do not exist on app, so this stays false and the MediaMTX stream picker is hidden.
    property bool mediaMtxAvailable: !!app.mediaMtxServersModel && !!app.mediaMtxModel

    // The audio device selected in the DirectShow section; an empty string means "No audio capture"
    // (the first entry of the audio combobox) or that no selection is available.
    property string directShowAudioDeviceSelection: {
        if (directShowAudioDeviceComboBox.currentIndex <= 0)
            return "";
        return directShowAudioDeviceComboBox.currentText;
    }

    property string restParametersJson: ""
    property var restObsActionNames: [qsTr("Set Profile"), qsTr("Set Scene"), qsTr("Set Scene Collection"), qsTr("Custom")]

    function resetValues() {
        typeComboBox.currentIndex = 0;
        fileForLayer.text = "";
        layerTitle.text = "";
        restObsConnectCheckBox.checked = false;
        restObsActionComboBox.currentIndex = 0;
        restObsOptionComboBox.currentIndex = -1;
        restObsCustomRequestType.text = "SetCurrentProgramScene";
        restCoreParamsModel.clear();
        for (let sm = 0; sm < stereoscopicModeForLayerList.count; ++sm) {
            if (stereoscopicModeForLayerList.get(sm).value === PresentationSettings.defaultStereoModeForLayers) {
                stereoscopicModeForLayer.currentIndex = sm;
                break;
            }
        }
        for (let gm = 0; gm < gridModeForLayerList.count; ++gm) {
            if (gridModeForLayerList.get(gm).value === PresentationSettings.defaultGridModeForLayers) {
                gridModeForLayer.currentIndex = gm;
                break;
            }
        }
        directShowPresetsLayout.customEntry = false;
    }

    // Returns the video/audio device combination of the currently selected predefined DirectShow setup, or null when none is available.
    function getDirectShowPresetDevices() {
        var m = app.directShowPresetsModel;
        if (!m || directShowPresetsComboBox.currentIndex < 0)
            return null;
        var idx = directShowPresetsComboBox.currentIndex;
        return { video: m.data(m.index(idx, 0), Qt.UserRole), audio: m.data(m.index(idx, 0), Qt.UserRole + 1) };
    }

    // Mirrors the selected predefined setup onto the custom device comboboxes so that switching to the
    // custom options starts from the same devices. Devices not present on this machine are left unselected.
    function applyDirectShowPresetToDevices() {
        var devices = getDirectShowPresetDevices();
        if (!devices || !app.directShowModel)
            return;
        var vi = app.directShowModel.videoDevices.indexOf(devices.video);
        directShowVideoDeviceComboBox.currentIndex = vi >= 0 ? vi : -1;
        var ai = app.directShowModel.audioDevices.indexOf(devices.audio);
        // Index 0 of the audio combobox is "No audio capture"; real devices start at index 1.
        directShowAudioDeviceComboBox.currentIndex = ai >= 0 ? ai + 1 : (devices.audio === "" ? 0 : -1);
    }

    function getRestParametersJson() {
        var arr = [];
        if (isRestObsCommand()) {
            var requestType = restObsRequestType();
            if (requestType !== "") {
                arr.push({"name": "requestType", "value": requestType});
                var optionParameterName = restObsOptionParameterName();
                if (optionParameterName !== "" && restObsOptionComboBox.currentText !== "") {
                    arr.push({"name": "requestData." + optionParameterName, "value": restObsOptionComboBox.currentText});
                }
            }
        }
        for (var i = 0; i < restCoreParamsModel.count; i++) {
            var item = restCoreParamsModel.get(i);
            if (item.paramName !== "") {
                var paramName = isRestObsCommand() && restObsRequestType() !== "" ? "requestData." + item.paramName : item.paramName;
                arr.push({"name": paramName, "value": item.paramValue});
            }
        }
        if (arr.length === 0)
            return "";
        return JSON.stringify(arr);
    }

    function loadRestParametersFromJson(jsonStr) {
        restCoreParamsModel.clear();
        restObsConnectCheckBox.checked = false;
        restObsActionComboBox.currentIndex = 0;
        restObsOptionComboBox.currentIndex = -1;
        restObsCustomRequestType.text = "SetCurrentProgramScene";
        if (!jsonStr || jsonStr === "")
            return;
        try {
            var arr = JSON.parse(jsonStr);
            if (Array.isArray(arr)) {
                for (var i = 0; i < arr.length; i++) {
                    var name = arr[i].name || "";
                    var value = arr[i].value || "";
                    if (name === "requestType") {
                        restObsConnectCheckBox.checked = true;
                        setRestObsActionFromRequestType(value);
                    } else if (name.indexOf("requestData.") === 0) {
                        var dataName = name.substring(12);
                        if (dataName === "profileName" || dataName === "sceneName" || dataName === "sceneCollectionName") {
                            restObsOptionComboBox.editText = value;
                        } else {
                            restCoreParamsModel.append({"paramName": dataName, "paramValue": value});
                        }
                    } else {
                        restCoreParamsModel.append({"paramName": name, "paramValue": value});
                    }
                }
            }
        } catch(e) {
            var pairs = jsonStr.split("&");
            for (var j = 0; j < pairs.length; j++) {
                var eqIdx = pairs[j].indexOf("=");
                if (eqIdx >= 0) {
                    restCoreParamsModel.append({"paramName": pairs[j].substring(0, eqIdx), "paramValue": pairs[j].substring(eqIdx + 1)});
                } else if (pairs[j] !== "") {
                    restCoreParamsModel.append({"paramName": pairs[j], "paramValue": ""});
                }
            }
        }
    }

    ListModel {
        id: restCoreParamsModel
    }

    function isRestObsCommand() {
        return typeComboBox.currentText === "REST" && restCommandsLayout.customEntry === true
            && restObsConnectCheckBox.checked && restMethodComboBox.currentIndex === 4;
    }

    function restObsRequestType() {
        if (!isRestObsCommand())
            return "";
        if (restObsActionComboBox.currentIndex === 0)
            return "SetCurrentProfile";
        if (restObsActionComboBox.currentIndex === 1)
            return "SetCurrentProgramScene";
        if (restObsActionComboBox.currentIndex === 2)
            return "SetCurrentSceneCollection";
        return restObsCustomRequestType.text;
    }

    function restObsOptionParameterName() {
        if (restObsActionComboBox.currentIndex === 0)
            return "profileName";
        if (restObsActionComboBox.currentIndex === 1)
            return "sceneName";
        if (restObsActionComboBox.currentIndex === 2)
            return "sceneCollectionName";
        return "";
    }

    function ensureRestCustomObsExample() {
        if (!isRestObsCommand() || restObsActionComboBox.currentIndex !== 3)
            return;
        if (restObsCustomRequestType.text === "")
            restObsCustomRequestType.text = "SetCurrentProgramScene";
        for (var i = 0; i < restCoreParamsModel.count; i++) {
            if (restCoreParamsModel.get(i).paramName === "sceneName")
                return;
        }
        restCoreParamsModel.append({"paramName": "sceneName", "paramValue": "Scene"});
    }

    function setRestObsActionFromRequestType(requestType) {
        if (requestType === "SetCurrentProfile") {
            restObsActionComboBox.currentIndex = 0;
        } else if (requestType === "SetCurrentProgramScene") {
            restObsActionComboBox.currentIndex = 1;
        } else if (requestType === "SetCurrentSceneCollection") {
            restObsActionComboBox.currentIndex = 2;
        } else {
            restObsActionComboBox.currentIndex = 3;
            restObsCustomRequestType.text = requestType;
            ensureRestCustomObsExample();
        }
    }

    function updateRestObsOptions() {
        if (!isRestObsCommand() || restObsActionComboBox.currentIndex === 3 || restCustomUrlField.text === "")
            return;
        app.wwsClientModel.updateObsOptions(restCustomUrlField.text, restObsActionComboBox.currentIndex);
    }

    CPlayFileDialog {
        id: fileToLoadAsImageLayerDialog
        property bool acceptedOnes: false

        parentWindow: root.Window.window
        fileMode: CPlayFileDialog.OpenFile
        currentFolder: fileToLoadAsImageLayerDialog.acceptedOnes ? app.pathToUrl(LocationSettings.imageFileDialogLastLocation) : app.pathToUrl(LocationSettings.imageFileDialogLocation)
        nameFilters: [playerController.supportedImageNameFilters()]
        title: "Choose image file"

        onAccepted: {
            fileForLayer.text = playerController.checkAndCorrectPath(fileToLoadAsImageLayerDialog.selectedFile);
            layerTitle.text = playerController.returnFileBaseName(fileForLayer.text);
            LocationSettings.imageFileDialogLastLocation = app.parentUrl(fileToLoadAsImageLayerDialog.selectedFile);
            LocationSettings.save();
            fileToLoadAsImageLayerDialog.acceptedOnes = true;
            if (typeComboBox.currentText === "Image") {
                analyzeImageSequence(fileForLayer.text);
            }
        }
    }
    CPlayFileDialog {
        id: fileToLoadAsPdfLayerDialog
        property bool acceptedOnes: false

        parentWindow: root.Window.window
        fileMode: CPlayFileDialog.OpenFile
        currentFolder: fileToLoadAsPdfLayerDialog.acceptedOnes ? app.pathToUrl(LocationSettings.pdfFileDialogLastLocation) : app.pathToUrl(LocationSettings.pdfFileDialogLocation)
        nameFilters: ["PDF files (*.pdf)"]
        title: "Choose pdf file"

        onAccepted: {
            fileForLayer.text = playerController.checkAndCorrectPath(fileToLoadAsPdfLayerDialog.selectedFile);
            layerTitle.text = playerController.returnFileBaseName(fileForLayer.text);
            LocationSettings.pdfFileDialogLastLocation = app.parentUrl(fileToLoadAsPdfLayerDialog.selectedFile);
            LocationSettings.save();
            fileToLoadAsPdfLayerDialog.acceptedOnes = true;
        }
    }
    CPlayFileDialog {
        id: fileToLoadAsVideoLayerDialog
        property bool acceptedOnes: false

        parentWindow: root.Window.window
        fileMode: CPlayFileDialog.OpenFile
        currentFolder: fileToLoadAsVideoLayerDialog.acceptedOnes ? app.pathToUrl(LocationSettings.videoFileDialogLastLocation) : app.pathToUrl(LocationSettings.videoFileDialogLocation)
        title: "Choose video file"

        onAccepted: {
            fileForLayer.text = playerController.checkAndCorrectPath(fileToLoadAsVideoLayerDialog.selectedFile);
            layerTitle.text = playerController.returnFileBaseName(fileForLayer.text);
            LocationSettings.videoFileDialogLastLocation = app.parentUrl(fileToLoadAsVideoLayerDialog.selectedFile);
            LocationSettings.save();
            fileToLoadAsVideoLayerDialog.acceptedOnes = true;
        }
    }
    CPlayFileDialog {
        id: fileToLoadAsAudioLayerDialog
        property bool acceptedOnes: false

        parentWindow: root.Window.window
        fileMode: CPlayFileDialog.OpenFile
        currentFolder: fileToLoadAsAudioLayerDialog.acceptedOnes ? app.pathToUrl(LocationSettings.audioFileDialogLastLocation) : app.pathToUrl(LocationSettings.audioFileDialogLocation)
        title: "Choose audio file"

        onAccepted: {
            fileForLayer.text = playerController.checkAndCorrectPath(fileToLoadAsAudioLayerDialog.selectedFile);
            layerTitle.text = playerController.returnFileBaseName(fileForLayer.text);
            LocationSettings.audioFileDialogLastLocation = app.parentUrl(fileToLoadAsAudioLayerDialog.selectedFile);
            LocationSettings.save();
            fileToLoadAsAudioLayerDialog.acceptedOnes = true;
        }
    }

    CPlayFileDialog {
        id: fileToLoadAsMultiVideoLayerDialog
        property bool acceptedOnes: false

        parentWindow: root.Window.window
        fileMode: CPlayFileDialog.OpenFile
        currentFolder: fileToLoadAsMultiVideoLayerDialog.acceptedOnes ? app.pathToUrl(LocationSettings.videoFileDialogLastLocation) : app.pathToUrl(LocationSettings.videoFileDialogLocation)
        nameFilters: ["Multi-Video composition files (*.json)", "All files (*)"]
        title: "Choose multi-video composition JSON"

        onAccepted: {
            fileForLayer.text = playerController.checkAndCorrectPath(fileToLoadAsMultiVideoLayerDialog.selectedFile);
            layerTitle.text = playerController.returnFileBaseName(fileForLayer.text);
            LocationSettings.videoFileDialogLastLocation = app.parentUrl(fileToLoadAsMultiVideoLayerDialog.selectedFile);
            LocationSettings.save();
            fileToLoadAsMultiVideoLayerDialog.acceptedOnes = true;
        }
    }

    Label {
        Layout.alignment: Qt.AlignRight
        font.pointSize: 9
        text: qsTr("Type:")
    }
    ComboBox {
        id: typeComboBox

        Layout.fillWidth: true
        model: app.slides.selected.layersTypeModel
        textRole: "typeName"

        onActivated: {
            if (typeComboBox.currentText === "NDI") {
                app.ndiSendersModel.updateSendersList();
                ndiSenderComboBox.currentIndex = app.ndiSendersModel.numberOfSenders - 1;
                layerTitle.text = ndiSenderComboBox.currentText;
            }
            else if (typeComboBox.currentText === "Spout") {
                app.spoutSendersModel.updateSendersList();
                spoutSenderComboBox.currentIndex = app.spoutSendersModel.numberOfSenders - 1;
                layerTitle.text = spoutSenderComboBox.currentText;
            }
            else if (typeComboBox.currentText === "OMT") {
                app.omtSendersModel.updateSendersList();
                omtSenderComboBox.currentIndex = app.omtSendersModel.numberOfSenders - 1;
                layerTitle.text = omtSenderComboBox.currentText;
            }
            else if (typeComboBox.currentText === "DirectShow") {
                if (app.directShowModel) {
                    app.directShowModel.updateDeviceLists();
                    directShowVideoDeviceComboBox.currentIndex = app.directShowModel.videoDevices.length - 1;
                    // Index 0 of the audio combobox is "No audio capture"; default to the last real device.
                    directShowAudioDeviceComboBox.currentIndex = app.directShowModel.audioDevices.length;
                }
                if (root.directShowPresetAvailable) {
                    // Predefined setups exist: default to the first one, like Stream layers do.
                    directShowPresetsLayout.customEntry = false;
                    directShowPresetsComboBox.currentIndex = 0;
                    applyDirectShowPresetToDevices();
                    layerTitle.text = directShowPresetsComboBox.currentText;
                } else {
                    layerTitle.text = "DirectShow:" + (directShowVideoDeviceComboBox.currentText || "");
                }
            }
            else if (typeComboBox.currentText === "Stream") {
                app.streamsModel.updateStreamsList();
                streamsLayout.customEntry = false;
                streamsLayout.mediaMtxEntry = false;
                streamsComboBox.currentIndex = 0;
                streamCustomEntryField.text = "";
                layerTitle.text = streamsComboBox.currentText;
            }
            else if (typeComboBox.currentText === "Control") {
                controlOperationComboBox.currentIndex = 0;
                layerTitle.text = "Ctrl:" + controlOperationComboBox.currentText;
            }
            else if (typeComboBox.currentText === "REST") {
                app.httpClientModel.updateCommandsList();
                restCommandsLayout.customEntry = false;
                restCommandsComboBox.currentIndex = 0;
                layerTitle.text = restCommandsComboBox.currentText;
                var idx = restCommandsComboBox.currentIndex;
                restCustomUrlField.text = app.httpClientModel.data(app.httpClientModel.index(idx, 0), Qt.UserRole + 1);
                restMethodComboBox.currentIndex = app.httpClientModel.data(app.httpClientModel.index(idx, 0), Qt.UserRole + 2);
                loadRestParametersFromJson(app.httpClientModel.data(app.httpClientModel.index(idx, 0), Qt.UserRole + 3));
                restIgnoreStatusCheckBox.checked = app.httpClientModel.data(app.httpClientModel.index(idx, 0), Qt.UserRole + 4) || false;
            }
            else if (typeComboBox.currentText === "WebRTC") {
                whepUrlField.text = "";
            }
            else {
                layerTitle.text = "";
                fileForLayer.text = "";
            }
        }
    }
    Item {
        visible: root.showSpacers
        Layout.fillWidth: true
    }

    Label {
        Layout.alignment: Qt.AlignRight
        font.pointSize: 9
        text: qsTr("File:")
        visible: typeComboBox.currentText != "Stream" && typeComboBox.currentText != "DirectShow"&& typeComboBox.currentText != "NDI" && typeComboBox.currentText != "Spout" && typeComboBox.currentText != "OMT" && typeComboBox.currentText != "WebRTC" && typeComboBox.currentText != "Text" && typeComboBox.currentText != "Control" && typeComboBox.currentText != "REST"
    }
    RowLayout {
        Layout.fillWidth: true
        visible: typeComboBox.currentText != "Stream" && typeComboBox.currentText != "DirectShow" && typeComboBox.currentText != "NDI" && typeComboBox.currentText != "Spout" && typeComboBox.currentText != "OMT" && typeComboBox.currentText != "WebRTC" && typeComboBox.currentText != "Text" && typeComboBox.currentText != "Control" && typeComboBox.currentText != "REST"

        TextField {
            id: fileForLayer

            Layout.fillWidth: true
            Layout.preferredWidth: font.pointSize * 17
            placeholderText: "Path to file"
            text: ""

            onEditingFinished: {}

            DropArea {
                id: dropAreaFileForLayer

                property int layerType: -1

                anchors.fill: parent
                keys: ["text/uri-list"]

                onDropped: function(drop) {
                    if(app.slides.selected.layersEnabled){
                        layerType = app.slides.selected.getLayerTypeBasedOnMime(drop.urls[0]);
                        if(layerType > 0 && layerType < typeComboBox.count) {
                            if (layerType !== typeComboBox.currentIndex + 1) {
                                typeComboBox.currentIndex = layerType - 1;
                            }
                            fileForLayer.text = playerController.checkAndCorrectPath(drop.urls[0]);
                            layerTitle.text = playerController.returnFileBaseName(fileForLayer.text);
                        }
                    }
                }
            }

            ToolTip {
                text: qsTr("Path to file for layer")
            }
        }
        ToolButton {
            id: fileToLoadAsLayerButton

            focusPolicy: Qt.NoFocus
            icon.height: 16
            icon.name: "document-open"
            text: ""

            onClicked: {
                if (typeComboBox.currentText === "Image")
                    fileToLoadAsImageLayerDialog.open();
                else if (typeComboBox.currentText === "PDF")
                    fileToLoadAsPdfLayerDialog.open();
                else if (typeComboBox.currentText === "Video")
                    fileToLoadAsVideoLayerDialog.open();
                else if (typeComboBox.currentText === "Audio")
                    fileToLoadAsAudioLayerDialog.open();
                else if (typeComboBox.currentText === "MultiVideo")
                    fileToLoadAsMultiVideoLayerDialog.open();
            }
        }
    }

    Label {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Path:")
        visible: typeComboBox.currentText === "Stream"
    }
    RowLayout {
        id: streamsLayout
        Layout.fillWidth: true
        visible: typeComboBox.currentText === "Stream"
        property bool customEntry: false
        property bool mediaMtxEntry: false

        ComboBox {
            id: streamsComboBox

            Layout.fillWidth: true
            model: app.streamsModel
            currentIndex: 0
            textRole: "title"
            valueRole: "path"
            visible: streamsLayout.customEntry === false && streamsLayout.mediaMtxEntry === false

            Component.onCompleted: {
                app.streamsModel.updateStreamsList();
                streamsLayout.customEntry = false;
                streamsComboBox.currentIndex = 0;
                layerTitle.text = streamsComboBox.currentText;
            }
            onActivated: {
                layerTitle.text = streamsComboBox.currentText;
            }
        }
        TextField {
            id: streamCustomEntryField

            Layout.fillWidth: true
            Layout.preferredWidth: font.pointSize * 17
            placeholderText: "Stream path (see mpv docs).."
            text: ""
            visible: streamsLayout.customEntry === true

            onEditingFinished: {}

            ToolTip {
                text: qsTr("Stream path (see mpv docs)..")
            }
        }
        ComboBox {
            id: mediaMtxServerComboBox

            Layout.fillWidth: true
            Layout.preferredWidth: font.pointSize * 8
            model: app.mediaMtxServersModel
            textRole: "name"
            currentIndex: 0
            visible: root.mediaMtxAvailable && streamsLayout.mediaMtxEntry === true

            onActivated: {
                app.mediaMtxModel.refresh(mediaMtxServerComboBox.currentIndex);
            }

            ToolTip {
                text: qsTr("MediaMTX server (configured in Settings -> MediaMTX Streams)")
            }
        }
        ComboBox {
            id: mediaMtxStreamsComboBox

            Layout.fillWidth: true
            Layout.preferredWidth: font.pointSize * 12
            model: app.mediaMtxModel
            textRole: "name"
            valueRole: "rtspUrl"
            currentIndex: 0
            visible: root.mediaMtxAvailable && streamsLayout.mediaMtxEntry === true

            onActivated: {
                layerTitle.text = mediaMtxStreamsComboBox.currentText;
            }

            ToolTip {
                text: qsTr("Streams published on the selected MediaMTX server")
            }
        }
        ToolButton {
            id: mediaMtxRefreshButton

            focusPolicy: Qt.NoFocus
            icon.height: 16
            icon.name: "view-refresh"
            text: ""
            visible: root.mediaMtxAvailable && streamsLayout.mediaMtxEntry === true
            enabled: root.mediaMtxAvailable && !app.mediaMtxModel.refreshInProgress && app.mediaMtxServersModel.numberOfServers > 0

            onClicked: {
                app.mediaMtxModel.refresh(mediaMtxServerComboBox.currentIndex);
            }

            ToolTip {
                text: qsTr("Fetch stream list from the MediaMTX server")
            }
        }
        ToolButton {
            id: streamComboOrFieldButton

            focusPolicy: Qt.NoFocus
            icon.height: 16
            icon.name: streamsLayout.customEntry ? (root.mediaMtxAvailable ? "network-server" : "gnumeric-object-combo") : (streamsLayout.mediaMtxEntry ? "gnumeric-object-combo" : "text-field")
            text: ""

            onClicked: {
                if(streamsLayout.customEntry) {
                    streamsLayout.customEntry = false;
                    if (root.mediaMtxAvailable) {
                        // custom -> MediaMTX
                        streamsLayout.mediaMtxEntry = true;
                        app.mediaMtxServersModel.updateServersList();
                        layerTitle.text = mediaMtxStreamsComboBox.currentText;
                    } else {
                        // custom -> predefined list (no MediaMTX in this build)
                        app.streamsModel.updateStreamsList();
                        streamsComboBox.currentIndex = 0;
                        layerTitle.text = streamsComboBox.currentText;
                    }
                }
                else if(streamsLayout.mediaMtxEntry) {
                    // MediaMTX -> predefined list
                    streamsLayout.mediaMtxEntry = false;
                    app.streamsModel.updateStreamsList();
                    streamsComboBox.currentIndex = 0;
                    layerTitle.text = streamsComboBox.currentText;
                }
                else {
                    // predefined list -> custom
                    streamsLayout.customEntry = true;
                    layerTitle.text = ""
                }

            }

            ToolTip {
                text: streamsLayout.customEntry ? (root.mediaMtxAvailable ? qsTr("Use MediaMTX server") : qsTr("Use predefined stream list")) : (streamsLayout.mediaMtxEntry ? qsTr("Use predefined stream list") : qsTr("Use custom stream path"))
            }
        }
    }

    Label {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Name:")
        visible: typeComboBox.currentText === "NDI" || typeComboBox.currentText === "Spout" || typeComboBox.currentText === "OMT"
    }
    RowLayout {
        Layout.fillWidth: true
        visible: typeComboBox.currentText === "NDI"

        ComboBox {
            id: ndiSenderComboBox

            Layout.fillWidth: true
            model: app.ndiSendersModel
            currentIndex: (app.ndiSendersModel ? app.ndiSendersModel.numberOfSenders - 1 : -1)
            textRole: "typeName"

            Component.onCompleted: {
                if(app.ndiSendersModel){
                    app.ndiSendersModel.updateSendersList();
                    ndiSenderComboBox.currentIndex = app.ndiSendersModel.numberOfSenders - 1;
                    layerTitle.text = ndiSenderComboBox.currentText;
                }
            }
            onActivated: {
                layerTitle.text = ndiSenderComboBox.currentText;
            }
        }
        ToolButton {
            id: updateNdiSendersBox

            focusPolicy: Qt.NoFocus
            icon.height: 16
            icon.name: "view-refresh"
            text: ""

            onClicked: {
                if(app.ndiSendersModel){
                    app.ndiSendersModel.updateSendersList();
                    ndiSenderComboBox.currentIndex = app.ndiSendersModel.numberOfSenders - 1;
                    layerTitle.text = ndiSenderComboBox.currentText;
                }
            }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        visible: typeComboBox.currentText === "Spout"

        ComboBox {
            id: spoutSenderComboBox

            Layout.fillWidth: true
            model: app.spoutSendersModel
            currentIndex: (app.spoutSendersModel ? app.spoutSendersModel.numberOfSenders - 1 : -1)
            textRole: "typeName"

            Component.onCompleted: {
                if(app.spoutSendersModel){
                    app.spoutSendersModel.updateSendersList();
                    spoutSenderComboBox.currentIndex = app.spoutSendersModel.numberOfSenders - 1;
                    layerTitle.text = spoutSenderComboBox.currentText;
                }
            }
            onActivated: {
                layerTitle.text = spoutSenderComboBox.currentText;
            }
        }
        ToolButton {
            id: updateSpoutSendersBox

            focusPolicy: Qt.NoFocus
            icon.height: 16
            icon.name: "view-refresh"
            text: ""

            onClicked: {
                if(app.spoutSendersModel){
                    app.spoutSendersModel.updateSendersList();
                    spoutSenderComboBox.currentIndex = app.spoutSendersModel.numberOfSenders - 1;
                    layerTitle.text = spoutSenderComboBox.currentText;
                }
            }
        }
    }
    Item {
        visible: root.showSpacers && typeComboBox.currentText === "Spout"
        Layout.fillWidth: true
    }
    RowLayout {
        Layout.fillWidth: true
        visible: typeComboBox.currentText === "OMT"

        ComboBox {
            id: omtSenderComboBox

            Layout.fillWidth: true
            model: app.omtSendersModel
            currentIndex: (app.omtSendersModel ? app.omtSendersModel.numberOfSenders - 1 : -1)
            textRole: "typeName"

            Component.onCompleted: {
                if(app.omtSendersModel){
                    app.omtSendersModel.updateSendersList();
                    omtSenderComboBox.currentIndex = app.omtSendersModel.numberOfSenders - 1;
                    layerTitle.text = omtSenderComboBox.currentText;
                }
            }
            onActivated: {
                layerTitle.text = omtSenderComboBox.currentText;
            }
        }
        ToolButton {
            id: updateOmtSendersBox

            focusPolicy: Qt.NoFocus
            icon.height: 16
            icon.name: "view-refresh"
            text: ""

            onClicked: {
                if(app.omtSendersModel){
                    app.omtSendersModel.updateSendersList();
                    omtSenderComboBox.currentIndex = app.omtSendersModel.numberOfSenders - 1;
                    layerTitle.text = omtSenderComboBox.currentText;
                }
            }
        }
    }
    Item {
        visible: root.showSpacers && typeComboBox.currentText === "OMT"
        Layout.fillWidth: true
    }

    Label {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Setup:")
        visible: typeComboBox.currentText === "DirectShow" && root.directShowPresetAvailable
    }
    RowLayout {
        id: directShowPresetsLayout

        Layout.fillWidth: true
        property bool customEntry: false
        visible: typeComboBox.currentText === "DirectShow" && root.directShowPresetAvailable

        ComboBox {
            id: directShowPresetsComboBox

            Layout.fillWidth: true
            model: app.directShowPresetsModel ? app.directShowPresetsModel : []
            currentIndex: 0
            textRole: "title"
            visible: directShowPresetsLayout.customEntry === false

            Component.onCompleted: {
                if (app.directShowPresetsModel) {
                    app.directShowPresetsModel.updatePresetsList();
                    directShowPresetsLayout.customEntry = false;
                    directShowPresetsComboBox.currentIndex = 0;
                    layerTitle.text = directShowPresetsComboBox.currentText;
                }
            }
            onActivated: {
                layerTitle.text = directShowPresetsComboBox.currentText;
                applyDirectShowPresetToDevices();
            }
        }
        ToolButton {
            id: directShowPresetComboOrFieldButton

            focusPolicy: Qt.NoFocus
            icon.height: 16
            icon.name: directShowPresetsLayout.customEntry ? "gnumeric-object-combo" : "text-field"
            text: ""

            onClicked: {
                if (directShowPresetsLayout.customEntry) {
                    app.directShowPresetsModel.updatePresetsList();
                    directShowPresetsComboBox.currentIndex = 0;
                    applyDirectShowPresetToDevices();
                    directShowPresetsLayout.customEntry = false;
                    layerTitle.text = directShowPresetsComboBox.currentText;
                } else {
                    directShowPresetsLayout.customEntry = true;
                    layerTitle.text = "DirectShow:" + (directShowVideoDeviceComboBox.currentText || "");
                }
            }

            ToolTip {
                text: directShowPresetsLayout.customEntry ? qsTr("Use predefined setup list") : qsTr("Use custom capture devices")
            }
        }
    }

    Label {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Video device:")
        visible: typeComboBox.currentText === "DirectShow" && (!root.directShowPresetAvailable || directShowPresetsLayout.customEntry)
    }
    RowLayout {
        Layout.fillWidth: true
        visible: typeComboBox.currentText === "DirectShow" && (!root.directShowPresetAvailable || directShowPresetsLayout.customEntry)

        ComboBox {
            id: directShowVideoDeviceComboBox

            Layout.fillWidth: true
            model: app.directShowModel ? app.directShowModel.videoDevices : []
            currentIndex: (app.directShowModel && app.directShowModel.videoDevices.length > 0) ? app.directShowModel.videoDevices.length - 1 : -1

            Component.onCompleted: {
                if (app.directShowModel) {
                    app.directShowModel.updateDeviceLists();
                    if (root.directShowPresetAvailable) {
                        // A predefined setup is shown by default - mirror its devices onto the custom comboboxes.
                        applyDirectShowPresetToDevices();
                    } else {
                        directShowVideoDeviceComboBox.currentIndex = app.directShowModel.videoDevices.length - 1;
                        layerTitle.text = "DirectShow:" + (directShowVideoDeviceComboBox.currentText || "");
                    }
                }
            }
            onActivated: {
                layerTitle.text = "DirectShow:" + (directShowVideoDeviceComboBox.currentText || "");
            }
        }
        ToolButton {
            id: updateDirectShowDevicesBox

            focusPolicy: Qt.NoFocus
            icon.height: 16
            icon.name: "view-refresh"
            text: ""

            onClicked: {
                if (app.directShowModel) {
                    app.directShowModel.updateDeviceLists();
                    directShowVideoDeviceComboBox.currentIndex = app.directShowModel.videoDevices.length - 1;
                    // Index 0 of the audio combobox is "No audio capture"; default to the last real device.
                    directShowAudioDeviceComboBox.currentIndex = app.directShowModel.audioDevices.length;
                    layerTitle.text = "DirectShow:" + (directShowVideoDeviceComboBox.currentText || "");
                }
            }

            ToolTip {
                text: qsTr("Rescan capture devices")
            }
        }
    }
    Item {
        visible: root.showSpacers && typeComboBox.currentText === "DirectShow" && (!root.directShowPresetAvailable || directShowPresetsLayout.customEntry)
        Layout.fillWidth: true
    }

    Label {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Audio device:")
        visible: typeComboBox.currentText === "DirectShow" && (!root.directShowPresetAvailable || directShowPresetsLayout.customEntry)
    }
    RowLayout {
        Layout.fillWidth: true
        visible: typeComboBox.currentText === "DirectShow" && (!root.directShowPresetAvailable || directShowPresetsLayout.customEntry)

        ComboBox {
            id: directShowAudioDeviceComboBox

            Layout.fillWidth: true
            // Index 0 is the explicit "No audio capture" option (video-only layer); real devices start at index 1.
            // Note: use .concat(), not "+", which would stringify both arrays into one long string.
            model: [qsTr("No audio capture")].concat(app.directShowModel ? app.directShowModel.audioDevices : [])
            currentIndex: (app.directShowModel && app.directShowModel.audioDevices.length > 0) ? app.directShowModel.audioDevices.length : 0

            Component.onCompleted: {
                if (app.directShowModel) {
                    // Default to the last real device; index 0 is "No audio capture".
                    directShowAudioDeviceComboBox.currentIndex = app.directShowModel.audioDevices.length;
                }
            }
        }
    }
    Item {
        visible: root.showSpacers && typeComboBox.currentText === "DirectShow" && (!root.directShowPresetAvailable || directShowPresetsLayout.customEntry)
        Layout.fillWidth: true
    }

    Label {
        Layout.alignment: Qt.AlignRight
        font.pointSize: 9
        text: qsTr("WHEP URL:")
        visible: typeComboBox.currentText === "WebRTC"
    }
    TextField {
        id: whepUrlField

        Layout.fillWidth: true
        Layout.preferredWidth: font.pointSize * 17
        placeholderText: "http://host:8889/live/stream/whep"
        text: ""
        visible: typeComboBox.currentText === "WebRTC"

        onEditingFinished: {
            // Default the layer title to the stream name when the user did not set one.
            if (layerTitle.text === "" && text.trim() !== "") {
                var noQuery = text.trim().split("?")[0];
                var segments = noQuery.split("/");
                for (var i = segments.length - 1; i >= 0; --i) {
                    if (segments[i] !== "" && segments[i].toLowerCase() !== "whep") {
                        layerTitle.text = segments[i];
                        break;
                    }
                }
            }
        }

        ToolTip {
            text: qsTr("WHEP endpoint URL, e.g. http://mediamtx:8889/live/mystream/whep")
        }
    }
    Item {
        visible: root.showSpacers && typeComboBox.currentText === "WebRTC"
        Layout.fillWidth: true
    }

    Label {
        visible: showTitleParams
        Layout.alignment: Qt.AlignRight
        font.pointSize: 9
        text: qsTr("Title:")
    }
    TextField {
        id: layerTitle
        visible: showTitleParams
        Layout.fillWidth: true
        font.pointSize: 9
        maximumLength: 30
        placeholderText: "Layer title"
        text: ""
    }
    Item {
        visible: showTitleParams && root.showSpacers
        Layout.fillWidth: true
    }

    Label {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Stereo:")
        visible: showStereoParams && typeComboBox.currentText !== "Audio" && typeComboBox.currentText !== "Text" && typeComboBox.currentText !== "Control" && typeComboBox.currentText !== "REST"
    }
    ComboBox {
        id: stereoscopicModeForLayer

        Layout.fillWidth: true
        focusPolicy: Qt.NoFocus
        textRole: "mode"
        visible: showStereoParams && typeComboBox.currentText !== "Audio" && typeComboBox.currentText !== "Text" && typeComboBox.currentText !== "Control" && typeComboBox.currentText !== "REST"

        model: ListModel {
            id: stereoscopicModeForLayerList

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


        Component.onCompleted: {}
        onActivated: {}
    }
    Item {
        visible: root.showSpacers && typeComboBox.currentText !== "Audio" && typeComboBox.currentText !== "Text" && typeComboBox.currentText !== "Control" && typeComboBox.currentText !== "REST"
        Layout.fillWidth: true
    }

    Label {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Grid:")
        visible: showGridParams && typeComboBox.currentText !== "Audio" && typeComboBox.currentText !== "Text" && typeComboBox.currentText !== "Control" && typeComboBox.currentText !== "REST"
    }
    ComboBox {
        id: gridModeForLayer

        Layout.fillWidth: true
        focusPolicy: Qt.NoFocus
        textRole: "mode"
        visible: showGridParams && typeComboBox.currentText !== "Audio" && typeComboBox.currentText !== "Text" && typeComboBox.currentText !== "Control" && typeComboBox.currentText !== "REST"

        model: ListModel {
            id: gridModeForLayerList

            ListElement {
                mode: "None/Pre-split"
                value: 0
            }
            ListElement {
                mode: "Plane/Flat"
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

        Component.onCompleted: {}
        onActivated: {}
    }
    Item {
        visible: root.showSpacers && typeComboBox.currentText !== "Audio" && typeComboBox.currentText !== "Text" && typeComboBox.currentText !== "Control" && typeComboBox.currentText !== "REST"
        Layout.fillWidth: true
    }

    // --- Text layer section ---
    Label {
        Layout.alignment: Qt.AlignRight | Qt.AlignTop
        text: qsTr("Text:")
        visible: typeComboBox.currentText == "Text"
    }
    ScrollView {
        id: view
        visible: typeComboBox.currentText == "Text"

        TextArea {
            id: textForLayer
            visible: typeComboBox.currentText == "Text"
            text: "Some text.\n...more text..."
        }

        Layout.fillHeight: true
        Layout.fillWidth: true
    }
    Item {
        visible: root.showSpacers && typeComboBox.currentText == "Text"
        Layout.fillWidth: true
    }

    // --- Control layer section ---
    Label {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Operation:")
        visible: typeComboBox.currentText === "Control"
    }
    ComboBox {
        id: controlOperationComboBox

        Layout.fillWidth: true
        visible: typeComboBox.currentText === "Control"

        model: ["Play", "Pause", "Stop", "Rewind", "Seek", "SetPosition", 
                "FadeVolumeDown", "FadeVolumeUp", "FadeImageDown", "FadeImageUp",
                "LoadFromAudioTracks", "LoadFromPlaylist", "LoadFromSections", "LoadFromSlides", 
                "SetSpeed", "SetVolume", "SetSyncVolumeVisibilityFading",
                "SetBackgroundVisibility", "SetForegroundVisibility", "SetNodeWindowsOpacity",
                "SpinPitchUp", "SpinPitchDown", "SpinYawLeft", "SpinYawRight",
                "SpinRollCW", "SpinRollCCW", "OrientationAndSpinReset", "RunSurfaceTransition"]
        
        onActivated: {
            if (layerTitle.text === "" || layerTitle.text.startsWith("Ctrl:")) {
                layerTitle.text = "Ctrl:" + controlOperationComboBox.currentText;
            }
        }
    }
    Item {
        visible: root.showSpacers && typeComboBox.currentText === "Control"
        Layout.fillWidth: true
    }

    Label {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Parameter:")
        visible: typeComboBox.currentText === "Control" && controlNeedsParam()
    }
    TextField {
        id: controlParameterField

        Layout.fillWidth: true
        Layout.preferredWidth: font.pointSize * 17
        placeholderText: controlParamHint()
        text: ""
        visible: typeComboBox.currentText === "Control" && controlNeedsParam()

        ToolTip {
            text: qsTr("Parameter value (name/identifier for list items)")
        }
    }
    Item {
        visible: root.showSpacers && typeComboBox.currentText === "Control" && controlNeedsParam()
        Layout.fillWidth: true
    }

    // --- REST layer section ---
    Label {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Command:")
        visible: typeComboBox.currentText === "REST"
    }
    RowLayout {
        id: restCommandsLayout
        Layout.fillWidth: true
        visible: typeComboBox.currentText === "REST"
        property bool customEntry: false

        ComboBox {
            id: restCommandsComboBox

            Layout.fillWidth: true
            model: app.httpClientModel
            currentIndex: 0
            textRole: "title"
            valueRole: "url"
            visible: restCommandsLayout.customEntry === false

            Component.onCompleted: {
                app.httpClientModel.updateCommandsList();
                restCommandsLayout.customEntry = false;
                restCommandsComboBox.currentIndex = 0;
                layerTitle.text = restCommandsComboBox.currentText;
                var idx = restCommandsComboBox.currentIndex;
                restCustomUrlField.text = app.httpClientModel.data(app.httpClientModel.index(idx, 0), Qt.UserRole + 1);
                restMethodComboBox.currentIndex = app.httpClientModel.data(app.httpClientModel.index(idx, 0), Qt.UserRole + 2);
                loadRestParametersFromJson(app.httpClientModel.data(app.httpClientModel.index(idx, 0), Qt.UserRole + 3));
                restIgnoreStatusCheckBox.checked = app.httpClientModel.data(app.httpClientModel.index(idx, 0), Qt.UserRole + 4) || false;
            }
            onActivated: {
                layerTitle.text = restCommandsComboBox.currentText;
                var idx = restCommandsComboBox.currentIndex;
                restCustomUrlField.text = app.httpClientModel.data(app.httpClientModel.index(idx, 0), Qt.UserRole + 1);
                restMethodComboBox.currentIndex = app.httpClientModel.data(app.httpClientModel.index(idx, 0), Qt.UserRole + 2);
                loadRestParametersFromJson(app.httpClientModel.data(app.httpClientModel.index(idx, 0), Qt.UserRole + 3));
                restIgnoreStatusCheckBox.checked = app.httpClientModel.data(app.httpClientModel.index(idx, 0), Qt.UserRole + 4) || false;
            }
        }
        TextField {
            id: restCustomUrlField

            Layout.fillWidth: true
            Layout.preferredWidth: font.pointSize * 17
            placeholderText: "http://host:port/path or ws://host:port/path"
            text: ""
            visible: restCommandsLayout.customEntry === true

            ToolTip {
                text: qsTr("Custom REST API URL")
            }
        }
        ToolButton {
            id: restComboOrFieldButton

            focusPolicy: Qt.NoFocus
            icon.height: 16
            icon.name: restCommandsLayout.customEntry ? "gnumeric-object-combo" : "text-field"
            text: ""

            onClicked: {
                if(restCommandsLayout.customEntry) {
                    restCommandsComboBox.currentIndex = restCommandsComboBox.currentIndex;
                    restCommandsLayout.customEntry = false;
                }
                else {
                    restCommandsLayout.customEntry = true;
                }
            }

            ToolTip {
                text: restCommandsLayout.customEntry ? qsTr("Use predefined command list") : qsTr("Use custom REST command")
            }
        }
    }
    Item {
        visible: root.showSpacers && typeComboBox.currentText === "REST"
        Layout.fillWidth: true
    }

    Label {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Method:")
        visible: typeComboBox.currentText === "REST" && restCommandsLayout.customEntry === true
    }
    ComboBox {
        id: restMethodComboBox

        Layout.fillWidth: true
        visible: typeComboBox.currentText === "REST" && restCommandsLayout.customEntry === true
        model: ["GET", "POST", "PUT", "DELETE", "WS", "WSS"]
        currentIndex: 0
    }
    Item {
        visible: root.showSpacers && typeComboBox.currentText === "REST"
        Layout.fillWidth: true
    }

    Label {
        Layout.alignment: Qt.AlignRight
        text: qsTr("OBS:")
        visible: typeComboBox.currentText === "REST" && restCommandsLayout.customEntry === true && restMethodComboBox.currentIndex === 4
    }
    CheckBox {
        id: restObsConnectCheckBox

        Layout.fillWidth: true
        visible: typeComboBox.currentText === "REST" && restCommandsLayout.customEntry === true && restMethodComboBox.currentIndex === 4
        text: qsTr("Connecting to OBS")
        onToggled: updateRestObsOptions()
    }
    Item {
        visible: root.showSpacers && typeComboBox.currentText === "REST" && restCommandsLayout.customEntry === true && restMethodComboBox.currentIndex === 4
        Layout.fillWidth: true
    }

    Label {
        Layout.alignment: Qt.AlignRight
        text: qsTr("OBS command:")
        visible: isRestObsCommand()
    }
    ComboBox {
        id: restObsActionComboBox

        Layout.fillWidth: true
        visible: isRestObsCommand()
        model: root.restObsActionNames
        onActivated: {
            restObsOptionComboBox.currentIndex = -1;
            restObsOptionComboBox.editText = "";
            if (restObsActionComboBox.currentIndex === 3)
                ensureRestCustomObsExample();
            updateRestObsOptions();
        }
    }
    Item {
        visible: root.showSpacers && isRestObsCommand()
        Layout.fillWidth: true
    }

    Label {
        Layout.alignment: Qt.AlignRight
        text: qsTr("OBS target:")
        visible: isRestObsCommand() && restObsActionComboBox.currentIndex !== 3
    }
    RowLayout {
        Layout.fillWidth: true
        visible: isRestObsCommand() && restObsActionComboBox.currentIndex !== 3

        ComboBox {
            id: restObsOptionComboBox
            Layout.fillWidth: true
            editable: true
            model: app.wwsClientModel.obsOptions
            enabled: !app.wwsClientModel.obsOptionsInProgress
        }
        ToolButton {
            icon.name: "view-refresh"
            icon.height: 16
            enabled: restCustomUrlField.text !== "" && !app.wwsClientModel.obsOptionsInProgress
            onClicked: updateRestObsOptions()
        }
    }
    Item {
        visible: root.showSpacers && isRestObsCommand() && restObsActionComboBox.currentIndex !== 3
        Layout.fillWidth: true
    }

    Label {
        Layout.alignment: Qt.AlignRight
        text: qsTr("OBS request:")
        visible: isRestObsCommand() && restObsActionComboBox.currentIndex === 3
    }
    TextField {
        id: restObsCustomRequestType

        Layout.fillWidth: true
        visible: isRestObsCommand() && restObsActionComboBox.currentIndex === 3
        text: "SetCurrentProgramScene"
    }
    Item {
        visible: root.showSpacers && isRestObsCommand() && restObsActionComboBox.currentIndex === 3
        Layout.fillWidth: true
    }

    Label {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Ignore Status:")
        visible: typeComboBox.currentText === "REST" && restCommandsLayout.customEntry === true
    }
    CheckBox {
        id: restIgnoreStatusCheckBox

        Layout.fillWidth: true
        visible: typeComboBox.currentText === "REST" && restCommandsLayout.customEntry === true
        checked: false
        text: qsTr("Do not wait for response")
    }
    Item {
        visible: root.showSpacers && typeComboBox.currentText === "REST"
        Layout.fillWidth: true
    }

    Label {
        Layout.alignment: Qt.AlignRight
        text: isRestObsCommand() && restObsRequestType() !== "" ? qsTr("Request data:") : qsTr("Parameters:")
        visible: typeComboBox.currentText === "REST" && restCommandsLayout.customEntry === true
    }
    ColumnLayout {
        Layout.fillWidth: true
        visible: typeComboBox.currentText === "REST" && restCommandsLayout.customEntry === true
        spacing: 4

        Repeater {
            model: restCoreParamsModel
            delegate: RowLayout {
                Layout.fillWidth: true
                spacing: 4

                TextField {
                    Layout.preferredWidth: 120
                    placeholderText: "Name"
                    text: paramName
                    onTextChanged: restCoreParamsModel.setProperty(index, "paramName", text)
                }
                Label { text: "=" }
                TextField {
                    Layout.fillWidth: true
                    placeholderText: "Value"
                    text: paramValue
                    onTextChanged: restCoreParamsModel.setProperty(index, "paramValue", text)
                }
                ToolButton {
                    icon.name: "list-remove"
                    icon.height: 16
                    onClicked: restCoreParamsModel.remove(index)
                }
            }
        }

        Button {
            text: qsTr("+ Add Parameter")
            icon.name: "list-add"
            onClicked: restCoreParamsModel.append({"paramName": "", "paramValue": ""})
        }
    }
    Item {
        visible: root.showSpacers && typeComboBox.currentText === "REST" && restCommandsLayout.customEntry === true
        Layout.fillWidth: true
    }

    function controlNeedsParam() {
        var op = controlOperationComboBox.currentText;
        return op === "Seek" || op === "SetPosition"
            || op === "LoadFromAudioTracks" || op === "LoadFromPlaylist"
            || op === "LoadFromSections" || op === "LoadFromSlides"
            || op === "SetSpeed" || op === "SetVolume"
            || op === "SetSyncVolumeVisibilityFading"
            || op === "SetBackgroundVisibility" || op === "SetForegroundVisibility"
            || op === "SetNodeWindowsOpacity"
            || op === "SpinPitchUp" || op === "SpinPitchDown"
            || op === "SpinYawLeft" || op === "SpinYawRight"
            || op === "SpinRollCW" || op === "SpinRollCCW";
    }

    function controlParamHint() {
        var op = controlOperationComboBox.currentText;
        if (op === "Seek") return "Time in seconds";
        if (op === "SetPosition") return "Position (0.0-1.0)";
        if (op === "SetSpeed") return "Speed factor";
        if (op === "SetVolume") return "Volume level (0-100)";
        if (op === "SetSyncVolumeVisibilityFading") return "true/false";
        if (op === "SetBackgroundVisibility") return "Visibility (0.0-1.0)";
        if (op === "SetForegroundVisibility") return "Visibility (0.0-1.0)";
        if (op === "SetNodeWindowsOpacity") return "Opacity (0.0-1.0)";
        if (op === "LoadFromAudioTracks") return "Audio track name";
        if (op === "LoadFromPlaylist") return "Playlist item title/filename";
        if (op === "LoadFromSections") return "Section title";
        if (op === "LoadFromSlides") return "Slide name";
        if (op.startsWith("Spin")) return "true/false (run)";
        return "";
    }

    Item {
        enabled: typeComboBox.currentText != "Text" && typeComboBox.currentText != "Control" && typeComboBox.currentText != "REST"
        Layout.columnSpan: 2
        Layout.fillHeight: true
        // spacer item
        Layout.fillWidth: true
    }
    Item {
        visible: root.showSpacers && typeComboBox.currentText != "Text" && typeComboBox.currentText != "Control" && typeComboBox.currentText != "REST"
        Layout.fillWidth: true
    }
}
