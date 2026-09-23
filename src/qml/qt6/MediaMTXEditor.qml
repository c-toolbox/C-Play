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

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

Kirigami.ApplicationWindow {
    id: root

    color: Kirigami.Theme.alternateBackgroundColor
    height: 600
    title: qsTr("MediaMTX Streams")
    visible: false
    width: 800

    property int selectedServerIndex: -1
    property int selectedStreamIndex: -1
    // True when the Windows Credential Manager holds a credential for the currently shown
    // server name + API user ("MediaMTX/<name>/<username>").
    property bool credentialFound: false

    // MediaMTX support is a build option (BUILD_CPLAY_WITH_MEDIA_MTX); in builds without it the
    // models do not exist on app and this window can never be opened. The guards keep the
    // bindings below safe while main.qml still instantiates this component.
    property bool mediaMtxAvailable: !!app.mediaMtxServersModel && !!app.mediaMtxModel

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
            app.mediaMtxServersModel.updateServersList();
            root.selectedStreamIndex = -1;
            app.mediaMtxModel.clear();
            if (app.mediaMtxServersModel.numberOfServers > 0) {
                root.selectedServerIndex = 0;
                serversList.currentIndex = 0;
                root.loadServer(0);
            } else {
                root.selectedServerIndex = -1;
                root.clearServerFields();
            }
        }
    }

    Connections {
        target: app.mediaMtxModel
        function onResponseChanged() {
            if (app.mediaMtxModel.lastError !== "") {
                statusLabel.text = qsTr("Error: ") + app.mediaMtxModel.lastError;
                statusLabel.color = "red";
            } else if (app.mediaMtxModel.configWarning !== "") {
                statusLabel.text = app.mediaMtxModel.lastSummary + " - "
                    + qsTr("server config unavailable (%1), WebRTC/RTSP auto-detection disabled").arg(app.mediaMtxModel.configWarning);
                statusLabel.color = "orange";
            } else {
                statusLabel.text = app.mediaMtxModel.lastSummary;
                statusLabel.color = "green";
            }
        }
        function onStreamsListChanged() {
            root.selectedStreamIndex = -1;
            streamTitleField.text = "";
            // The RTSP port/scheme may have been auto-detected from the server config.
            root.loadServer(root.selectedServerIndex);
        }
    }

    function clearServerFields() {
        serverName.text = "";
        serverHost.text = "";
        serverApiPort.text = "9997";
        serverApiScheme.currentIndex = 0;
        serverUsername.text = "";
        serverPassword.text = "";
        serverManualPassword.checked = false;
        root.credentialFound = false;
        serverRtspPort.text = "8554";
        serverRtspScheme.currentIndex = 0;
        serverRtspTransport.currentIndex = 0;
        serverWebrtcPort.text = "8889";
        serverWebrtcScheme.currentIndex = 0;
        serverSrtPort.text = "8890";
        serverAutoDetectRtsp.checked = true;
        serverAutoDetectWebRtc.checked = true;
        serverAutoDetectSrt.checked = true;
        serverEnabled.checked = true;
        includeCredentialsCheckBox.checked = false;
    }

    function loadServer(index) {
        if (index < 0 || index >= app.mediaMtxServersModel.numberOfServers)
            return;
        var s = app.mediaMtxServersModel.serverAt(index);
        serverName.text = s.name;
        serverHost.text = s.host;
        serverApiPort.text = s.apiPort;
        serverApiScheme.currentIndex = s.apiScheme === "https" ? 1 : 0;
        serverUsername.text = s.username;
        serverRtspPort.text = s.rtspPort;
        serverRtspScheme.currentIndex = s.rtspScheme === "rtsps" ? 1 : 0;
        serverRtspTransport.currentIndex = s.rtspTransport === "udp" ? 1 : 0;
        serverWebrtcPort.text = s.webrtcPort;
        serverWebrtcScheme.currentIndex = s.webrtcScheme === "https" ? 1 : 0;
        serverSrtPort.text = s.srtPort;
        serverAutoDetectRtsp.checked = s.autoDetectRtsp;
        serverAutoDetectWebRtc.checked = s.autoDetectWebRtc;
        serverAutoDetectSrt.checked = s.autoDetectSrt;
        serverEnabled.checked = s.enabled;
        // Default to embedding credentials in the RTSP/SRT stream URL when this connection has
        // authentication configured, so an added Stream layer can actually connect.
        includeCredentialsCheckBox.checked = s.username !== "";
    }

    function applyPassword() {
        if (root.selectedServerIndex >= 0)
            app.mediaMtxServersModel.setPassword(root.selectedServerIndex, serverPassword.text);
    }

    // Re-checks the Windows Credential Manager against the current name/user fields and
    // decides whether a stored or a manually entered password is used.
    function refreshCredentialStatus() {
        root.credentialFound = root.selectedServerIndex >= 0 &&
            app.mediaMtxServersModel.hasStoredCredential(serverName.text, serverUsername.text);
        // When falling back to the stored credential, drop any session password so it does
        // not shadow the one from the Credential Manager.
        if (root.credentialFound && !serverManualPassword.checked) {
            serverPassword.text = "";
            root.applyPassword();
        }
    }

    // Short per-server auth state for the list delegate: a stored credential counts as
    // "auth set" even when no session password has been entered yet.
    function serverAuthLabel(index) {
        var s = app.mediaMtxServersModel.serverAt(index);
        if (s.hasPassword)
            return qsTr("auth set");
        if (s.username !== "" && app.mediaMtxServersModel.hasStoredCredential(s.name, s.username))
            return qsTr("credential manager");
        return s.username !== "" ? qsTr("no password") : "";
    }

    function streamLayerType() {
        var m = app.slides.selected.layersTypeModel;
        for (var i = 0; i < m.rowCount(); i++) {
            if (m.data(m.index(i, 0), Qt.UserRole) === "Stream")
                return i + 1;
        }
        return -1;
    }

    function webRtcLayerType() {
        var m = app.slides.selected.layersTypeModel;
        for (var i = 0; i < m.rowCount(); i++) {
            if (m.data(m.index(i, 0), Qt.UserRole) === "WebRTC")
                return i + 1;
        }
        return -1;
    }

    // A WebRTC layer can only be created when this build has the layer and the server has WebRTC enabled.
    function webRtcLayerAvailable() {
        if (!root.mediaMtxAvailable)
            return false;
        return app.mediaMtxModel.webRtcAvailable && root.webRtcLayerType() >= 0;
    }

    // SRT is played by an mpv-based Stream layer, so it needs a Stream layer in this build
    // and "srt: yes" on the server.
    function srtStreamAvailable() {
        if (!root.mediaMtxAvailable)
            return false;
        return app.mediaMtxModel.srtAvailable && root.streamLayerType() >= 0;
    }

    // Protocol options for the stream combo box. Note: do NOT concatenate arrays with `+` -
    // in JavaScript, array + array is string concatenation ("RTSP" + "WebRTC").
    function protocolOptions() {
        var opts = ["RTSP"];
        if (root.webRtcLayerAvailable())
            opts.push("WebRTC");
        if (root.srtStreamAvailable())
            opts.push("SRT");
        return opts;
    }

    // Selects the output protocol that best matches how the stream is published on the server.
    // Online streams report a plain source type ("rtsp", "srt", "webrtc", ...); configured-only
    // paths report their configured source, which may be a URL such as "srt://host:port/path".
    // Falls back to RTSP when there is no better match or the matching protocol is unavailable.
    function selectBestProtocol(sourceType) {
        var st = String(sourceType || "").toLowerCase();
        if (st.indexOf("webrtc") >= 0 && root.webRtcLayerAvailable()) {
            streamProtocolCombo.currentIndex = protocolOptions().indexOf("WebRTC");
            return;
        }
        if ((st === "srt" || st.startsWith("srt:")) && root.srtStreamAvailable()) {
            streamProtocolCombo.currentIndex = protocolOptions().indexOf("SRT");
            return;
        }
        streamProtocolCombo.currentIndex = 0; // RTSP is always the first option
    }

    // The playback URL for the selected stream with the currently selected protocol.
    function selectedStreamUrl(includeCredentials) {
        var proto = streamProtocolCombo.currentText;
        if (proto === "WebRTC")
            return app.mediaMtxModel.whepUrlAt(root.selectedStreamIndex, includeCredentials);
        if (proto === "SRT")
            return app.mediaMtxModel.srtUrlAt(root.selectedStreamIndex);
        return app.mediaMtxModel.rtspUrlAt(root.selectedStreamIndex, includeCredentials);
    }

    // Explains which protocols are offered and why the others are not.
    function protocolTooltip() {
        if (!root.mediaMtxAvailable)
            return "";
        if (app.mediaMtxModel.configWarning !== "") {
            return qsTr("The server configuration could not be read (%1), so it is unknown whether WebRTC/SRT is enabled").arg(app.mediaMtxModel.configWarning);
        }
        var missing = [];
        if (!app.mediaMtxModel.webRtcAvailable)
            missing.push(qsTr("the server does not have WebRTC enabled ('webrtc: yes' in mediamtx.yml)"));
        else if (root.webRtcLayerType() < 0)
            missing.push(qsTr("this build has no WebRTC support"));
        if (!app.mediaMtxModel.srtAvailable)
            missing.push(qsTr("the server does not have SRT enabled ('srt: yes' in mediamtx.yml)"));
        else if (root.streamLayerType() < 0)
            missing.push(qsTr("this build has no Stream layer support"));
        if (missing.length === 0)
            return qsTr("RTSP and SRT create a Stream layer, WebRTC creates a WebRTC layer using the WHEP endpoint.");
        return qsTr("Only RTSP is available: %1").arg(missing.join("; "));
    }

    function streamModelValue(index, role) {
        var m = app.mediaMtxModel;
        return m.data(m.index(index, 0), role);
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
                text: qsTr("MediaMTX Servers")
            }
            Rectangle {
                Layout.fillWidth: true
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            Layout.minimumHeight: 30
            Layout.maximumHeight: 80
            spacing: 8

            ColumnLayout {
                Layout.alignment: Qt.AlignTop
                spacing: 4

                ToolButton {
                    icon.name: "go-up"
                    enabled: root.selectedServerIndex > 0
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Move up")
                    onClicked: {
                        app.mediaMtxServersModel.moveServer(root.selectedServerIndex, root.selectedServerIndex - 1);
                        root.selectedServerIndex = root.selectedServerIndex - 1;
                        serversList.currentIndex = root.selectedServerIndex;
                        root.loadServer(root.selectedServerIndex);
                    }
                }
                ToolButton {
                    icon.name: "go-down"
                    enabled: root.selectedServerIndex >= 0 && root.selectedServerIndex < app.mediaMtxServersModel.numberOfServers - 1
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Move down")
                    onClicked: {
                        app.mediaMtxServersModel.moveServer(root.selectedServerIndex, root.selectedServerIndex + 1);
                        root.selectedServerIndex = root.selectedServerIndex + 1;
                        serversList.currentIndex = root.selectedServerIndex;
                        root.loadServer(root.selectedServerIndex);
                    }
                }
            }

            ListView {
                id: serversList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: app.mediaMtxServersModel
                currentIndex: root.selectedServerIndex

                delegate: ItemDelegate {
                    width: serversList.width
                    highlighted: index === root.selectedServerIndex
                    contentItem: RowLayout {
                        Label {
                            Layout.fillWidth: true
                            text: model.name
                            elide: Text.ElideRight
                            color: model.enabled ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                        }
                        Label {
                            text: model.apiScheme + "://" + model.host + ":" + model.apiPort
                            color: Kirigami.Theme.disabledTextColor
                        }
                        Label {
                            text: root.serverAuthLabel(index)
                            color: Kirigami.Theme.disabledTextColor
                        }
                    }
                    onClicked: {
                        root.selectedServerIndex = index;
                        serverPassword.text = "";
                        root.loadServer(index);
                    }
                }

                ScrollBar.vertical: ScrollBar {}
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 4
            columnSpacing: 8
            rowSpacing: 6

            Label { text: qsTr("Name:"); Layout.alignment: Qt.AlignRight }
            TextField {
                id: serverName
                Layout.fillWidth: true
                placeholderText: qsTr("Server name")
                onTextChanged: root.refreshCredentialStatus()
            }
            Label { text: qsTr("Host:"); Layout.alignment: Qt.AlignRight }
            TextField {
                id: serverHost
                Layout.fillWidth: true
                placeholderText: "127.0.0.1"
            }

            Label { text: qsTr("API:"); Layout.alignment: Qt.AlignRight }
            RowLayout {
                Layout.fillWidth: true
                ComboBox {
                    id: serverApiScheme
                    model: ["http", "https"]
                    currentIndex: 0
                }
                TextField {
                    id: serverApiPort
                    Layout.fillWidth: true
                    text: "9997"
                    validator: IntValidator { bottom: 1; top: 65535 }
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Control API port, requires 'api: yes' in mediamtx.yml")
                }
            }
            Label { text: qsTr("RTSP:"); Layout.alignment: Qt.AlignRight }
            RowLayout {
                Layout.fillWidth: true
                ComboBox {
                    id: serverRtspScheme
                    model: ["rtsp", "rtsps"]
                    currentIndex: 0
                }
                TextField {
                    id: serverRtspPort
                    Layout.fillWidth: true
                    text: "8554"
                    validator: IntValidator { bottom: 1; top: 65535 }
                }
                ComboBox {
                    id: serverRtspTransport
                    model: ["tcp", "udp"]
                    currentIndex: 0
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Transport used by the stream layer (tcp is recommended)")
                }
            }

            Label { text: qsTr("WebRTC:"); Layout.alignment: Qt.AlignRight }
            RowLayout {
                Layout.fillWidth: true
                ComboBox {
                    id: serverWebrtcScheme
                    model: ["http", "https"]
                    currentIndex: 0
                }
                TextField {
                    id: serverWebrtcPort
                    Layout.fillWidth: true
                    text: "8889"
                    validator: IntValidator { bottom: 1; top: 65535 }
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("WHEP/WebRTC port, requires 'webrtc: yes' in mediamtx.yml")
                }
            }

            Label { text: qsTr("SRT:"); Layout.alignment: Qt.AlignRight }
            RowLayout {
                Layout.fillWidth: true
                TextField {
                    id: serverSrtPort
                    Layout.fillWidth: true
                    text: "8890"
                    validator: IntValidator { bottom: 1; top: 65535 }
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("SRT port, requires 'srt: yes' in mediamtx.yml")
                }
            }

            Label { text: qsTr("Username:"); Layout.alignment: Qt.AlignRight }
            TextField {
                id: serverUsername
                Layout.fillWidth: true
                placeholderText: qsTr("Optional API user")
                onTextChanged: root.refreshCredentialStatus()
            }
            Label { text: qsTr("Password:"); Layout.alignment: Qt.AlignRight }
            RowLayout {
                Layout.fillWidth: true
                TextField {
                    id: serverPassword
                    Layout.fillWidth: true
                    echoMode: TextInput.Password
                    placeholderText: root.credentialFound && !serverManualPassword.checked
                        ? qsTr("Using the Windows Credential Manager")
                        : qsTr("Session only, never saved")
                    enabled: serverManualPassword.checked || !root.credentialFound
                    onEditingFinished: {
                        if (enabled)
                            root.applyPassword();
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: root.credentialFound && !serverManualPassword.checked
                        ? qsTr("The password is read from the Windows Credential Manager entry \"MediaMTX/<name>/<username>\".")
                        : qsTr("The password is kept in memory for this session only and is never written to disk.")
                }
                CheckBox {
                    id: serverManualPassword
                    text: qsTr("Enter manually")
                    checked: false
                    onToggled: root.refreshCredentialStatus()
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Type a password here instead of using the one from the Windows Credential Manager.")
                }
            }

            Label { text: ""; Layout.alignment: Qt.AlignRight }
            CheckBox {
                id: serverAutoDetectRtsp
                text: qsTr("Auto-detect RTSP port")
                checked: true
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Reads rtspAddress/rtspsAddress from the server configuration when fetching streams.")
            }
            Label { text: ""; Layout.alignment: Qt.AlignRight }
            CheckBox {
                id: serverAutoDetectWebRtc
                text: qsTr("Auto-detect WebRTC port")
                checked: true
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Reads webrtcAddress/webrtcsAddress from the server configuration when fetching streams.")
            }
            Label { text: ""; Layout.alignment: Qt.AlignRight }
            CheckBox {
                id: serverAutoDetectSrt
                text: qsTr("Auto-detect SRT port")
                checked: true
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Reads srtAddress from the server configuration when fetching streams.")
            }
            Label { text: ""; Layout.alignment: Qt.AlignRight }
            CheckBox {
                id: serverEnabled
                text: qsTr("Enabled")
                checked: true
            }
        }

        // Explicitly reports whether a stored credential was found, so the user can always
        // see the outcome of the Windows Credential Manager check.
        RowLayout {
            Layout.fillWidth: true
            visible: root.selectedServerIndex >= 0 && serverUsername.text !== ""
            spacing: 6

            Kirigami.Icon {
                source: root.credentialFound ? "dialog-ok-true" : "dialog-information"
                width: Kirigami.Units.iconSizes.small
                height: Kirigami.Units.iconSizes.small
                color: root.credentialFound ? "green" : Kirigami.Theme.textColor
            }

            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: {
                    if (root.credentialFound) {
                        return serverManualPassword.checked
                            ? qsTr("Credential found in the Windows Credential Manager — the manual password takes precedence.")
                            : qsTr("Credential found in the Windows Credential Manager — using the stored password.");
                    }
                    return qsTr("No credential found for \"%1\" in the Windows Credential Manager.").arg(
                        "MediaMTX/" + serverName.text + "/" + serverUsername.text);
                }
                color: root.credentialFound ? "green" : Kirigami.Theme.textColor
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                text: qsTr("Add New")
                icon.name: "list-add"
                enabled: serverName.text !== "" && serverHost.text !== ""
                onClicked: {
                    app.mediaMtxServersModel.addServer(serverName.text, serverHost.text, parseInt(serverApiPort.text), serverApiScheme.currentText, serverUsername.text, parseInt(serverRtspPort.text), serverRtspScheme.currentText, serverRtspTransport.currentText, parseInt(serverWebrtcPort.text), serverWebrtcScheme.currentText, parseInt(serverSrtPort.text), serverAutoDetectRtsp.checked, serverAutoDetectWebRtc.checked, serverAutoDetectSrt.checked, serverEnabled.checked);
                    root.selectedServerIndex = app.mediaMtxServersModel.numberOfServers - 1;
                    serversList.currentIndex = root.selectedServerIndex;
                    root.applyPassword();
                }
            }
            Button {
                text: qsTr("Update")
                icon.name: "document-save"
                enabled: root.selectedServerIndex >= 0 && serverName.text !== "" && serverHost.text !== ""
                onClicked: {
                    app.mediaMtxServersModel.updateServer(root.selectedServerIndex, serverName.text, serverHost.text, parseInt(serverApiPort.text), serverApiScheme.currentText, serverUsername.text, parseInt(serverRtspPort.text), serverRtspScheme.currentText, serverRtspTransport.currentText, parseInt(serverWebrtcPort.text), serverWebrtcScheme.currentText, parseInt(serverSrtPort.text), serverAutoDetectRtsp.checked, serverAutoDetectWebRtc.checked, serverAutoDetectSrt.checked, serverEnabled.checked);
                    root.applyPassword();
                }
            }
            Button {
                text: qsTr("Remove")
                icon.name: "list-remove"
                enabled: root.selectedServerIndex >= 0
                onClicked: {
                    app.mediaMtxServersModel.removeServer(root.selectedServerIndex);
                    root.selectedServerIndex = -1;
                    root.clearServerFields();
                    app.mediaMtxModel.clear();
                }
            }
            Item { Layout.fillWidth: true }
            Button {
                text: qsTr("Test Connection")
                icon.name: "network-connect"
                enabled: root.selectedServerIndex >= 0 && !app.mediaMtxModel.refreshInProgress
                onClicked: {
                    root.applyPassword();
                    statusLabel.text = qsTr("Connecting...");
                    statusLabel.color = Kirigami.Theme.disabledTextColor;
                    app.mediaMtxModel.testConnection(root.selectedServerIndex);
                }
            }
            Button {
                text: root.mediaMtxAvailable && app.mediaMtxModel.refreshInProgress ? qsTr("Fetching...") : qsTr("Fetch Streams")
                icon.name: "view-refresh"
                enabled: root.selectedServerIndex >= 0 && !app.mediaMtxModel.refreshInProgress
                onClicked: {
                    root.applyPassword();
                    statusLabel.text = qsTr("Fetching...");
                    statusLabel.color = Kirigami.Theme.disabledTextColor;
                    app.mediaMtxModel.refresh(root.selectedServerIndex);
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Rectangle {
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
                width: Kirigami.Units.gridUnit
            }
            Kirigami.Heading {
                level: 3
                text: qsTr("Available Streams")
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

        ListView {
            id: streamsList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 70
            clip: true
            model: app.mediaMtxModel
            currentIndex: root.selectedStreamIndex

            delegate: ItemDelegate {
                width: streamsList.width
                highlighted: index === root.selectedStreamIndex
                contentItem: RowLayout {
                    Rectangle {
                        width: 10
                        height: 10
                        radius: 5
                        color: model.online ? "green" : Kirigami.Theme.disabledTextColor
                    }
                    Label {
                        Layout.preferredWidth: 160
                        text: model.name
                        elide: Text.ElideMiddle
                    }
                    Label {
                        Layout.preferredWidth: 100
                        text: model.configuredOnly ? qsTr("configured") : model.sourceType
                        color: Kirigami.Theme.disabledTextColor
                        elide: Text.ElideRight
                    }
                    Label {
                        Layout.preferredWidth: 120
                        text: model.tracks
                        color: Kirigami.Theme.disabledTextColor
                        elide: Text.ElideRight
                    }
                    Label {
                        text: model.readers + qsTr(" readers")
                        color: Kirigami.Theme.disabledTextColor
                    }
                    Label {
                        Layout.fillWidth: true
                        text: streamProtocolCombo.currentText === "WebRTC" ? model.whepUrl
                             : (streamProtocolCombo.currentText === "SRT" ? model.srtUrl : model.rtspUrl)
                        color: Kirigami.Theme.disabledTextColor
                        elide: Text.ElideMiddle
                    }
                }
                onClicked: {
                    root.selectedStreamIndex = index;
                    streamTitleField.text = model.serverName !== "" ? model.serverName + "/" + model.name : model.name;
                    // Match the output protocol to how this stream is published on the server.
                    root.selectBestProtocol(model.sourceType);
                }
            }

            Kirigami.PlaceholderMessage {
                anchors.centerIn: parent
                visible: root.mediaMtxAvailable && streamsList.count === 0 && !app.mediaMtxModel.refreshInProgress
                icon.name: "network-connect"
                icon.height: Kirigami.Units.iconSizes.medium
                text: qsTr("No streams fetched yet")
                explanation: qsTr("Select a server and press Fetch Streams.")
            }

            ScrollBar.vertical: ScrollBar {}
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label { text: qsTr("Title:") }
            TextField {
                id: streamTitleField
                Layout.fillWidth: true
                placeholderText: qsTr("Title used for the layer or predefined stream entry")
                enabled: root.selectedStreamIndex >= 0
            }
            Label { text: qsTr("Protocol:") }
            ComboBox {
                id: streamProtocolCombo
                // protocolOptions() returns a fresh array; do NOT build the model with `+`:
                // in JavaScript, array + array is string concatenation ("RTSP" + "WebRTC"),
                // which makes the ComboBox iterate the model one character per line.
                model: root.protocolOptions()
                currentIndex: 0
                onModelChanged: {
                    // Fall back to RTSP when the selected protocol is no longer offered, e.g.
                    // after fetching a server that does not have it enabled.
                    if (!model.includes(currentText))
                        currentIndex = 0;
                }
                ToolTip.visible: hovered
                ToolTip.text: root.protocolTooltip()
            }
            CheckBox {
                id: includeCredentialsCheckBox
                text: qsTr("Include credentials in URL")
                checked: false
                // SRT URLs cannot carry user:password - MediaMTX authenticates SRT readers by IP allowlist or passphrase.
                // WebRTC carries auth as an HTTP Authorization header on the layer, not embedded in the WHEP URL.
                enabled: streamProtocolCombo.currentText !== "SRT" && streamProtocolCombo.currentText !== "WebRTC"
                ToolTip.visible: hovered
                ToolTip.text: streamProtocolCombo.currentText === "SRT"
                    ? qsTr("Not applicable to SRT: MediaMTX authenticates SRT readers by IP allowlist or passphrase, not user:password.")
                    : streamProtocolCombo.currentText === "WebRTC"
                        ? qsTr("Not needed for WebRTC: authentication is added automatically as an HTTP Authorization header on the layer.")
                        : qsTr("Adds user:password to the stream URL (RTSP). Saving this to the predefined stream list writes the password to disk in clear text.")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: root.selectedStreamIndex >= 0 ? root.selectedStreamUrl(false) : ""
                elide: Text.ElideMiddle
                color: Kirigami.Theme.disabledTextColor
            }
            Button {
                text: qsTr("Copy Address")
                icon.name: "edit-copy"
                enabled: root.selectedStreamIndex >= 0
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Copies the stream address for the selected protocol to the clipboard.")
                onClicked: {
                    app.copyToClipboard(root.selectedStreamUrl(includeCredentialsCheckBox.checked));
                    statusLabel.text = qsTr("Address copied to clipboard");
                    statusLabel.color = "green";
                }
            }
            Button {
                text: qsTr("Save To Predefined Streams")
                icon.name: "document-save-as"
                // WHEP URLs are ephemeral, so only RTSP and SRT can be saved to the predefined list.
                enabled: root.selectedStreamIndex >= 0 && streamTitleField.text !== ""
                         && streamProtocolCombo.currentText !== "WebRTC"
                ToolTip.visible: hovered && streamProtocolCombo.currentText === "WebRTC"
                ToolTip.text: qsTr("Only RTSP and SRT streams can be saved to the predefined stream list")
                onClicked: {
                    if (app.mediaMtxModel.addToPredefinedStreams(streamTitleField.text, root.selectedStreamUrl(includeCredentialsCheckBox.checked))) {
                        app.streamsModel.updateStreamsList();
                        statusLabel.text = qsTr("Saved to predefined streams");
                        statusLabel.color = "green";
                    } else {
                        statusLabel.text = qsTr("Could not save to predefined streams");
                        statusLabel.color = "red";
                    }
                }
            }
            Button {
                text: qsTr("Add As Layer")
                icon.name: "layer-new"
                enabled: root.selectedStreamIndex >= 0 && streamTitleField.text !== "" && app.slides.selected.layersEnabled
                onClicked: {
                    var isWebRtc = streamProtocolCombo.currentText === "WebRTC";
                    var layerType = isWebRtc ? root.webRtcLayerType() : root.streamLayerType();
                    if (layerType < 0) {
                        statusLabel.text = isWebRtc
                            ? qsTr("WebRTC layers are not available in this build")
                            : qsTr("Stream layers are not available in this build");
                        statusLabel.color = "red";
                        return;
                    }
                    // WebRTC carries auth as an HTTP Authorization header stored on the layer, so it
                    // keeps a clean WHEP URL (no user:password). RTSP/SRT embed credentials in the
                    // stream URL when the checkbox is set.
                    var url = isWebRtc ? root.selectedStreamUrl(false) : root.selectedStreamUrl(includeCredentialsCheckBox.checked);
                    // Use the presentation's default stereo and grid mode for the new layer, like drag-and-drop does.
                    layerView.layerItem.layerIdx = app.slides.selected.addLayer(streamTitleField.text, layerType, url,
                        PresentationSettings.defaultStereoModeForLayers, PresentationSettings.defaultGridModeForLayers);
                    if (isWebRtc && layerView.layerItem.layerIdx >= 0) {
                        // Attach the server's HTTP Basic auth as an Authorization header on the WebRTC layer.
                        var creds = app.mediaMtxServersModel.authCredentials(root.selectedServerIndex);
                        if (creds.username !== "") {
                            layerView.layerItem.layerWhepAuthUsername = creds.username;
                            layerView.layerItem.layerWhepAuthPassword = creds.password;
                        }
                    }
                    app.slides.updateSelectedSlide();
                    statusLabel.text = qsTr("Layer added to the selected slide");
                    statusLabel.color = "green";
                }
            }
        }
    }
}
