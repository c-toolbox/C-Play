/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
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
    title: qsTr("C-Lux Editor")
    visible: false
    width: 950

    readonly property var client: app.cluxClient

    // Live-mode operator commands (blackout, half light) only work while connected and in
    // live mode; preview mode is read-only with respect to the server, so they stay
    // disabled there. Scene and pattern changes are locally editable in both modes: in
    // preview mode they update the local state and queue up for replay when live mode is
    // entered - which works even without a connection (against the cached state) - while
    // live mode itself requires one, so its switch stays disabled there.
    readonly property bool canControl: client.connected && client.liveMode
    // Scene/pattern editing is local in preview mode (works offline) and sent at once in
    // live mode, so it is only disabled in the impossible combination of live mode without
    // a connection; the client refuses those sends as well.
    readonly property bool canEditLook: !client.liveMode || client.connected

    // NDI capture on the server: it receives one NDI source and feeds a Video pattern from
    // it. The endpoints are open like the stream, so only a connection is needed - not live
    // mode (the preview blackout keeps the real output dark meanwhile). ndiAwaitingDefault
    // is set between the user turning the switch on and the refreshed source list arriving;
    // while it is set, the state sync must not treat that update as an external change.
    property bool ndiAwaitingDefault: false
    // Guards programmatic writes to the NDI switch so their toggled() signal does not run
    // the user-action handler (QML bindings on checked would break after the first click).
    property bool ndiSyncingSwitch: false

    // One-line summary of what the server's NDI receiver is doing right now.
    readonly property string ndiStatusText: {
        if (!root.client.ndiSupported)
            return root.client.ndiReason !== "" ? root.client.ndiReason
                                                : qsTr("NDI is not available on the server");
        if (root.client.ndiError !== "")
            return root.client.ndiError;
        if (root.client.ndiSource === "")
            return qsTr("No source assigned");
        var text = qsTr("Receiving %1").arg(root.client.ndiSource);
        if (!root.client.ndiRunning)
            text += " (" + qsTr("starting") + ")";
        else if (root.client.ndiConnections === 0)
            text += " (" + qsTr("no signal") + ")";
        return text;
    }
    readonly property bool ndiStatusIsError: !root.client.ndiSupported || root.client.ndiError !== ""

    Component.onCompleted: {
        // The server URL was already loaded from data/clux-server.json at startup.
        serverField.text = client.serverUrl;
    }
    onVisibleChanged: {
        if (visible) {
            // Center over the main application window. From this nested component "window" is
            // that outer window, not this one. The top edge is clamped so a dialog taller than
            // the main window never rises above its top corner.
            x = window.x + (window.width - width) / 2;
            y = Math.max(window.y, window.y + (window.height - height) / 2);
        }
    }

    // Shows the given message in the bottom bar (in red when isError), or clears it when
    // text is empty. Every client call starts by clearing it, so a stale result of an
    // earlier operation does not linger while the new one runs.
    function showStatus(text, isError) {
        statusLabel.text = text;
        statusLabel.color = isError ? "red" : Kirigami.Theme.textColor;
    }

    // Attach a C-Lux scene action to the currently selected slide (or master when none is
    // selected). The REST layer fires when the slide starts, so no new pattern or scene is
    // created on the server.
    function addSceneToSlide(sceneName, action) {
        var url = client.serverUrl + "/api/scenes/" + encodeURIComponent(sceneName) + "/" + action;
        var title = "C-Lux: " + sceneName + " (" + action + ")";
        var idx = app.slides.selected.addRestLayer(title, url, 1, "", false);
        if (idx >= 0) {
            showStatus(qsTr("Added REST layer to %1").arg(app.slides.getSlidesName()), false);
        } else {
            showStatus(qsTr("Failed to add the REST layer"), true);
        }
    }

    // The full name C-Play's own NDI output is known by ("<machine> (C-Play)"); empty while
    // it is not sending or when this build has no NDI support.
    function cplayNdiName() {
        return (ndiSender && ndiSender.available) ? ndiSender.ndiName : "";
    }

    // Picks the default source after a refresh that followed "turn on": C-Play's own output
    // first, then whatever the server already receives. The receiver is assigned in both
    // cases - for C-Play's own name even before discovery has listed it, since the server
    // opens its receiver for that name and picks the sender up when it appears on the net.
    function applyNdiDefault() {
        var names = [];
        for (var i = 0; i < root.client.ndiSources.length; ++i)
            names.push(root.client.ndiSources[i].name);

        var own = cplayNdiName();
        if (own !== "") {
            ndiCombo.currentIndex = names.indexOf(own);   // -1 until discovery lists it
            root.client.setNdiSource(own);
            return;
        }

        if (root.client.ndiSource !== "" && names.indexOf(root.client.ndiSource) >= 0) {
            ndiCombo.currentIndex = names.indexOf(root.client.ndiSource);
            return;   // already assigned on the server, nothing to send
        }

        showStatus(qsTr("No NDI sources found"), true);
        setNdiSwitch(false);
    }

    // Programmatic change of the NDI switch (server-state sync), with its toggled() signal
    // suppressed so it does not run as a user action.
    function setNdiSwitch(on) {
        root.ndiSyncingSwitch = true;
        ndiSwitch.checked = on;
        root.ndiSyncingSwitch = false;
    }

    Connections {
        target: root.client
        function onErrorOccurred(message) { root.showStatus(message, true); }

        // One consistent NDI update per refresh (see CLuxClient::refreshNdi): the source
        // list and the receiver status arrive together.
        function onNdiStateChanged() {
            if (root.ndiAwaitingDefault) {
                // The switch was just turned on and its discovery run has answered.
                root.ndiAwaitingDefault = false;
                if (!root.client.ndiSupported) {
                    showStatus(root.client.ndiReason !== "" ? root.client.ndiReason
                                                            : qsTr("NDI is not available on the server"), true);
                    root.setNdiSwitch(false);
                    return;
                }
                root.applyNdiDefault();
                return;
            }

            // Keep the switch and combo in step with what the server actually receives, so
            // changes made from another control surface show up like the rest of the state.
            var hasSource = root.client.ndiSource !== "";
            if (hasSource !== ndiSwitch.checked)
                root.setNdiSwitch(hasSource);

            var names = [];
            for (var i = 0; i < root.client.ndiSources.length; ++i)
                names.push(root.client.ndiSources[i].name);
            // -1 when the assigned source is not in the discovered list yet (C-Play's own
            // name before discovery has seen it, or none at all), so the combo never shows
            // a stale pick.
            var idx = names.indexOf(root.client.ndiSource);
            if (ndiCombo.currentIndex !== idx)
                ndiCombo.currentIndex = idx;

            // Keep the ring width slider in step with what the server samples, so changes
            // made from another control surface show up like the rest of the state; skipped
            // while the user has it pressed.
            if (!ringWidthSlider.pressed &&
                Math.abs(ringWidthSlider.value - root.client.ndiRingWidth) > 0.0005)
                ringWidthSlider.value = root.client.ndiRingWidth;
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        // Connection toolbar.
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Rectangle {
                width: 10
                height: 10
                radius: 5
                color: root.client.connected ? "#2ecc40" : Kirigami.Theme.disabledTextColor
            }
            Label {
                text: root.client.connected ? qsTr("Connected") : qsTr("Disconnected")
                font.bold: true
            }

            TextField {
                id: serverField
                Layout.preferredWidth: 280
                placeholderText: "http://localhost:8787"
                selectByMouse: true
            }
            Button {
                text: root.client.connected ? qsTr("Disconnect") : qsTr("Connect")
                icon.name: root.client.connected ? "network-offline" : "network-workgroup"
                onClicked: {
                    showStatus("");
                    var url = serverField.text.trim();
                    // Assigning serverUrl calls setServerUrl(), which persists it to data/clux-server.json.
                    root.client.serverUrl = url;
                    if (root.client.connected) {
                        root.client.disconnectFromServer();
                    } else {
                        root.client.connectToServer();
                    }
                }
            }

            Item { Layout.fillWidth: true }

            Label { text: qsTr("Live mode") }
            Switch {
                id: liveSwitch
                // Live mode drives the physical lights, so it needs a connection; preview
                // mode stays available offline (the client enforces this as well).
                enabled: root.client.connected
                checked: root.client.liveMode
                onToggled: {
                    showStatus("");
                    root.client.liveMode = checked;
                }
            }
            Label {
                text: root.client.liveMode ? qsTr("LIVE") : qsTr("PREVIEW")
                font.bold: true
                color: root.client.liveMode ? "red" : Kirigami.Theme.highlightColor
            }

            // Runtime-only toggle for the live light-color overlay in the 3D view (always off at
            // startup, not persisted). Drives app.cluxPreviewVisible, which main.qml binds to.
            Label { text: qsTr("Show in 3D view") }
            Switch {
                checked: app.cluxPreviewVisible
                onToggled: app.cluxPreviewVisible = checked
            }

            // Queued preview-mode changes, replayed in order when live mode is entered.
            Label {
                visible: !root.client.liveMode && root.client.pendingChanges > 0
                text: qsTr("%1 unsent change%2").arg(root.client.pendingChanges)
                                             .arg(root.client.pendingChanges === 1 ? "" : "s")
                color: Kirigami.Theme.highlightColor
            }
        }

        // Login row, only shown while connected and the server requires an edit password
        // that we have not logged in with yet - without a connection there is no session
        // to log into (a stale authRequired from a lost connection must not show a prompt
        // that cannot succeed), and the next connect re-checks /auth anyway.
        RowLayout {
            Layout.fillWidth: true
            visible: root.client.connected && root.client.authRequired && !root.client.authenticated
            spacing: 8

            Label { text: qsTr("Edit password required:") }
            TextField {
                id: passwordField
                echoMode: TextInput.Password
                Layout.preferredWidth: 200
            }
            Button {
                text: qsTr("Log in")
                onClicked: {
                    showStatus("");
                    root.client.login(passwordField.text);
                    passwordField.clear();
                }
            }
        }

        // Main area. Scenes and patterns stay editable in preview mode even while
        // disconnected (they edit the cached state locally); each control's own enabled
        // binding decides what is possible right now.
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            // Scenes and the work light.
            ColumnLayout {
                width: 340
                spacing: 8

                GroupBox {
                    title: qsTr("Scenes")
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 4

                        RowLayout {
                            Item { Layout.fillWidth: true }
                            Button {
                                // Manual scene sync for preview mode, where nothing is polled. In live
                                // mode the client refreshes the state every second, so it stays disabled;
                                // with queued changes a refresh would overwrite them, so wait until they
                                // are replayed (or discarded by leaving preview).
                                text: qsTr("Update scenes")
                                flat: true
                                icon.name: "view-refresh"
                                enabled: root.client.connected && !root.client.liveMode
                                         && root.client.pendingChanges === 0
                                onClicked: {
                                    showStatus("");
                                    root.client.refreshScenes();
                                }
                            }
                        }

                        ListView {
                            id: sceneList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: root.client.scenes
                            boundsBehavior: Flickable.StopAtBounds

                            delegate: RowLayout {
                                width: sceneList.width
                                spacing: 4

                                CheckBox {
                                    text: modelData.name
                                    checked: root.client.appliedScenes.indexOf(modelData.name) >= 0
                                    enabled: root.canEditLook
                                    // The QQC2 toggled() signal carries no parameters, so the toggle
                                    // direction is read from the checked property.
                                    onToggled: {
                                        showStatus("");
                                        if (checked)
                                            root.client.applyScene(modelData.name);
                                        else
                                            root.client.unapplyScene(modelData.name);
                                    }
                                }
                                Label {
                                    text: "(" + modelData.patternCount + ")"
                                    color: Kirigami.Theme.disabledTextColor
                                }

                                Item { Layout.fillWidth: true }

                                Button {
                                    text: qsTr("Replace")
                                    flat: true
                                    enabled: root.canEditLook
                                    onClicked: {
                                        showStatus("");
                                        root.client.replaceWithScene(modelData.name);
                                    }
                                }
                                ToolButton {
                                    icon.name: "list-add"
                                    onClicked: slideMenu.popup()
                                    ToolTip { text: qsTr("Attach to slide") }

                                    Menu {
                                        id: slideMenu
                                        MenuItem {
                                            text: qsTr("Apply on slide start")
                                            onTriggered: root.addSceneToSlide(modelData.name, "apply")
                                        }
                                        MenuItem {
                                            text: qsTr("Unapply on slide start")
                                            onTriggered: root.addSceneToSlide(modelData.name, "unapply")
                                        }
                                        MenuItem {
                                            text: qsTr("Replace with scene on slide start")
                                            onTriggered: root.addSceneToSlide(modelData.name, "replace")
                                        }
                                    }
                                }
                            }
                        }
                    }
                }



                GroupBox {
                    title: qsTr("Work light")
                    Layout.fillWidth: true

                    ColumnLayout {
                        spacing: 4
                        enabled: root.canControl

                        CheckBox {
                            id: solidCheck
                            text: qsTr("Enabled")
                            checked: root.client.solidEnabled
                        }
                        RowLayout {
                            Label { text: "R" }
                            SpinBox {
                                id: spinR
                                from: 0
                                to: 255
                                value: root.client.solidR
                                Layout.fillWidth: true
                            }
                            Label { text: "G" }
                            SpinBox {
                                id: spinG
                                from: 0
                                to: 255
                                value: root.client.solidG
                                Layout.fillWidth: true
                            }
                            Label { text: "B" }
                            SpinBox {
                                id: spinB
                                from: 0
                                to: 255
                                value: root.client.solidB
                                Layout.fillWidth: true
                            }
                        }
                        Button {
                            text: qsTr("Apply work light")
                            onClicked: {
                                showStatus("");
                                root.client.setSolidColor(spinR.value, spinG.value,
                                                           spinB.value, solidCheck.checked);
                            }
                        }
                    }
                }

                // NDI capture on the server. The switch mirrors whether it has a source
                // assigned; turning it on defaults to C-Play's own output (see
                // applyNdiDefault()).
                GroupBox {
                    title: qsTr("NDI")
                    Layout.fillWidth: true

                    ColumnLayout {
                        spacing: 4

                        RowLayout {
                            Switch {
                                id: ndiSwitch
                                text: qsTr("Use NDI source")
                                enabled: root.client.connected
                                ToolTip {
                                    text: qsTr("Receives an NDI source into C-Lux; defaults to C-Play's own output")
                                }
                                onToggled: {
                                    if (root.ndiSyncingSwitch)
                                        return;   // programmatic server-state sync, not the user
                                    showStatus("");
                                    if (checked) {
                                        // Refresh discovery first and pick the default from
                                        // that single update; see applyNdiDefault().
                                        root.ndiAwaitingDefault = true;
                                        root.client.refreshNdi();
                                    } else {
                                        root.client.setNdiSource("");
                                    }
                                }
                            }
                            Item { Layout.fillWidth: true }
                            Button {
                                // Manual source sync for preview mode, where nothing is polled. In live
                                // mode the client refreshes the NDI state every second as well.
                                text: qsTr("Update sources")
                                flat: true
                                icon.name: "view-refresh"
                                enabled: root.client.connected && !root.client.liveMode
                                onClicked: {
                                    showStatus("");
                                    root.client.refreshNdi();
                                }
                            }
                        }

                        ComboBox {
                            id: ndiCombo
                            Layout.fillWidth: true
                            // List of {name, urlAddress} maps from the server's discovery.
                            model: root.client.ndiSources
                            textRole: "name"
                            enabled: root.client.connected && ndiSwitch.checked
                            onActivated: function(index) {
                                showStatus("");
                                var src = root.client.ndiSources[index];
                                if (src !== undefined)
                                    root.client.setNdiSource(src.name);
                            }
                        }

                        // The thickness of the rim band each light samples in fisheye mode, as a
                        // fraction of the ring's radius. Sent on release rather than per pixel;
                        // the server keeps it whether or not a source is running, so this also
                        // pre-aims the next one.
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Label { text: qsTr("Ring width") }
                            Slider {
                                id: ringWidthSlider
                                Layout.fillWidth: true
                                from: 0
                                to: 1
                                stepSize: 0.001
                                value: root.client.ndiRingWidth
                                enabled: root.client.connected && ndiSwitch.checked
                                // This Qt's Slider has no released() signal; pressed goes false on
                                // mouse/touch/keyboard release, which is where the update goes out
                                // (one request per interaction, not one per pixel). Skipped when
                                // nothing changed.
                                onPressedChanged: {
                                    if (!pressed) {
                                        showStatus("");
                                        if (Math.abs(value - root.client.ndiRingWidth) > 0.0005)
                                            root.client.setNdiRingWidth(value);
                                    }
                                }
                            }
                            Label { text: ringWidthSlider.value.toFixed(3) }
                        }

                        Label {
                            text: root.ndiStatusText
                            color: root.ndiStatusIsError ? "red" : Kirigami.Theme.disabledTextColor
                            elide: Text.ElideRight
                        }
                    }
                }
            }


            // Patterns.
            ColumnLayout {
                width: 340
                spacing: 8

                GroupBox {
                    title: qsTr("Patterns")
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 4

                        RowLayout {
                            Item { Layout.fillWidth: true }
                            Button {
                                text: qsTr("Clear all patterns")
                                flat: true
                                icon.name: "edit-clear-all"
                                enabled: root.canEditLook
                                onClicked: {
                                    showStatus("");
                                    root.client.clearPatterns();
                                }
                            }
                        }

                        ListView {
                            id: patternList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: root.client.patterns
                            boundsBehavior: Flickable.StopAtBounds

                            delegate: RowLayout {
                                width: patternList.width
                                spacing: 4

                                Switch {
                                    checked: modelData.enabled
                                    enabled: root.canEditLook
                                    onToggled: {
                                        showStatus("");
                                        root.client.setPatternEnabled(modelData.name, checked);
                                    }
                                }
                                Label {
                                    text: modelData.name
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Label {
                                    text: "(" + modelData.type + ")"
                                    color: Kirigami.Theme.disabledTextColor
                                }
                            }
                        }
                    }
                }

                // Dome ring preview of the live frame stream.
                CLuxPreview {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: 200
                    Layout.minimumHeight: 100
                    nLights: root.client.nLights
                    frame: root.client.frame
                }
            }
        }

        // Bottom bar.
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                text: qsTr("Blackout")
                checkable: true
                checked: root.client.blackout
                enabled: root.canControl
                onToggled: {
                    showStatus("");
                    root.client.setBlackout(checked);
                }
            }
            Button {
                text: qsTr("Half light")
                checkable: true
                checked: root.client.halfLight
                enabled: root.canControl
                onToggled: {
                    showStatus("");
                    root.client.setHalfLight(checked);
                }
            }

            Item { Layout.fillWidth: true }

            Label {
                id: statusLabel
                elide: Text.ElideRight
                Layout.maximumWidth: 500
            }
        }
    }
}
