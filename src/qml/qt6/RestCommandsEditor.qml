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
    height: 700
    title: qsTr("REST Commands Editor")
    visible: false
    width: 700

    property int selectedCommandIndex: -1
    property var methodNames: ["GET", "POST", "PUT", "DELETE", "WS", "WSS"]
    property var obsActionNames: [qsTr("Set Profile"), qsTr("Set Scene"), qsTr("Set Scene Collection")]

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
            app.httpClientModel.updateCommandsList();
            selectedCommandIndex = -1;
            clearEditFields();
        }
    }

    Connections {
        target: app.httpClientModel
        function onResponseChanged() {
            var status = app.httpClientModel.lastStatusCode;
            if (status > 0) {
                statusLabel.text = "Status: " + status;
                statusLabel.color = (status >= 200 && status < 300) ? "green" : "orange";
                responseArea.text = app.httpClientModel.lastResponseBody;
            } else {
                statusLabel.text = "Error: " + app.httpClientModel.lastError;
                statusLabel.color = "red";
                responseArea.text = "";
            }
        }
    }

    Connections {
        target: app.wwsClientModel
        function onResponseChanged() {
            var status = app.wwsClientModel.lastStatusCode;
            if (status > 0) {
                statusLabel.text = "Status: " + status;
                statusLabel.color = (status >= 200 && status < 300) ? "green" : "orange";
                responseArea.text = app.wwsClientModel.lastResponseBody;
            } else {
                statusLabel.text = "Error: " + app.wwsClientModel.lastError;
                statusLabel.color = "red";
                responseArea.text = "";
            }
        }
    }

    function requestInProgress() {
        return app.httpClientModel.requestInProgress || app.wwsClientModel.requestInProgress;
    }

    function sendCommand(url, method, parameters, ignoreStatus) {
        if (method === 4 || method === 5 || url.indexOf("ws://") === 0 || url.indexOf("wss://") === 0) {
            app.wwsClientModel.sendRequest(url, parameters, ignoreStatus);
        } else {
            app.httpClientModel.sendRequest(url, method, parameters, ignoreStatus);
        }
    }

    function clearEditFields() {
        editTitle.text = "";
        editUrl.text = "";
        editMethod.currentIndex = 0;
        editIgnoreStatus.checked = false;
        obsConnectCheckBox.checked = false;
        obsActionComboBox.currentIndex = 0;
        obsOptionComboBox.currentIndex = -1;
        obsCustomRequestType.text = "SetCurrentProgramScene";
        paramsModel.clear();
        responseArea.text = "";
        statusLabel.text = "";
    }

    function loadCommand(index) {
        if (index < 0 || index >= app.httpClientModel.numberOfCommands)
            return;
        var m = app.httpClientModel;
        editTitle.text = m.data(m.index(index, 0), Qt.UserRole);
        editUrl.text = m.data(m.index(index, 0), Qt.UserRole + 1);
        editMethod.currentIndex = Math.max(0, Math.min(m.data(m.index(index, 0), Qt.UserRole + 2), root.methodNames.length - 1));
        loadParametersFromJson(m.data(m.index(index, 0), Qt.UserRole + 3));
        editIgnoreStatus.checked = m.data(m.index(index, 0), Qt.UserRole + 4);
        updateObsOptions();
        responseArea.text = "";
        statusLabel.text = "";
    }

    function loadParametersFromJson(jsonStr) {
        paramsModel.clear();
        obsConnectCheckBox.checked = false;
        obsActionComboBox.currentIndex = -1;
        obsOptionComboBox.currentIndex = -1;
        obsCustomRequestType.text = "SetCurrentProgramScene";
        if (!jsonStr || jsonStr === "")
            return;
        try {
            var arr = JSON.parse(jsonStr);
            if (Array.isArray(arr)) {
                for (var i = 0; i < arr.length; i++) {
                    var name = arr[i].name || "";
                    var value = arr[i].value || "";
                    if (name === "requestType") {
                        obsConnectCheckBox.checked = true;
                        obsCustomRequestType.text = value;
                        setObsHelperFromRequestType(value);
                    } else if (name.indexOf("requestData.") === 0) {
                        var dataName = name.substring(12);
                        paramsModel.append({"paramName": dataName, "paramValue": value});
                        if (dataName === obsOptionParameterName()) {
                            obsOptionComboBox.editText = value;
                        }
                    } else {
                        paramsModel.append({"paramName": name, "paramValue": value});
                    }
                }
            }
        } catch(e) {
            // Backward compatibility: try old format
            var pairs = jsonStr.split("&");
            for (var j = 0; j < pairs.length; j++) {
                var eqIdx = pairs[j].indexOf("=");
                if (eqIdx >= 0) {
                    paramsModel.append({"paramName": pairs[j].substring(0, eqIdx), "paramValue": pairs[j].substring(eqIdx + 1)});
                } else if (pairs[j] !== "") {
                    paramsModel.append({"paramName": pairs[j], "paramValue": ""});
                }
            }
        }
    }

    function getParametersAsJson() {
        var arr = [];
        if (isObsCommand()) {
            var requestType = obsCustomRequestType.text;
            if (requestType !== "") {
                arr.push({"name": "requestType", "value": requestType});
            }
        }
        for (var i = 0; i < paramsModel.count; i++) {
            var item = paramsModel.get(i);
            if (item.paramName !== "") {
                var paramName = isObsCommand() && obsCustomRequestType.text !== "" ? "requestData." + item.paramName : item.paramName;
                arr.push({"name": paramName, "value": item.paramValue});
            }
        }
        if (arr.length === 0)
            return "";
        return JSON.stringify(arr);
    }

    function methodName(method) {
        return (method >= 0 && method < root.methodNames.length) ? root.methodNames[method] : "";
    }

    function isObsCommand() {
        return obsConnectCheckBox.checked && editMethod.currentIndex === 4;
    }

    function obsRequestType() {
        if (!isObsCommand())
            return "";
        return obsCustomRequestType.text;
    }

    function obsRequestTypeForAction(actionIndex) {
        if (actionIndex === 0)
            return "SetCurrentProfile";
        if (actionIndex === 1)
            return "SetCurrentProgramScene";
        if (actionIndex === 2)
            return "SetCurrentSceneCollection";
        return "";
    }

    function obsOptionParameterName() {
        return obsOptionParameterNameForAction(obsActionComboBox.currentIndex);
    }

    function obsOptionParameterNameForAction(actionIndex) {
        if (actionIndex === 0)
            return "profileName";
        if (actionIndex === 1)
            return "sceneName";
        if (actionIndex === 2)
            return "sceneCollectionName";
        return "";
    }

    function setOrAppendParameter(paramName, paramValue) {
        if (paramName === "")
            return;
        for (var i = 0; i < paramsModel.count; i++) {
            if (paramsModel.get(i).paramName === paramName) {
                paramsModel.setProperty(i, "paramValue", paramValue);
                return;
            }
        }
        paramsModel.append({"paramName": paramName, "paramValue": paramValue});
    }

    function removeObsHelperParametersExcept(paramNameToKeep) {
        for (var i = paramsModel.count - 1; i >= 0; i--) {
            var paramName = paramsModel.get(i).paramName;
            if (paramName !== paramNameToKeep && (paramName === "profileName" || paramName === "sceneName" || paramName === "sceneCollectionName")) {
                paramsModel.remove(i);
            }
        }
    }

    function applyObsHelperAction() {
        if (!isObsCommand() || obsActionComboBox.currentIndex < 0)
            return;
        obsCustomRequestType.text = obsRequestTypeForAction(obsActionComboBox.currentIndex);
        var optionParameterName = obsOptionParameterName();
        removeObsHelperParametersExcept(optionParameterName);
        setOrAppendParameter(optionParameterName, obsOptionComboBox.currentText);
        updateObsOptions();
    }

    function applyObsHelperTarget() {
        if (!isObsCommand() || obsActionComboBox.currentIndex < 0)
            return;
        setOrAppendParameter(obsOptionParameterName(), obsOptionComboBox.currentText);
    }

    function setObsHelperFromRequestType(requestType) {
        if (requestType === "SetCurrentProfile") {
            obsActionComboBox.currentIndex = 0;
        } else if (requestType === "SetCurrentProgramScene") {
            obsActionComboBox.currentIndex = 1;
        } else if (requestType === "SetCurrentSceneCollection") {
            obsActionComboBox.currentIndex = 2;
        } else {
            obsActionComboBox.currentIndex = -1;
        }
    }

    function updateObsOptions() {
        if (!isObsCommand() || obsActionComboBox.currentIndex < 0 || editUrl.text === "")
            return;
        app.wwsClientModel.updateObsOptions(editUrl.text, obsActionComboBox.currentIndex);
    }

    ListModel {
        id: paramsModel
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        RowLayout {
            Layout.fillWidth: true

            Rectangle {
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
                width: Kirigami.Units.gridUnit
            }
            Kirigami.Heading {
                text: qsTr("Predefined REST Commands")
            }
            Rectangle {
                Layout.fillWidth: true
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
            }
        }

        // Command list
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 170
            Layout.bottomMargin: 10
            spacing: 8

            ColumnLayout {
                Layout.alignment: Qt.AlignTop
                Layout.preferredWidth: moveUpButton.implicitWidth
                Layout.preferredHeight: parent.height
                Layout.maximumWidth: Layout.preferredWidth
                spacing: 4

                ToolButton {
                    id: moveUpButton
                    icon.name: "go-up"
                    visible: true
                    enabled: root.selectedCommandIndex > 0
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Move up")
                    onClicked: {
                        app.httpClientModel.moveCommand(root.selectedCommandIndex, root.selectedCommandIndex - 1);
                        root.selectedCommandIndex = root.selectedCommandIndex - 1;
                        commandsList.currentIndex = root.selectedCommandIndex;
                        root.loadCommand(root.selectedCommandIndex);
                    }
                }
                ToolButton {
                    id: moveDownButton
                    icon.name: "go-down"
                    visible: true
                    enabled: root.selectedCommandIndex >= 0 && root.selectedCommandIndex < app.httpClientModel.numberOfCommands - 1
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Move down")
                    onClicked: {
                        app.httpClientModel.moveCommand(root.selectedCommandIndex, root.selectedCommandIndex + 1);
                        root.selectedCommandIndex = root.selectedCommandIndex + 1;
                        commandsList.currentIndex = root.selectedCommandIndex;
                        root.loadCommand(root.selectedCommandIndex);
                    }
                }
            }

            ListView {
                id: commandsList
                Layout.fillWidth: true
                Layout.preferredHeight: parent.height
                clip: true
                model: app.httpClientModel
                currentIndex: root.selectedCommandIndex

                delegate: ItemDelegate {
                    width: commandsList.width
                    highlighted: index === root.selectedCommandIndex
                    contentItem: RowLayout {
                        Label {
                            Layout.fillWidth: true
                            text: model.title
                            elide: Text.ElideRight
                        }
                        Label {
                            text: root.methodName(model.method) + " "
                            font.bold: true
                            color: Kirigami.Theme.disabledTextColor
                        }
                        Label {
                            text: model.url
                            elide: Text.ElideMiddle
                            Layout.preferredWidth: 250
                            color: Kirigami.Theme.disabledTextColor
                        }
                    }
                    onClicked: {
                        root.selectedCommandIndex = index;
                        root.loadCommand(index);
                    }
                }

                ScrollBar.vertical: ScrollBar {}
            }
        }

        // Edit area
        RowLayout {
            Layout.fillWidth: true

            Rectangle {
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
                width: Kirigami.Units.gridUnit
            }
            Kirigami.Heading {
                level: 3
                text: root.selectedCommandIndex >= 0 ? qsTr("Edit Command") : qsTr("New Command")
            }
            Rectangle {
                Layout.fillWidth: true
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 8
            rowSpacing: 6

            Label { text: qsTr("Title:"); Layout.alignment: Qt.AlignRight }
            TextField {
                id: editTitle
                Layout.fillWidth: true
                placeholderText: "Command name"
            }

            Label { text: qsTr("URL:"); Layout.alignment: Qt.AlignRight }
            TextField {
                id: editUrl
                Layout.fillWidth: true
                placeholderText: "http://host:port/path or ws://host:port/path"
            }

            Label { text: qsTr("Method:"); Layout.alignment: Qt.AlignRight }
            ComboBox {
                id: editMethod
                Layout.fillWidth: true
                model: root.methodNames
                currentIndex: 0
            }

            Label {
                text: qsTr("OBS:")
                Layout.alignment: Qt.AlignRight
                visible: editMethod.currentIndex === 4
            }
            CheckBox {
                id: obsConnectCheckBox
                Layout.fillWidth: true
                text: qsTr("Connecting to OBS")
                visible: editMethod.currentIndex === 4
                onToggled: {
                    if (checked && obsCustomRequestType.text === "") {
                        obsCustomRequestType.text = "SetCurrentProgramScene";
                    }
                    root.updateObsOptions();
                }
            }

            Label {
                text: qsTr("OBS helper:")
                Layout.alignment: Qt.AlignRight
                visible: root.isObsCommand()
            }
            ComboBox {
                id: obsActionComboBox
                Layout.fillWidth: true
                model: root.obsActionNames
                visible: root.isObsCommand()
                onActivated: {
                    obsOptionComboBox.currentIndex = -1;
                    obsOptionComboBox.editText = "";
                    root.applyObsHelperAction();
                }
            }

            Label {
                text: ""
                visible: root.isObsCommand()
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Helper only: choosing a preset fills the request and data fields below. Only those fields are saved and loaded.")
                wrapMode: Text.WordWrap
                color: Kirigami.Theme.disabledTextColor
                visible: root.isObsCommand()
            }

            Label {
                text: qsTr("OBS target helper:")
                Layout.alignment: Qt.AlignRight
                visible: root.isObsCommand() && obsActionComboBox.currentIndex >= 0
            }
            RowLayout {
                Layout.fillWidth: true
                visible: root.isObsCommand() && obsActionComboBox.currentIndex >= 0

                ComboBox {
                    id: obsOptionComboBox
                    Layout.fillWidth: true
                    editable: true
                    model: app.wwsClientModel.obsOptions
                    enabled: !app.wwsClientModel.obsOptionsInProgress
                    onActivated: root.applyObsHelperTarget()
                    onAccepted: root.applyObsHelperTarget()
                    onEditTextChanged: root.applyObsHelperTarget()
                }
                ToolButton {
                    icon.name: "view-refresh"
                    icon.height: 16
                    enabled: editUrl.text !== "" && !app.wwsClientModel.obsOptionsInProgress
                    onClicked: root.updateObsOptions()
                }
            }

            Label {
                text: qsTr("OBS request:")
                Layout.alignment: Qt.AlignRight
                visible: root.isObsCommand()
            }
            TextField {
                id: obsCustomRequestType
                Layout.fillWidth: true
                text: "SetCurrentProgramScene"
                visible: root.isObsCommand()
            }

            Label { text: qsTr("Ignore Status:"); Layout.alignment: Qt.AlignRight }
            CheckBox {
                id: editIgnoreStatus
                checked: false
                text: qsTr("Do not wait for response (assume OK)")
            }

            Label {
                text: root.isObsCommand() && root.obsRequestType() !== "" ? qsTr("Request data:") : qsTr("Parameters:")
                Layout.alignment: Qt.AlignRight | Qt.AlignTop
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Repeater {
                    model: paramsModel
                    delegate: RowLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        TextField {
                            Layout.preferredWidth: 150
                            placeholderText: root.isObsCommand() && root.obsRequestType() !== "" ? "Name" : "Name"
                            text: paramName
                            onTextChanged: paramsModel.setProperty(index, "paramName", text)
                        }
                        Label { text: "=" }
                        TextField {
                            Layout.fillWidth: true
                            placeholderText: "Value"
                            text: paramValue
                            onTextChanged: paramsModel.setProperty(index, "paramValue", text)
                        }
                        ToolButton {
                            icon.name: "list-remove"
                            icon.height: 16
                            onClicked: paramsModel.remove(index)
                        }
                    }
                }

                Button {
                    text: qsTr("+ Add Parameter")
                    icon.name: "list-add"
                    onClicked: paramsModel.append({"paramName": "", "paramValue": ""})
                }
            }
        }

        // Action buttons
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                text: qsTr("Add New")
                icon.name: "list-add"
                enabled: editTitle.text !== "" && editUrl.text !== ""
                onClicked: {
                    app.httpClientModel.addCommand(editTitle.text, editUrl.text,
                        editMethod.currentIndex, getParametersAsJson(),
                        editIgnoreStatus.checked);
                    root.selectedCommandIndex = app.httpClientModel.numberOfCommands - 1;
                }
            }
            Button {
                text: qsTr("Update")
                icon.name: "document-save"
                enabled: root.selectedCommandIndex >= 0 && editTitle.text !== "" && editUrl.text !== ""
                onClicked: {
                    app.httpClientModel.updateCommand(root.selectedCommandIndex,
                        editTitle.text, editUrl.text,
                        editMethod.currentIndex, getParametersAsJson(),
                        editIgnoreStatus.checked);
                }
            }
            Button {
                text: qsTr("Remove")
                icon.name: "list-remove"
                enabled: root.selectedCommandIndex >= 0
                onClicked: {
                    app.httpClientModel.removeCommand(root.selectedCommandIndex);
                    root.selectedCommandIndex = -1;
                    clearEditFields();
                }
            }
            Item { Layout.fillWidth: true }
            Button {
                text: root.requestInProgress() ? qsTr("Sending...") : qsTr("Trigger")
                icon.name: "media-playback-start"
                enabled: editUrl.text !== "" && !root.requestInProgress()
                onClicked: {
                    statusLabel.text = "Sending...";
                    statusLabel.color = Kirigami.Theme.disabledTextColor;
                    responseArea.text = "";
                    root.sendCommand(editUrl.text, editMethod.currentIndex,
                        getParametersAsJson(), editIgnoreStatus.checked);
                }
            }
        }

        // Response section
        RowLayout {
            Layout.fillWidth: true

            Rectangle {
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
                width: Kirigami.Units.gridUnit
            }
            Kirigami.Heading {
                level: 3
                text: qsTr("Response")
            }
            Label {
                id: statusLabel
                text: ""
                font.bold: true
            }
            Rectangle {
                Layout.fillWidth: true
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            TextArea {
                id: responseArea
                readOnly: true
                wrapMode: TextEdit.Wrap
                font.family: "monospace"
                placeholderText: "Response will appear here after triggering a command..."
            }
        }
    }
}
