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
    property alias ndiSenderComboBox: ndiSenderComboBox
    property alias spoutSenderComboBox: spoutSenderComboBox
    property alias omtSenderComboBox: omtSenderComboBox
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
            else if (typeComboBox.currentText === "Stream") {
                app.streamsModel.updateStreamsList();
                streamsLayout.customEntry = false;
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
        visible: typeComboBox.currentText != "Stream" && typeComboBox.currentText != "NDI" && typeComboBox.currentText != "Spout" && typeComboBox.currentText != "OMT" && typeComboBox.currentText != "Text" && typeComboBox.currentText != "Control" && typeComboBox.currentText != "REST"
    }
    RowLayout {
        Layout.fillWidth: true
        visible: typeComboBox.currentText != "Stream" && typeComboBox.currentText != "NDI" && typeComboBox.currentText != "Spout" && typeComboBox.currentText != "OMT" && typeComboBox.currentText != "Text" && typeComboBox.currentText != "Control" && typeComboBox.currentText != "REST"

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

        ComboBox {
            id: streamsComboBox

            Layout.fillWidth: true
            model: app.streamsModel
            currentIndex: 0
            textRole: "title"
            valueRole: "path"
            visible: streamsLayout.customEntry === false

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
        ToolButton {
            id: streamComboOrFieldButton

            focusPolicy: Qt.NoFocus
            icon.height: 16
            icon.name: streamsLayout.customEntry ? "gnumeric-object-combo" : "text-field"
            text: ""

            onClicked: {
                if(streamsLayout.customEntry) {
                    app.streamsModel.updateStreamsList();
                    streamsComboBox.currentIndex = 0;
                    streamsLayout.customEntry = false;
                    layerTitle.text = streamsComboBox.currentText;
                }
                else {
                    streamsLayout.customEntry = true;
                    layerTitle.text = ""
                }

            }

            ToolTip {
                text: streamsLayout.customEntry ? qsTr("Use predefined stream list") : qsTr("Use custom stream path")
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
