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

// Edits data/nodes.json: the mapping from a node's IP address to the node id that multi-video
// compositions (*.cplaymulti) and per-node stream, DirectShow and capture settings use.
Kirigami.ApplicationWindow {
    id: root

    // Emitted after nodes.json was written, so other windows can refresh their node lists.
    signal saved()

    color: Kirigami.Theme.alternateBackgroundColor
    height: 560
    width: 720
    visible: false
    title: qsTr("Node Configuration") + (root.dirty ? " *" : "")

    property string filePath: ""
    property bool fileExists: false
    property bool legacyFile: false
    property bool dirty: false
    property var issues: []

    ListModel {
        id: nodesModel
    }

    MultiVideoComposerBackend {
        id: backend
    }

    onVisibleChanged: {
        if (visible && !root.dirty)
            root.reload();
    }

    function reload() {
        var r = backend.loadNodes();
        nodesModel.clear();
        for (var i = 0; i < r.nodes.length; ++i)
            nodesModel.append({ nodeId: r.nodes[i].id, ip: r.nodes[i].ip });
        root.filePath = r.path;
        root.fileExists = r.exists;
        root.legacyFile = !!r.legacy;
        root.dirty = false;
        if (!r.ok)
            statusLabel.show(r.error, true);
        else if (root.legacyFile)
            statusLabel.show(qsTr("Read from the old location; saving writes %1").arg(r.savePath), false);
        else
            statusLabel.show(r.exists ? qsTr("Loaded %1 node(s)").arg(nodesModel.count) : qsTr("No nodes.json yet; it is created when you save."), false);
        root.validate();
    }

    function changed() {
        root.dirty = true;
        root.validate();
    }

    function nodesList() {
        var list = [];
        for (var i = 0; i < nodesModel.count; ++i)
            list.push({ id: nodesModel.get(i).nodeId, ip: nodesModel.get(i).ip });
        return list;
    }

    function uniqueId() {
        var ids = root.nodesList().map(function (n) { return n.id; });
        for (var n = 1; ; ++n) {
            if (ids.indexOf("Node" + n) < 0)
                return "Node" + n;
        }
    }

    // Node addresses from the running SGCT cluster configuration, without the master's own address.
    function clusterAddresses() {
        var list = [];
        if (!ConfigModel.loaded)
            return list;
        for (var i = 0; i < ConfigModel.numberOfNodes; ++i) {
            var a = ConfigModel.nodeAddress(i);
            if (a !== "" && a !== ConfigModel.masterAddress && list.indexOf(a) < 0)
                list.push(a);
        }
        return list;
    }

    function importFromCluster() {
        var ips = root.nodesList().map(function (n) { return n.ip; });
        var added = 0;
        var addresses = root.clusterAddresses();
        for (var i = 0; i < addresses.length; ++i) {
            if (ips.indexOf(addresses[i]) >= 0)
                continue;
            nodesModel.append({ nodeId: root.uniqueId(), ip: addresses[i] });
            ++added;
        }
        statusLabel.show(added > 0 ? qsTr("Added %1 node(s) from the cluster configuration").arg(added) : qsTr("All cluster node addresses are already listed"), false);
        if (added > 0)
            root.changed();
    }

    function validate() {
        var list = root.nodesList();
        var out = [];
        var ids = {};
        var ips = {};
        for (var i = 0; i < list.length; ++i) {
            var row = qsTr("Row %1").arg(i + 1);
            var id = list[i].id.trim();
            var ip = list[i].ip.trim();
            if (id === "" && ip === "")
                continue;
            if (id === "")
                out.push({ level: "error", text: qsTr("%1 has no node id.").arg(row) });
            if (ip === "")
                out.push({ level: "error", text: qsTr("%1 (%2) has no IP address.").arg(row).arg(id) });
            if (id !== "" && ids[id])
                out.push({ level: "warning", text: qsTr("Node id '%1' is used more than once; those nodes get the same files.").arg(id) });
            if (ip !== "" && ips[ip])
                out.push({ level: "error", text: qsTr("IP address %1 is listed more than once; only the last row counts.").arg(ip) });
            ids[id] = true;
            ips[ip] = true;
        }
        var cluster = root.clusterAddresses();
        var missing = cluster.filter(function (a) { return !ips[a]; });
        if (missing.length > 0)
            out.push({ level: "warning", text: qsTr("Cluster nodes without a node id: %1").arg(missing.join(", ")) });
        if (cluster.length > 0) {
            var unknown = Object.keys(ips).filter(function (a) { return a !== "" && cluster.indexOf(a) < 0; });
            if (unknown.length > 0)
                out.push({ level: "info", text: qsTr("Not in the current cluster configuration: %1").arg(unknown.join(", ")) });
        }
        root.issues = out;
    }

    function save() {
        var err = backend.saveNodes(root.nodesList());
        if (err !== "") {
            statusLabel.show(err, true);
            return;
        }
        root.dirty = false;
        root.fileExists = true;
        root.legacyFile = false;
        root.filePath = backend.nodesSavePath();
        statusLabel.show(qsTr("Saved %1").arg(root.filePath), false);
        root.saved();
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.smallSpacing

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Each node looks up its own IP address here to find its node id. Multi-video compositions use the node ids to pick the files and audio for each node, and per-node stream, DirectShow and capture settings use them to pick each node's source.")
        }
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: true
            type: Kirigami.MessageType.Information
            text: qsTr("Every computer reads data/nodes.json from its own C-Play folder. After saving, copy the file to every node computer (unless they share this folder) and restart C-Play on the nodes.")
        }
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: root.legacyFile
            type: Kirigami.MessageType.Warning
            text: qsTr("This nodes.json is in the old data/multivideo folder. Save to write it to data/nodes.json, then copy that file to the nodes as well.")
        }
        RowLayout {
            Layout.fillWidth: true
            Label {
                text: qsTr("File:")
            }
            Label {
                Layout.fillWidth: true
                elide: Text.ElideMiddle
                text: root.filePath + (root.fileExists ? "" : qsTr(" (not created yet)"))
                color: root.fileExists ? Kirigami.Theme.textColor : Kirigami.Theme.neutralTextColor
            }
        }

        // Table header
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.smallSpacing
            Label {
                Layout.fillWidth: true
                font.bold: true
                text: qsTr("Node id")
            }
            Label {
                Layout.fillWidth: true
                font.bold: true
                text: qsTr("IP address")
            }
            Item {
                Layout.preferredWidth: rowButtons.implicitWidth
            }
        }

        ListView {
            id: nodesList

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 120
            clip: true
            model: nodesModel
            spacing: 2

            ScrollBar.vertical: ScrollBar {}

            delegate: RowLayout {
                id: rowDelegate

                required property int index
                required property string nodeId
                required property string ip

                width: ListView.view.width - Kirigami.Units.largeSpacing

                TextField {
                    Layout.fillWidth: true
                    text: rowDelegate.nodeId
                    placeholderText: qsTr("e.g. Node1")
                    onTextEdited: {
                        nodesModel.setProperty(rowDelegate.index, "nodeId", text);
                        root.changed();
                    }
                }
                TextField {
                    Layout.fillWidth: true
                    text: rowDelegate.ip
                    placeholderText: qsTr("e.g. 10.0.0.11")
                    onTextEdited: {
                        nodesModel.setProperty(rowDelegate.index, "ip", text);
                        root.changed();
                    }
                }
                RowLayout {
                    spacing: 0
                    ToolButton {
                        icon.name: "go-up"
                        enabled: rowDelegate.index > 0
                        onClicked: {
                            nodesModel.move(rowDelegate.index, rowDelegate.index - 1, 1);
                            root.changed();
                        }
                    }
                    ToolButton {
                        icon.name: "go-down"
                        enabled: rowDelegate.index < nodesModel.count - 1
                        onClicked: {
                            nodesModel.move(rowDelegate.index, rowDelegate.index + 1, 1);
                            root.changed();
                        }
                    }
                    ToolButton {
                        icon.name: "edit-delete"
                        onClicked: {
                            nodesModel.remove(rowDelegate.index);
                            root.changed();
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Remove node")
                    }
                }
            }
        }
        // Width reference for the header spacer (same buttons as a row).
        RowLayout {
            id: rowButtons
            visible: false
            spacing: 0
            ToolButton { icon.name: "go-up" }
            ToolButton { icon.name: "go-down" }
            ToolButton { icon.name: "edit-delete" }
        }

        // Checks
        Repeater {
            model: root.issues
            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true
                Kirigami.Icon {
                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                    Layout.preferredHeight: Kirigami.Units.iconSizes.small
                    source: modelData.level === "error" ? "dialog-error" : (modelData.level === "warning" ? "dialog-warning" : "dialog-information")
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: modelData.text
                    color: modelData.level === "error" ? Kirigami.Theme.negativeTextColor : (modelData.level === "warning" ? Kirigami.Theme.neutralTextColor : Kirigami.Theme.textColor)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Button {
                icon.name: "list-add"
                text: qsTr("Add node")
                onClicked: {
                    nodesModel.append({ nodeId: root.uniqueId(), ip: "" });
                    nodesList.positionViewAtEnd();
                    root.changed();
                }
            }
            Button {
                icon.name: "network-server"
                text: qsTr("Add from cluster")
                enabled: root.clusterAddresses().length > 0
                onClicked: root.importFromCluster()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Add a row for each node address in the running cluster configuration that is not listed yet.")
            }
            Button {
                icon.name: "view-refresh"
                text: qsTr("Reload")
                onClicked: root.reload()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Discard changes and read nodes.json again.")
            }
            Label {
                id: statusLabel

                Layout.fillWidth: true
                elide: Text.ElideMiddle
                horizontalAlignment: Text.AlignRight

                function show(message, isError) {
                    text = message;
                    color = isError ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.disabledTextColor;
                }
            }
            Button {
                icon.name: "document-save"
                text: qsTr("Save")
                enabled: root.dirty || !root.fileExists || root.legacyFile
                onClicked: root.save()
            }
        }
    }
}
