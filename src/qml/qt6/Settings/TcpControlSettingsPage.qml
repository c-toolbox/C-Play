/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.ctoolbox.cplay

SettingsBasePage {
    id: root
    helpUrl: "https://cplay.appvis.se/remote-control/tcp"
    property var manager: app.tcpControlManager
    property string serverId: ""
    property string commandId: ""
    property string commandServerId: ""
    property string triggerResult: ""
    property string incomingId: ""
    property string incomingCommandServerId: ""
    // Topics left out of the profile's on-connect subscription to all topics.
    property var incomingExcluded: []

    function findIndex(items, id) {
        for (var i = 0; i < items.length; ++i)
            if (items[i].id === id) return i;
        return -1;
    }
    function topics(text) {
        return text.split(",").map(function(value) { return value.trim(); })
            .filter(function(value) { return value.length > 0; });
    }
    function editServer(s) {
        serverId = s ? s.id : "";
        serverName.text = s ? s.name : "";
        host.text = s ? s.host : "";
        port.value = s ? s.port : 7008;
        enabled.checked = s ? s.enabled : true;
        connectOnStartup.checked = s ? s.connectOnStartup !== false : true;
        acceptControl.checked = s ? s.acceptControlCommands : false;
        serverEventTopics.text = s && s.eventTopics ? s.eventTopics.join(", ") : "";
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
        var previousId = serverId;
        var id = manager.saveServer({id: serverId, name: serverName.text, host: host.text,
            port: port.value, enabled: enabled.checked,
            connectOnStartup: connectOnStartup.checked, acceptControlCommands: acceptControl.checked,
            eventTopics: topics(serverEventTopics.text),
            autoReconnect: reconnect.checked, framing: framing.currentIndex,
            receiveTerminatorHex: receiveHex.text, sendTerminatorHex: sendHex.text,
            connectTimeoutMs: timeout.value, retryInitialMs: retryInitial.value,
            retryMaximumMs: retryMaximum.value, stableConnectionMs: stable.value,
            maximumMessageBytes: messageLimit.value, maximumPendingWriteBytes: writeLimit.value});
        if (id !== "") {
            serverId = id;
            if (commandServerId === previousId) commandServerId = id;
        }
    }
    function editCommand(c) {
        commandId = c ? c.id : "";
        commandName.text = c ? c.name : "";
        commandServerId = c ? c.serverId : (manager.servers.length ? manager.servers[0].id : "");
        encoding.currentIndex = c && c.encoding === "hex" ? 1 : 0;
        payload.text = c ? c.payload : "";
        raw.checked = c ? c.raw : false;
        commandEventTopics.text = c && c.eventTopics ? c.eventTopics.join(", ") : "";
        eventPayload.currentIndex = c && c.eventPayload !== "fixed" && c.eventPayload ? 1 : 0;
        charsPerItem.value = c && c.charsPerItem ? c.charsPerItem : 0;
        timeFormat.text = c && c.timeFormat ? c.timeFormat : "";
    }
    function editIncoming(p) {
        incomingId = p ? p.id : "";
        incomingName.text = p ? p.name : "";
        incomingAddress.text = p ? p.address : "";
        incomingEnabled.checked = p ? p.enabled : true;
        // New profiles subscribe to all topics; explicit topic lists show as all minus the others.
        var subscribed = p && p.eventTopics ? p.eventTopics : ["*"];
        incomingSubscribe.checked = subscribed.length > 0;
        if (subscribed.indexOf("*") >= 0)
            incomingExcluded = p && p.excludedTopics ? p.excludedTopics.slice() : [];
        else if (subscribed.length > 0)
            incomingExcluded = manager.eventTopics.filter(function(topic) { return subscribed.indexOf(topic) < 0; });
        else
            incomingExcluded = [];
        incomingFormat.currentIndex = p && p.eventFormat === "commands" ? 1 : 0;
        incomingCommandServerId = p && p.commandServerId ? p.commandServerId
            : (manager.servers.length ? manager.servers[0].id : "");
        incomingSendHex.text = p ? p.sendTerminatorHex : "0a";
        incomingReplies.checked = p ? p.sendReplies !== false : true;
        incomingInitialUpdate.checked = p ? p.sendInitialUpdate !== false : true;
    }
    function setIncomingTopic(topic, subscribed) {
        var excluded = incomingExcluded.filter(function(value) { return value !== topic; });
        if (!subscribed) excluded.push(topic);
        incomingExcluded = excluded;
    }
    function saveIncoming() {
        var id = manager.saveIncomingClient({id: incomingId, name: incomingName.text,
            address: incomingAddress.text, enabled: incomingEnabled.checked,
            eventTopics: incomingSubscribe.checked ? ["*"] : [],
            excludedTopics: incomingSubscribe.checked ? incomingExcluded : [],
            eventFormat: incomingFormat.currentIndex === 1 ? "commands" : "json",
            commandServerId: incomingCommandServerId, sendTerminatorHex: incomingSendHex.text,
            sendReplies: incomingReplies.checked, sendInitialUpdate: incomingInitialUpdate.checked});
        if (id !== "") incomingId = id;
    }
    Component.onCompleted: {
        editServer(manager.servers.length ? manager.servers[0] : null);
        editCommand(null);
        editIncoming(manager.incomingClients.length ? manager.incomingClients[0] : null);
    }
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
        SettingsHeader { Layout.fillWidth: true; text: qsTr("Incoming TCP control") }
        CheckBox { id: listenerEnabled; text: qsTr("Enable TCP server on master"); checked: TcpControlSettings.serverEnabled }
        RowLayout {
            Label { text: qsTr("Bind address:") }
            TextField { id: listenerAddress; text: TcpControlSettings.serverBindAddress }
            Label { text: qsTr("Port:") }
            DeferredSpinBox { id: listenerPort; from: 1; to: 65535; value: TcpControlSettings.serverPort; editable: true }
            Button {
                text: qsTr("Apply listener")
                onClicked: {
                    TcpControlSettings.serverEnabled = listenerEnabled.checked;
                    TcpControlSettings.serverBindAddress = listenerAddress.text;
                    TcpControlSettings.serverPort = listenerPort.value;
                    TcpControlSettings.save();
                    playerController.applyTcpControlSettings();
                }
            }
        }
        Label {
            Layout.fillWidth: true; wrapMode: Text.WordWrap
            text: root.manager.listener.listening
                ? qsTr("Listening on port %1; %2 clients connected").arg(root.manager.listener.port).arg(root.manager.listener.clientCount)
                : qsTr("TCP server stopped")
        }
        Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: root.manager.listener.lastError }
        SettingsHeader { Layout.fillWidth: true; text: qsTr("Incoming client subscriptions") }
        Label {
            Layout.fillWidth: true; wrapMode: Text.WordWrap
            text: qsTr("Clients of the TCP server subscribe to changes with /subscribe, /unsubscribe and /full_update. A profile, selected by the client's address, defines what they receive: JSON notifications, or the event commands of a saved TCP server, exactly as that server receives them. Clients without an enabled profile for their address use the default profile (address *), which cannot be deleted.")
        }
        Repeater {
            model: root.manager.incomingSessions
            delegate: Label {
                required property var modelData
                Layout.fillWidth: true; wrapMode: Text.WordWrap
                text: modelData.address + ":" + modelData.port + " — " + modelData.profileName + " — " +
                    (modelData.topics.length ? qsTr("subscribed to %1").arg(modelData.topics.join(", ")) : qsTr("not subscribed"))
            }
        }
        RowLayout {
            ComboBox {
                Layout.fillWidth: true
                model: root.manager.incomingClients; textRole: "name"; valueRole: "id"
                currentIndex: root.findIndex(root.manager.incomingClients, root.incomingId)
                onActivated: root.editIncoming(root.manager.incomingClients[index])
            }
            Button { text: qsTr("New client profile"); onClicked: root.editIncoming(null) }
        }
        GridLayout {
            columns: 2; Layout.fillWidth: true
            Label { text: qsTr("Name:") }
            TextField { id: incomingName; Layout.fillWidth: true }
            Label { text: qsTr("Client address:") }
            TextField {
                id: incomingAddress; Layout.fillWidth: true; readOnly: root.incomingId === "*"
                placeholderText: qsTr("IP address of the client")
            }
            Label { text: qsTr("Enabled:") }
            CheckBox {
                id: incomingEnabled; enabled: root.incomingId !== "*"
                text: root.incomingId === "*" ? qsTr("The default profile is always used for clients without their own profile") : ""
            }
            Label { text: qsTr("Subscribe on connect:"); Layout.alignment: Qt.AlignTop }
            ColumnLayout {
                Layout.fillWidth: true
                RowLayout {
                    CheckBox {
                        id: incomingSubscribe
                        text: qsTr("Subscribe to the checked topics when the client connects")
                    }
                    Button {
                        text: qsTr("Check all"); enabled: incomingSubscribe.checked
                        onClicked: root.incomingExcluded = []
                    }
                    Button {
                        text: qsTr("Uncheck all"); enabled: incomingSubscribe.checked
                        onClicked: root.incomingExcluded = root.manager.eventTopics.slice()
                    }
                }
                Flow {
                    Layout.fillWidth: true; spacing: 4; enabled: incomingSubscribe.checked
                    Repeater {
                        model: root.manager.eventTopics
                        delegate: CheckBox {
                            required property string modelData
                            text: modelData
                            checked: root.incomingExcluded.indexOf(modelData) < 0
                            onToggled: {
                                root.setIncomingTopic(modelData, checked);
                                // Keep following the profile when another profile is selected.
                                checked = Qt.binding(function() { return root.incomingExcluded.indexOf(modelData) < 0; });
                            }
                        }
                    }
                }
                Label {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap; opacity: 0.7
                    text: incomingSubscribe.checked
                        ? qsTr("Unchecked topics are left out; topics added in later versions are included. Saving updates connected clients.")
                        : qsTr("Clients wait for /subscribe.")
                }
            }
            Label { text: qsTr("Update on connect:") }
            CheckBox {
                id: incomingInitialUpdate; enabled: incomingSubscribe.checked
                text: qsTr("Send the current values when the client connects (turn off for clients that reconnect often; they request /full_update)")
            }
            Label { text: qsTr("Update format:") }
            ComboBox { id: incomingFormat; Layout.fillWidth: true; model: [qsTr("JSON notifications"), qsTr("Event commands of a TCP server")] }
            Label { text: qsTr("Command structure from:") }
            ComboBox {
                Layout.fillWidth: true; enabled: incomingFormat.currentIndex === 1
                model: root.manager.servers; textRole: "name"; valueRole: "id"
                currentIndex: root.findIndex(root.manager.servers, root.incomingCommandServerId)
                onActivated: root.incomingCommandServerId = currentValue
            }
            Label { text: qsTr("Send terminator (hex):") }
            TextField { id: incomingSendHex; Layout.fillWidth: true; placeholderText: qsTr("0a = LF, 0d = CR") }
            Label { text: qsTr("Replies:") }
            CheckBox {
                id: incomingReplies
                text: qsTr("Reply to every action with a JSON result (turn off for clients that only need the updates)")
            }
        }
        RowLayout {
            Button { text: qsTr("Save client profile"); onClicked: root.saveIncoming() }
            Button {
                text: qsTr("Delete client profile"); enabled: root.incomingId !== "" && root.incomingId !== "*"
                onClicked: if (root.manager.removeIncomingClient(root.incomingId)) root.editIncoming(null)
            }
        }
        Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: root.manager.lastError }
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
            DeferredSpinBox { id: port; from: 1; to: 65535; editable: true }
            Label { text: qsTr("Enabled:") }
            CheckBox { id: enabled }
            Label { text: qsTr("Connect on startup:") }
            CheckBox {
                id: connectOnStartup; enabled: enabled.checked
                text: qsTr("Otherwise connect manually, via REST or via TCP control")
            }
            Label { text: qsTr("Receive C-Play control commands:") }
            CheckBox { id: acceptControl; text: qsTr("Parse incoming JSON commands") }
            Label { text: qsTr("Send change notifications:") }
            TextField {
                id: serverEventTopics; Layout.fillWidth: true
                placeholderText: qsTr("Comma-separated topics; empty disables notifications")
            }
            Label { text: qsTr("Automatic reconnect:") }
            CheckBox { id: reconnect }
            Label { text: qsTr("Framing:") }
            ComboBox { id: framing; model: [qsTr("Delimiter"), qsTr("32-bit big-endian length")] }
            Label { text: qsTr("Receive terminator (hex):") }
            TextField { id: receiveHex; Layout.fillWidth: true; enabled: framing.currentIndex === 0; placeholderText: "0a = LF, 0d0a = CRLF, 00 = NUL" }
            Label { text: qsTr("Send terminator (hex):") }
            TextField { id: sendHex; Layout.fillWidth: true; enabled: framing.currentIndex === 0 }
            Label { text: qsTr("Connection timeout (ms):") }
            DeferredSpinBox { id: timeout; from: 1; to: 600000; editable: true }
            Label { text: qsTr("Initial retry delay (ms):") }
            DeferredSpinBox { id: retryInitial; from: 1; to: 600000; editable: true }
            Label { text: qsTr("Maximum retry delay (ms):") }
            DeferredSpinBox { id: retryMaximum; from: 1; to: 600000; editable: true }
            Label { text: qsTr("Reset backoff after (ms):") }
            DeferredSpinBox { id: stable; from: 1; to: 600000; editable: true }
            Label { text: qsTr("Maximum message (bytes):") }
            DeferredSpinBox { id: messageLimit; from: 1; to: 67108864; editable: true }
            Label { text: qsTr("Maximum pending writes (bytes):") }
            DeferredSpinBox { id: writeLimit; from: 1; to: 67108864; editable: true }
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
            TextField { id: eventModelPath; Layout.fillWidth: true; text: "./data/EventModel.json"; placeholderText: qsTr("EventModel JSON path") }
            Button {
                text: qsTr("Import EventModel"); enabled: root.serverId !== ""
                onClicked: root.manager.importEventModel(root.serverId, eventModelPath.text)
            }
        }
        Label {
            Layout.fillWidth: true; wrapMode: Text.WordWrap
            text: qsTr("Import adds the EventModel's event commands to the selected saved server.")
        }
        Label {
            Layout.fillWidth: true; wrapMode: Text.WordWrap
            text: qsTr("Notification and command trigger topics (use * for all): ") + root.manager.eventTopics.join(", ")
        }
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
            Label { text: qsTr("Trigger on changes:") }
            TextField {
                id: commandEventTopics; Layout.fillWidth: true
                placeholderText: qsTr("e.g. triggered_slide, layers, playlist; empty = manual")
            }
            Label { text: qsTr("Event payload:") }
            ComboBox { id: eventPayload; model: [qsTr("Fixed payload"), qsTr("Append current value")] }
            Label { text: qsTr("Characters per list item:") }
            DeferredSpinBox { id: charsPerItem; from: 0; to: 4096; editable: true }
            Label { text: qsTr("Time format:") }
            TextField {
                id: timeFormat; Layout.fillWidth: true
                placeholderText: qsTr("Empty = seconds; e.g. hh:mm:ss or hh:mm:ss/zz (position, remaining, duration)")
            }
        }
        Label {
            Layout.fillWidth: true; wrapMode: Text.WordWrap
            text: qsTr("Use 0 to send full lists. A positive width pads or shortens each list label to that many characters.")
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
                        payload: payload.text, raw: raw.checked, eventTopics: root.topics(commandEventTopics.text),
                        eventPayload: ["fixed", "value"][eventPayload.currentIndex], charsPerItem: charsPerItem.value,
                        timeFormat: timeFormat.text.trim()});
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
