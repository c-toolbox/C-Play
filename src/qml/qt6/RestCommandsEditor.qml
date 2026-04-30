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
    property var methodNames: ["GET", "POST", "PUT", "DELETE"]

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

    function clearEditFields() {
        editTitle.text = "";
        editUrl.text = "";
        editMethod.currentIndex = 0;
        editIgnoreStatus.checked = false;
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
        editMethod.currentIndex = m.data(m.index(index, 0), Qt.UserRole + 2);
        loadParametersFromJson(m.data(m.index(index, 0), Qt.UserRole + 3));
        editIgnoreStatus.checked = m.data(m.index(index, 0), Qt.UserRole + 4);
        responseArea.text = "";
        statusLabel.text = "";
    }

    function loadParametersFromJson(jsonStr) {
        paramsModel.clear();
        if (!jsonStr || jsonStr === "")
            return;
        try {
            var arr = JSON.parse(jsonStr);
            if (Array.isArray(arr)) {
                for (var i = 0; i < arr.length; i++) {
                    paramsModel.append({"paramName": arr[i].name || "", "paramValue": arr[i].value || ""});
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
        for (var i = 0; i < paramsModel.count; i++) {
            var item = paramsModel.get(i);
            if (item.paramName !== "") {
                arr.push({"name": item.paramName, "value": item.paramValue});
            }
        }
        if (arr.length === 0)
            return "";
        return JSON.stringify(arr);
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
        ListView {
            id: commandsList
            Layout.fillWidth: true
            Layout.preferredHeight: 170
            Layout.bottomMargin: 10
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
                        text: root.methodNames[model.method] + " "
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
                placeholderText: "http://host:port/path"
            }

            Label { text: qsTr("Method:"); Layout.alignment: Qt.AlignRight }
            ComboBox {
                id: editMethod
                Layout.fillWidth: true
                model: root.methodNames
                currentIndex: 0
            }

            Label { text: qsTr("Ignore Status:"); Layout.alignment: Qt.AlignRight }
            CheckBox {
                id: editIgnoreStatus
                checked: false
                text: qsTr("Do not wait for response (assume OK)")
            }

            Label {
                text: qsTr("Parameters:")
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
                            placeholderText: "Name"
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
                text: app.httpClientModel.requestInProgress ? qsTr("Sending...") : qsTr("Trigger")
                icon.name: "media-playback-start"
                enabled: editUrl.text !== "" && !app.httpClientModel.requestInProgress
                onClicked: {
                    statusLabel.text = "Sending...";
                    statusLabel.color = Kirigami.Theme.disabledTextColor;
                    responseArea.text = "";
                    app.httpClientModel.sendRequest(editUrl.text,
                        editMethod.currentIndex, getParametersAsJson(),
                        editIgnoreStatus.checked);
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
