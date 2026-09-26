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
    width: 800

    readonly property var client: app.cluxClient

    // Live-mode operator commands (blackout, half light, work light) only work while
    // connected and in live mode; preview mode is read-only with respect to the server,
    // so they stay disabled there. Scene and pattern changes are locally editable in both
    // modes: in preview mode they update the local state and queue up for replay when
    // live mode is entered.
    readonly property bool canControl: client.connected && client.liveMode
    readonly property bool canEditLook: client.connected

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

    Connections {
        target: root.client
        function onErrorOccurred(message) { root.showStatus(message, true); }
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
                checked: root.client.liveMode
                onToggled: root.client.liveMode = checked
            }
            Label {
                text: root.client.liveMode ? qsTr("LIVE") : qsTr("PREVIEW")
                font.bold: true
                color: root.client.liveMode ? "red" : Kirigami.Theme.highlightColor
            }

            // Queued preview-mode changes, replayed in order when live mode is entered.
            Label {
                visible: !root.client.liveMode && root.client.pendingChanges > 0
                text: qsTr("%1 unsent change%2").arg(root.client.pendingChanges)
                                             .arg(root.client.pendingChanges === 1 ? "" : "s")
                color: Kirigami.Theme.highlightColor
            }
        }

        // Login row, only shown while the server requires an edit password.
        RowLayout {
            Layout.fillWidth: true
            visible: root.client.authRequired && !root.client.authenticated
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
                    root.client.login(passwordField.text);
                    passwordField.clear();
                }
            }
        }

        // Main area.
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            enabled: root.client.connected
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
                                onClicked: root.client.refreshScenes()
                            }
                        }

                        ListView {
                            id: sceneList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
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
                                    onToggled: checked ? root.client.applyScene(modelData.name)
                                                       : root.client.unapplyScene(modelData.name)
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
                                    onClicked: root.client.replaceWithScene(modelData.name)
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
                            onClicked: root.client.setSolidColor(spinR.value, spinG.value,
                                                                spinB.value, solidCheck.checked)
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
                                onClicked: root.client.clearPatterns()
                            }
                        }

                        ListView {
                            id: patternList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            model: root.client.patterns
                            boundsBehavior: Flickable.StopAtBounds

                            delegate: RowLayout {
                                width: patternList.width
                                spacing: 4

                                Switch {
                                    checked: modelData.enabled
                                    enabled: root.canEditLook
                                    onToggled: root.client.setPatternEnabled(modelData.name, checked)
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
                onToggled: root.client.setBlackout(checked)
            }
            Button {
                text: qsTr("Half light")
                checkable: true
                checked: root.client.halfLight
                enabled: root.canControl
                onToggled: root.client.setHalfLight(checked)
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
