/* SPDX-License-Identifier: GPL-3.0-or-later */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.ctoolbox.cplay

SettingsBasePage {
    id: root
    helpUrl: "https://c-toolbox.github.io/C-Play/remote-control/tcp"
    property var manager: app.tcpControlManager
    property string serverId: ""
    property string commandId: ""
    property string commandServerId: ""
    property string triggerResult: ""

    function findIndex(items, id) {
        for (var i = 0; i < items.length; ++i)
            if (items[i].id === id) return i;
        return -1;
    }
    function editServer(s) {
        serverId = s ? s.id : "";
        serverName.text = s ? s.name : "";
        host.text = s ? s.host : "";
        port.value = s ? s.port : 7008;
        enabled.checked = s ? s.enabled : true;
        acceptControl.checked = s ? s.acceptControlCommands : false;
        reconnect.checked = s ? s.autoReconnect : true;
        framing.currentIndex = s ? s.framing : 0;
        receiveHex.text = s ? s.receiveTerminatorHex : "0a";
        sendHex.text = s ? s.sendTerminatorHex : "0a";
        timeout.value = s ? s.connectTimeoutMs : 5000;
        retryInitial.value = s ? s.retryInitialMs : 1000;
        retryMaximum.value = s ? s.retryMaximumMs : 30000;
        stable.value = s ? s.stableConnectionMs : 10000;
        messageLimit.value = s ? s.maximumMessageBytes : 1048576;
        writeLimit.value = s ? s.maximumPendingWriteBytes : 1048576;
    }
    function saveServer() {
        var id = manager.saveServer({id: serverId, name: serverName.text, host: host.text,
            port: port.value, enabled: enabled.checked, acceptControlCommands: acceptControl.checked,
            autoReconnect: reconnect.checked, framing: framing.currentIndex,
            receiveTerminatorHex: receiveHex.text, sendTerminatorHex: sendHex.text,
            connectTimeoutMs: timeout.value, retryInitialMs: retryInitial.value,
            retryMaximumMs: retryMaximum.value, stableConnectionMs: stable.value,
            maximumMessageBytes: messageLimit.value, maximumPendingWriteBytes: writeLimit.value});
        if (id !== "") serverId = id;
    }
    function editCommand(c) {
        commandId = c ? c.id : "";
        commandName.text = c ? c.name : "";
        commandServerId = c ? c.serverId : (manager.servers.length ? manager.servers[0].id : "");
        encoding.currentIndex = c && c.encoding === "hex" ? 1 : 0;
        payload.text = c ? c.payload : "";
        raw.checked = c ? c.raw : false;
    }
    Component.onCompleted: { editServer(null); editCommand(null); }
    Connections {
        target: root.manager
        function onCommandTriggered(id, accepted) {
            root.triggerResult = root.manager.commandDescription(id) + ": " +
                (accepted ? qsTr("Accepted by TCP transport") : qsTr("Rejected"));
        }
    }

    ColumnLayout {
        width: parent.width
        spacing: 12
        SettingsHeader { Layout.fillWidth: true; text: qsTr("Persistent TCP servers") }
        Label {
            Layout.fillWidth: true; wrapMode: Text.WordWrap
            text: qsTr("Each enabled server keeps its own connection on the master. Define reusable commands below, then select them in TCP presentation layers.")
        }
        Repeater {
            model: root.manager.servers
            delegate: Label {
                required property var modelData
                Layout.fillWidth: true; wrapMode: Text.WordWrap
                text: modelData.name + " (" + modelData.host + ":" + modelData.port + "): " +
                    modelData.state + (modelData.lastError ? " — " + modelData.lastError : "")
            }
        }
        RowLayout {
            ComboBox {
                Layout.fillWidth: true
                model: root.manager.servers; textRole: "name"; valueRole: "id"
                currentIndex: root.findIndex(root.manager.servers, root.serverId)
                onActivated: root.editServer(root.manager.servers[index])
            }
            Button { text: qsTr("New server"); onClicked: root.editServer(null) }
        }
        GridLayout {
            columns: 2; Layout.fillWidth: true
            Label { text: qsTr("Name:") }
            TextField { id: serverName; Layout.fillWidth: true }
            Label { text: qsTr("Host:") }
            TextField { id: host; Layout.fillWidth: true; placeholderText: qsTr("Hostname or IP address") }
            Label { text: qsTr("Port:") }
            SpinBox { id: port; from: 1; to: 65535; editable: true }
            Label { text: qsTr("Enabled:") }
            CheckBox { id: enabled }
            Label { text: qsTr("Receive C-Play control commands:") }
            CheckBox { id: acceptControl; text: qsTr("Parse incoming JSON commands") }
            Label { text: qsTr("Automatic reconnect:") }
            CheckBox { id: reconnect }
            Label { text: qsTr("Framing:") }
            ComboBox { id: framing; model: [qsTr("Delimiter"), qsTr("32-bit big-endian length")] }
            Label { text: qsTr("Receive terminator (hex):") }
            TextField { id: receiveHex; Layout.fillWidth: true; enabled: framing.currentIndex === 0; placeholderText: "0a = LF, 0d0a = CRLF, 00 = NUL" }
            Label { text: qsTr("Send terminator (hex):") }
            TextField { id: sendHex; Layout.fillWidth: true; enabled: framing.currentIndex === 0 }
            Label { text: qsTr("Connection timeout (ms):") }
            SpinBox { id: timeout; from: 1; to: 600000; editable: true }
            Label { text: qsTr("Initial retry delay (ms):") }
            SpinBox { id: retryInitial; from: 1; to: 600000; editable: true }
            Label { text: qsTr("Maximum retry delay (ms):") }
            SpinBox { id: retryMaximum; from: 1; to: 600000; editable: true }
            Label { text: qsTr("Reset backoff after (ms):") }
            SpinBox { id: stable; from: 1; to: 600000; editable: true }
            Label { text: qsTr("Maximum message (bytes):") }
            SpinBox { id: messageLimit; from: 1; to: 67108864; editable: true }
            Label { text: qsTr("Maximum pending writes (bytes):") }
            SpinBox { id: writeLimit; from: 1; to: 67108864; editable: true }
        }
        RowLayout {
            Button { text: qsTr("Save server"); onClicked: root.saveServer() }
            Button {
                text: qsTr("Delete server"); enabled: root.serverId !== ""
                onClicked: if (root.manager.removeServer(root.serverId)) root.editServer(null)
            }
            Button { text: qsTr("Connect"); enabled: root.manager.active && root.serverId !== ""; onClicked: root.manager.connectServer(root.serverId) }
            Button { text: qsTr("Disconnect"); enabled: root.serverId !== ""; onClicked: root.manager.disconnectServer(root.serverId) }
        }
        SettingsHeader { Layout.fillWidth: true; text: qsTr("Reusable TCP commands") }
        RowLayout {
            ComboBox {
                Layout.fillWidth: true
                model: root.manager.commands; textRole: "name"; valueRole: "id"
                currentIndex: root.findIndex(root.manager.commands, root.commandId)
                onActivated: root.editCommand(root.manager.commands[index])
            }
            Button { text: qsTr("New command"); onClicked: root.editCommand(null) }
        }
        GridLayout {
            columns: 2; Layout.fillWidth: true
            Label { text: qsTr("Name:") }
            TextField { id: commandName; Layout.fillWidth: true }
            Label { text: qsTr("Server:") }
            ComboBox {
                id: commandServer; Layout.fillWidth: true
                model: root.manager.servers; textRole: "name"; valueRole: "id"
                currentIndex: root.findIndex(root.manager.servers, root.commandServerId)
                onActivated: root.commandServerId = currentValue
            }
            Label { text: qsTr("Payload:") }
            ComboBox { id: encoding; model: [qsTr("UTF-8 text"), qsTr("Hexadecimal bytes")] }
            Label { text: qsTr("Exact wire bytes:") }
            CheckBox { id: raw; text: qsTr("Send without adding framing or termination") }
        }
        TextArea {
            id: payload; Layout.fillWidth: true; Layout.preferredHeight: 120
            wrapMode: TextEdit.Wrap
            placeholderText: encoding.currentIndex === 0 ? qsTr("Command text") : qsTr("Hex byte pairs, e.g. 020100ff03")
        }
        RowLayout {
            Button {
                text: qsTr("Save command")
                onClicked: {
                    var id = root.manager.saveCommand({id: root.commandId, name: commandName.text,
                        serverId: root.commandServerId, encoding: encoding.currentIndex === 0 ? "text" : "hex",
                        payload: payload.text, raw: raw.checked});
                    if (id !== "") root.commandId = id;
                }
            }
            Button {
                text: qsTr("Delete command"); enabled: root.commandId !== ""
                onClicked: if (root.manager.removeCommand(root.commandId)) root.editCommand(null)
            }
            Button { text: qsTr("Trigger saved command"); enabled: root.commandId !== ""; onClicked: root.manager.triggerCommand(root.commandId) }
        }
        Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: root.triggerResult }
        Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: root.manager.lastError }
        Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: playerController.tcpControlError }
        Label {
            Layout.fillWidth: true; wrapMode: Text.WordWrap
            text: qsTr("Disconnected commands are rejected and are never replayed after reconnect. Deleting a command leaves any referencing layers with a missing-command error. Configuration: ") + root.manager.configurationPath
        }
    }
}
