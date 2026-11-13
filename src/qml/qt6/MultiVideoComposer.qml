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
import "Components/WindowHelpers.js" as WindowHelpers

// Builds multi-video composition files (*.cplaymulti): a master reference video plus entries
// that play on the cluster nodes. Each entry is a video (per-node file, with per-node audio
// on/off and separate audio file) or an audio-only stream (per-node audio file). Audio can so be
// placed on the master, on every node, or on one single node.
Kirigami.ApplicationWindow {
    id: root

    color: Kirigami.Theme.alternateBackgroundColor
    height: 780
    width: 1180
    visible: false
    title: qsTr("Multi-Video Composer") + " - " + (root.filePath !== "" ? root.fileName(root.filePath) : qsTr("Untitled")) + (root.dirty ? " *" : "")

    // ---- State -------------------------------------------------------------------------
    // comp is a plain JS object (see MultiVideoComposer in multivideocomposer.h for its layout).
    // Value edits write straight into it; rev is bumped for structural changes so the views
    // that depend on the entry list / node list rebuild.
    property var comp: root.plain(backend.newComposition())
    property string filePath: ""
    property bool dirty: false
    property int rev: 0
    property int selected: -1 // -1 = master, otherwise index into comp.videos
    property var nodeIds: []
    property var extraNodeIds: [] // node ids added by hand in this session
    property var issues: []
    property var routing: []
    property string lastFolder: ""
    property var pendingAction: null

    readonly property var gridModes: ["None", "Plane", "Dome", "Sphere_EQR", "Sphere_EAC"]
    readonly property var gridLabels: [qsTr("None/Pre-split"), qsTr("Plane/Flat"), qsTr("Dome"), qsTr("Sphere EQR"), qsTr("Sphere EAC")]
    readonly property var stereoModes: ["No_2D", "SBS_3D", "TB_3D", "TBF_3D"]
    readonly property var stereoLabels: [qsTr("2D (mono)"), qsTr("3D (side-by-side)"), qsTr("3D (top-bottom)"), qsTr("3D (top-bottom+flip)")]
    readonly property var eyeModes: ["Both", "left", "right"]
    readonly property var eyeLabels: [qsTr("Both eyes"), qsTr("Left eye only"), qsTr("Right eye only")]
    readonly property var videoFilters: ["Video files (*.mp4 *.mov *.mkv *.avi *.webm *.m4v *.mxf *.ts *.mpg *.mpeg)", "All files (*)"]
    readonly property var audioFilters: ["Audio files (*.wav *.mp3 *.flac *.ogg *.m4a *.aac *.opus *.ac3)", "All files (*)"]

    readonly property int nodeColumnWidth: 110
    readonly property int audioColumnWidth: 150
    readonly property var entry: root.rev >= 0 && root.selected >= 0 && root.selected < root.comp.videos.length ? root.comp.videos[root.selected] : null
    readonly property bool entryIsAudio: !!root.entry && root.entry.type === "audio"

    MultiVideoComposerBackend {
        id: backend
    }

    Timer {
        id: refreshTimer
        interval: 250
        onTriggered: root.refreshChecks()
    }

    onVisibleChanged: {
        if (visible) {
            root.rebuildNodeIds();
            root.refreshChecks();
        }
    }

    // ---- Helpers -----------------------------------------------------------------------
    function plain(v) {
        return JSON.parse(JSON.stringify(v));
    }

    function fileName(path) {
        var p = String(path).replace(/\\/g, "/");
        return p.substring(p.lastIndexOf("/") + 1);
    }

    function changed() {
        root.dirty = true;
        refreshTimer.restart();
    }

    function structureChanged() {
        root.rev++;
        root.changed();
    }

    function refreshChecks() {
        root.issues = backend.validate(root.comp, root.nodeIds);
        root.routing = backend.audioRouting(root.comp, root.nodeIds);
        if (bottomTabs.currentIndex === 2)
            jsonText.text = backend.toJson(root.comp);
    }

    function rebuildNodeIds() {
        var ids = backend.nodeIdsFromNodesFile();
        var add = function (id) {
            if (id !== "" && ids.indexOf(id) < 0)
                ids.push(id);
        };
        for (var i = 0; i < root.comp.videos.length; ++i) {
            var nodes = root.comp.videos[i].nodes;
            for (var j = 0; j < nodes.length; ++j)
                add(nodes[j].id);
        }
        for (var k = 0; k < root.extraNodeIds.length; ++k)
            add(root.extraNodeIds[k]);
        root.nodeIds = ids;
    }

    // nodes.json was edited (Node Configuration window): pick up added or renamed node ids.
    function reloadNodes() {
        root.rebuildNodeIds();
        root.rev++;
        root.refreshChecks();
    }

    function addNodeId(id) {
        id = id.trim();
        if (id === "" || root.nodeIds.indexOf(id) >= 0)
            return;
        var extra = root.extraNodeIds.slice();
        extra.push(id);
        root.extraNodeIds = extra;
        root.rebuildNodeIds();
        root.structureChanged();
    }

    function nodeRec(e, id, create) {
        if (!e)
            return null;
        for (var i = 0; i < e.nodes.length; ++i) {
            if (e.nodes[i].id === id)
                return e.nodes[i];
        }
        if (!create)
            return null;
        var r = { id: id, file: "", audio: "default", audioFile: "" };
        e.nodes.push(r);
        return r;
    }

    function nodeValue(e, id, key, fallback) {
        var r = root.nodeRec(e, id, false);
        return (r && r[key] !== undefined) ? r[key] : fallback;
    }

    function setNodeValue(e, id, key, value) {
        root.nodeRec(e, id, true)[key] = value;
        root.changed();
    }

    function resolvedFile(e, id) {
        var direct = String(root.nodeValue(e, id, "file", "")).trim();
        if (direct !== "")
            return direct;
        var t = String(e.pathTemplate || "").trim();
        return t === "" ? "" : t.replace("{nodeId}", id);
    }

    function audioOnNode(e, id) {
        if (root.resolvedFile(e, id) === "")
            return false;
        if (e.type === "audio")
            return true;
        var a = root.nodeValue(e, id, "audio", "default");
        return a === "on" || (a !== "off" && e.audio);
    }

    function entrySummary(e) {
        var ids = root.nodeIds;
        var playing = [];
        var audio = [];
        for (var i = 0; i < ids.length; ++i) {
            if (root.resolvedFile(e, ids[i]) !== "")
                playing.push(ids[i]);
            if (root.audioOnNode(e, ids[i]))
                audio.push(ids[i]);
        }
        var s = playing.length === 0 ? qsTr("no nodes") : (playing.length === ids.length && ids.length > 1 ? qsTr("all %1 nodes").arg(ids.length) : playing.join(", "));
        if (e.type === "audio")
            return qsTr("Audio on %1").arg(s);
        if (audio.length === 0)
            return s + qsTr(" · no audio");
        if (audio.length === playing.length && playing.length > 1)
            return s + qsTr(" · audio on all");
        return s + qsTr(" · audio on %1").arg(audio.join(", "));
    }

    function uniqueName(base) {
        var names = root.comp.videos.map(function (v) { return v.name; });
        for (var n = 1; ; ++n) {
            if (names.indexOf(base + n) < 0)
                return base + n;
        }
    }

    function addEntry(type) {
        var e = root.plain(backend.newEntry(type, root.uniqueName(type === "audio" ? "audio" : "video")));
        root.comp.videos.push(e);
        root.selected = root.comp.videos.length - 1;
        root.structureChanged();
    }

    function duplicateEntry() {
        if (!root.entry)
            return;
        var e = root.plain(root.entry);
        e.name = root.uniqueName(e.name + "_copy");
        root.comp.videos.splice(root.selected + 1, 0, e);
        root.selected = root.selected + 1;
        root.structureChanged();
    }

    function removeEntry() {
        if (!root.entry)
            return;
        root.comp.videos.splice(root.selected, 1);
        root.selected = Math.min(root.selected, root.comp.videos.length - 1);
        root.structureChanged();
    }

    function moveEntry(delta) {
        var to = root.selected + delta;
        if (!root.entry || to < 0 || to >= root.comp.videos.length)
            return;
        var e = root.comp.videos.splice(root.selected, 1)[0];
        root.comp.videos.splice(to, 0, e);
        root.selected = to;
        root.structureChanged();
    }

    // Audio of a video entry: everywhere, nowhere, or only on one node.
    function setEntryAudio(mode, nodeId) {
        var e = root.entry;
        if (!e)
            return;
        e.audio = (mode === "all");
        for (var i = 0; i < e.nodes.length; ++i) {
            e.nodes[i].audio = "default";
        }
        if (mode === "only")
            root.nodeRec(e, nodeId, true).audio = "on";
        root.structureChanged();
    }

    function setComposition(c, path) {
        root.comp = root.plain(c);
        root.filePath = path;
        root.dirty = false;
        root.selected = -1;
        root.extraNodeIds = [];
        root.rebuildNodeIds();
        root.rev++;
        root.refreshChecks();
        statusLabel.show(path !== "" ? qsTr("Opened %1").arg(path) : qsTr("New composition"), false);
    }

    // Runs action now, or after the user agreed to drop unsaved changes.
    function guardUnsaved(action) {
        if (!root.dirty) {
            action();
            return;
        }
        root.pendingAction = action;
        discardDialog.open();
    }

    function newComposition() {
        root.guardUnsaved(function () {
            root.setComposition(backend.newComposition(), "");
        });
    }

    // Opens a .cplaymulti file in the composer (also used from the Save As C-Play File window).
    function openFile(path) {
        root.guardUnsaved(function () {
            var r = backend.load(path);
            if (!r.ok) {
                statusLabel.show(r.error, true);
                return;
            }
            root.setComposition(r.composition, path);
            root.lastFolder = path;
        });
    }

    function saveTo(path, thenPlay) {
        if (path.toLowerCase().indexOf(".cplaymulti") < 0 && path.toLowerCase().indexOf(".json") < 0)
            path += ".cplaymulti";
        // Write node paths in the order of the node list.
        for (var i = 0; i < root.comp.videos.length; ++i) {
            root.comp.videos[i].nodes.sort(function (a, b) {
                return root.nodeIds.indexOf(a.id) - root.nodeIds.indexOf(b.id);
            });
        }
        var err = backend.save(path, root.comp);
        if (err !== "") {
            statusLabel.show(err, true);
            return;
        }
        root.filePath = path;
        root.dirty = false;
        root.lastFolder = path;
        statusLabel.show(qsTr("Saved %1").arg(path), false);
        if (thenPlay)
            mpv.loadFile(path);
    }

    function save(thenPlay) {
        if (root.filePath === "") {
            saveDialog.thenPlay = thenPlay;
            root.openSaveDialog();
            return;
        }
        root.saveTo(root.filePath, thenPlay);
    }

    function openSaveDialog() {
        var folder = backend.folderUrlOf(root.filePath !== "" ? root.filePath : root.lastFolder);
        if (folder.toString() !== "")
            saveDialog.currentFolder = folder;
        Qt.callLater(function () {
            saveDialog.open();
        });
    }

    function pick(title, filters, current, callback) {
        pickDialog.title = title;
        pickDialog.nameFilters = filters;
        var folder = backend.folderUrlOf(current !== "" ? current : root.lastFolder);
        if (folder.toString() === "" && LocationSettings.cPlayMediaLocation !== "")
            folder = app.pathToUrl(LocationSettings.cPlayMediaLocation);
        if (folder.toString() !== "")
            pickDialog.currentFolder = folder;
        pickDialog.callback = callback;
        Qt.callLater(function () {
            pickDialog.open();
        });
    }

    // ---- Dialogs -----------------------------------------------------------------------
    CPlayFileDialog {
        id: pickDialog

        property var callback: null

        parentWindow: root
        fileMode: CPlayFileDialog.OpenFile

        onAccepted: {
            var path = backend.urlToPath(pickDialog.selectedFile);
            root.lastFolder = path;
            if (pickDialog.callback)
                pickDialog.callback(path);
            pickDialog.callback = null;
        }
        onRejected: pickDialog.callback = null
    }

    CPlayFileDialog {
        id: openDialog

        parentWindow: root
        fileMode: CPlayFileDialog.OpenFile
        nameFilters: ["Multi-video composition (*.cplaymulti)", "JSON (*.json)", "All files (*)"]
        title: qsTr("Open Multi-Video Composition")

        onAccepted: root.openFile(backend.urlToPath(openDialog.selectedFile))
    }

    CPlayFileDialog {
        id: saveDialog

        property bool thenPlay: false

        parentWindow: root
        fileMode: CPlayFileDialog.SaveFile
        nameFilters: ["Multi-video composition (*.cplaymulti)"]
        title: qsTr("Save Multi-Video Composition")

        onAccepted: {
            root.saveTo(backend.urlToPath(saveDialog.selectedFile), saveDialog.thenPlay);
            saveDialog.thenPlay = false;
        }
        onRejected: saveDialog.thenPlay = false
    }

    Dialog {
        id: discardDialog

        anchors.centerIn: Overlay.overlay
        modal: true
        title: qsTr("Discard unsaved changes?")
        standardButtons: Dialog.Yes | Dialog.No

        Label {
            text: qsTr("The current composition has unsaved changes.")
        }

        onAccepted: {
            var a = root.pendingAction;
            root.pendingAction = null;
            root.dirty = false;
            if (a)
                a();
        }
        onRejected: root.pendingAction = null
    }

    // ---- Reusable bits -----------------------------------------------------------------
    // Path field with browse/clear. The owner keeps the path in the composition: typing emits
    // edited(), browse/clear emit chosen() and the owner refreshes the bound text (via rev).
    component FileField: RowLayout {
        id: ff

        property string text: ""
        property string placeholder: ""
        property string dialogTitle: ""
        property var filters: root.videoFilters
        signal chosen(string path)
        signal edited(string path)

        Layout.fillWidth: true
        spacing: 2

        TextField {
            id: ffText
            Layout.fillWidth: true
            Layout.minimumWidth: 120
            text: ff.text
            placeholderText: ff.placeholder
            // Absolute paths missing on this computer are flagged; they may still exist on a node.
            color: text === "" || !/^([A-Za-z]:[\\/]|\/|\\\\)/.test(text) || backend.fileExists(text) ? Kirigami.Theme.textColor : Kirigami.Theme.neutralTextColor
            onTextEdited: ff.edited(text)

            ToolTip.visible: hovered && text !== ""
            ToolTip.text: backend.fileExists(text) ? text : text + "\n" + qsTr("(not found on this computer; fine if the file only exists on the node)")
        }
        ToolButton {
            icon.name: "document-open"
            focusPolicy: Qt.NoFocus
            onClicked: root.pick(ff.dialogTitle, ff.filters, ffText.text, function (path) {
                ff.chosen(path);
            })
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Browse...")
        }
        ToolButton {
            icon.name: "edit-clear"
            focusPolicy: Qt.NoFocus
            enabled: ffText.text !== ""
            onClicked: ff.chosen("")
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Clear")
        }
    }

    component SectionHeader: RowLayout {
        property alias text: shLabel.text
        Layout.fillWidth: true
        Layout.topMargin: Kirigami.Units.smallSpacing
        Label {
            id: shLabel
            font.bold: true
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Kirigami.Theme.disabledTextColor
            opacity: 0.4
        }
    }

    // ---- Layout ------------------------------------------------------------------------
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.smallSpacing

        // Toolbar
        RowLayout {
            Layout.fillWidth: true

            Button {
                icon.name: "document-new"
                text: qsTr("New")
                onClicked: root.newComposition()
            }
            Button {
                icon.name: "document-open"
                text: qsTr("Open...")
                onClicked: {
                    var folder = backend.folderUrlOf(root.filePath !== "" ? root.filePath : root.lastFolder);
                    if (folder.toString() !== "")
                        openDialog.currentFolder = folder;
                    else
                        openDialog.currentFolder = app.pathToUrl("./data/multivideo");
                    Qt.callLater(function () {
                        openDialog.open();
                    });
                }
            }
            Button {
                icon.name: "document-save"
                text: qsTr("Save")
                onClicked: root.save(false)
            }
            Button {
                icon.name: "document-save-as"
                text: qsTr("Save As...")
                onClicked: {
                    saveDialog.thenPlay = false;
                    root.openSaveDialog();
                }
            }
            Button {
                icon.name: "media-playback-start"
                text: qsTr("Save and Play")
                onClicked: root.save(true)
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Save the composition and load it on the master and all nodes.")
            }
            Item {
                Layout.fillWidth: true
            }
            Label {
                id: statusLabel

                Layout.maximumWidth: root.width * 0.45
                elide: Text.ElideMiddle

                function show(message, isError) {
                    text = message;
                    color = isError ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.disabledTextColor;
                }
            }
        }

        // Nodes bar
        RowLayout {
            Layout.fillWidth: true

            Label {
                text: qsTr("Nodes:")
            }
            Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: root.nodeIds.length > 0 ? root.nodeIds.join(", ") : qsTr("(none; add node ids or create data/nodes.json)")
                color: root.nodeIds.length > 0 ? Kirigami.Theme.textColor : Kirigami.Theme.neutralTextColor

                ToolTip.visible: nodesMouse.containsMouse
                ToolTip.text: backend.nodesFilePath() !== "" ? qsTr("Node ids from %1, plus ids used in this composition.").arg(backend.nodesFilePath()) : qsTr("nodes.json not found.")
                MouseArea {
                    id: nodesMouse
                    anchors.fill: parent
                    hoverEnabled: true
                }
            }
            TextField {
                id: newNodeField
                Layout.preferredWidth: 140
                placeholderText: qsTr("Node id")
                onAccepted: {
                    root.addNodeId(text);
                    text = "";
                }
            }
            Button {
                icon.name: "network-workgroup"
                text: qsTr("Node Configuration...")
                onClicked: WindowHelpers.showWindow(nodeConfigurationWindow)
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Edit nodes.json: the node ids and the IP address of each node.")
            }
            Button {
                icon.name: "list-add"
                text: qsTr("Add node")
                enabled: newNodeField.text.trim() !== ""
                onClicked: {
                    root.addNodeId(newNodeField.text);
                    newNodeField.text = "";
                }
            }
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Vertical

            // Upper part: entry list + details
            SplitView {
                SplitView.fillHeight: true
                SplitView.minimumHeight: 260
                orientation: Qt.Horizontal

                // Entry list
                ColumnLayout {
                    SplitView.preferredWidth: 290
                    SplitView.minimumWidth: 220
                    spacing: Kirigami.Units.smallSpacing

                    ListView {
                        id: entryList

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: root.rev >= 0 ? root.comp.videos.length + 1 : 0
                        currentIndex: root.selected + 1

                        delegate: ItemDelegate {
                            readonly property bool isMaster: index === 0
                            readonly property var e: isMaster ? null : root.comp.videos[index - 1]

                            width: ListView.view.width
                            highlighted: ListView.isCurrentItem
                            onClicked: root.selected = index - 1

                            contentItem: RowLayout {
                                spacing: Kirigami.Units.smallSpacing
                                Kirigami.Icon {
                                    Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                                    Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                                    source: isMaster ? "computer" : (e.type === "audio" ? "audio-x-generic" : "video-x-generic")
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 0
                                    Label {
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                        font.bold: isMaster
                                        text: isMaster ? qsTr("Master reference") : (e.name !== "" ? e.name : qsTr("(unnamed)"))
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                        font: Kirigami.Theme.smallFont
                                        color: Kirigami.Theme.disabledTextColor
                                        text: {
                                            root.issues; // re-evaluate after edits
                                            if (isMaster) {
                                                var m = root.comp.master;
                                                return (m.file !== "" ? root.fileName(m.file) : qsTr("no file")) + (m.audio ? qsTr(" · audio") : qsTr(" · muted"));
                                            }
                                            return root.entrySummary(e);
                                        }
                                    }
                                }
                            }
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 3
                        columnSpacing: 2
                        rowSpacing: 2

                        Button {
                            Layout.fillWidth: true
                            icon.name: "video-x-generic"
                            text: qsTr("Add video")
                            onClicked: root.addEntry("video")
                        }
                        Button {
                            Layout.fillWidth: true
                            icon.name: "audio-x-generic"
                            text: qsTr("Add audio")
                            onClicked: root.addEntry("audio")
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("An audio-only stream that plays on the nodes you give a file, e.g. only the node wired to the sound system.")
                        }
                        Button {
                            Layout.fillWidth: true
                            icon.name: "edit-copy"
                            text: qsTr("Duplicate")
                            enabled: !!root.entry
                            onClicked: root.duplicateEntry()
                        }
                        Button {
                            Layout.fillWidth: true
                            icon.name: "go-up"
                            text: qsTr("Up")
                            enabled: root.selected > 0
                            onClicked: root.moveEntry(-1)
                        }
                        Button {
                            Layout.fillWidth: true
                            icon.name: "go-down"
                            text: qsTr("Down")
                            enabled: !!root.entry && root.selected < root.comp.videos.length - 1
                            onClicked: root.moveEntry(1)
                        }
                        Button {
                            Layout.fillWidth: true
                            icon.name: "edit-delete"
                            text: qsTr("Remove")
                            enabled: !!root.entry
                            onClicked: root.removeEntry()
                        }
                    }
                }

                // Details
                ScrollView {
                    id: detailsScroll

                    SplitView.fillWidth: true
                    SplitView.minimumWidth: 420
                    clip: true
                    contentWidth: availableWidth

                    ColumnLayout {
                        width: detailsScroll.availableWidth
                        spacing: Kirigami.Units.smallSpacing

                        // ---------- Master ----------
                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: root.selected < 0

                            SectionHeader {
                                text: qsTr("Master reference video")
                            }
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                color: Kirigami.Theme.disabledTextColor
                                text: qsTr("Played on the master for preview and timeline control. All node entries follow its time.")
                            }
                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2

                                Label {
                                    text: qsTr("File:")
                                }
                                FileField {
                                    text: root.rev >= 0 ? root.comp.master.file : ""
                                    dialogTitle: qsTr("Choose Master Reference Video")
                                    placeholder: qsTr("Reference video on the master")
                                    onChosen: path => { root.comp.master.file = path; root.structureChanged(); }
                                    onEdited: path => { root.comp.master.file = path; root.changed(); }
                                }
                                Label {
                                    text: qsTr("Grid mode:")
                                }
                                ComboBox {
                                    Layout.fillWidth: true
                                    model: root.gridLabels
                                    currentIndex: root.rev >= 0 ? Math.max(0, root.gridModes.indexOf(root.comp.master.gridMode)) : 0
                                    onActivated: { root.comp.master.gridMode = root.gridModes[currentIndex]; root.changed(); }
                                }
                                Label {
                                    text: qsTr("Stereoscopic mode:")
                                }
                                ComboBox {
                                    Layout.fillWidth: true
                                    model: root.stereoLabels
                                    currentIndex: root.rev >= 0 ? Math.max(0, root.stereoModes.indexOf(root.comp.master.stereoMode)) : 0
                                    onActivated: { root.comp.master.stereoMode = root.stereoModes[currentIndex]; root.changed(); }
                                }
                            }

                            SectionHeader {
                                text: qsTr("Master audio")
                            }
                            CheckBox {
                                text: qsTr("Play audio on the master")
                                checked: root.rev >= 0 && root.comp.master.audio
                                onToggled: { root.comp.master.audio = checked; root.structureChanged(); }
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Turn off when the audio plays on a node instead. When on, the audio settings of C-Play still decide the output device.")
                            }
                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                enabled: root.rev >= 0 && root.comp.master.audio

                                Label {
                                    text: qsTr("Separate audio file:")
                                }
                                FileField {
                                    text: root.rev >= 0 ? root.comp.master.audioFile : ""
                                    filters: root.audioFilters
                                    dialogTitle: qsTr("Choose Master Audio File")
                                    placeholder: qsTr("Optional; otherwise the reference video's own audio")
                                    onChosen: path => { root.comp.master.audioFile = path; root.structureChanged(); }
                                    onEdited: path => { root.comp.master.audioFile = path; root.changed(); }
                                }
                            }
                        }

                        // ---------- Entry ----------
                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: !!root.entry

                            SectionHeader {
                                text: root.entryIsAudio ? qsTr("Audio-only entry") : qsTr("Video entry")
                            }
                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2

                                Label {
                                    text: qsTr("Name:")
                                }
                                TextField {
                                    Layout.fillWidth: true
                                    text: root.entry ? root.entry.name : ""
                                    placeholderText: qsTr("Used in logs, e.g. leftEye")
                                    onTextEdited: { root.entry.name = text; root.changed(); }
                                    onEditingFinished: root.rev++ // refresh the entry list label
                                }
                                Label {
                                    visible: !root.entryIsAudio
                                    text: qsTr("Eye:")
                                }
                                ComboBox {
                                    visible: !root.entryIsAudio
                                    Layout.fillWidth: true
                                    model: root.eyeLabels
                                    currentIndex: root.entry ? Math.max(0, root.eyeModes.indexOf(root.entry.eyeMode)) : 0
                                    onActivated: { root.entry.eyeMode = root.eyeModes[currentIndex]; root.changed(); }
                                }
                                Label {
                                    visible: !root.entryIsAudio
                                    text: qsTr("Grid mode:")
                                }
                                ComboBox {
                                    visible: !root.entryIsAudio
                                    Layout.fillWidth: true
                                    model: root.gridLabels
                                    currentIndex: root.entry ? Math.max(0, root.gridModes.indexOf(root.entry.gridMode)) : 0
                                    onActivated: { root.entry.gridMode = root.gridModes[currentIndex]; root.changed(); }
                                }
                                Label {
                                    visible: !root.entryIsAudio
                                    text: qsTr("Stereoscopic mode:")
                                }
                                ComboBox {
                                    visible: !root.entryIsAudio
                                    Layout.fillWidth: true
                                    model: root.stereoLabels
                                    currentIndex: root.entry ? Math.max(0, root.stereoModes.indexOf(root.entry.stereoMode)) : 0
                                    onActivated: { root.entry.stereoMode = root.stereoModes[currentIndex]; root.changed(); }
                                }
                                Label {
                                    text: qsTr("Path template:")
                                }
                                TextField {
                                    Layout.fillWidth: true
                                    text: root.entry ? root.entry.pathTemplate : ""
                                    placeholderText: root.entryIsAudio ? qsTr("Optional, e.g. /media/{nodeId}/audio.wav (then every node plays it)") : qsTr("Optional, e.g. /media/sliced/{nodeId}/left.mp4")
                                    onTextEdited: { root.entry.pathTemplate = text; root.changed(); }
                                    onEditingFinished: root.rev++
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Used for nodes without a file below. {nodeId} is replaced by the node id.")
                                }
                            }

                            // Audio defaults of a video entry
                            SectionHeader {
                                visible: !root.entryIsAudio
                                text: qsTr("Audio")
                            }
                            RowLayout {
                                visible: !root.entryIsAudio
                                Layout.fillWidth: true

                                Label {
                                    text: qsTr("Play audio on:")
                                }
                                Button {
                                    text: qsTr("All nodes")
                                    checkable: true
                                    checked: !!root.entry && root.rev >= 0 && root.entry.audio && root.entry.nodes.every(function (n) { return n.audio !== "off"; })
                                    onClicked: root.setEntryAudio("all")
                                }
                                Button {
                                    text: qsTr("No nodes")
                                    checkable: true
                                    checked: !!root.entry && root.rev >= 0 && !root.entry.audio && root.entry.nodes.every(function (n) { return n.audio !== "on"; })
                                    onClicked: root.setEntryAudio("none")
                                }
                                Label {
                                    text: qsTr("Only on")
                                }
                                ComboBox {
                                    id: onlyNodeCombo
                                    Layout.preferredWidth: 140
                                    model: root.nodeIds
                                }
                                Button {
                                    text: qsTr("Apply")
                                    enabled: onlyNodeCombo.currentIndex >= 0
                                    onClicked: root.setEntryAudio("only", onlyNodeCombo.currentText)
                                }
                                Item {
                                    Layout.fillWidth: true
                                }
                            }
                            GridLayout {
                                visible: !root.entryIsAudio
                                Layout.fillWidth: true
                                columns: 2

                                Label {
                                    text: qsTr("Default:")
                                }
                                CheckBox {
                                    text: qsTr("Audio on (for nodes set to \"Default\" below)")
                                    checked: !!root.entry && root.rev >= 0 && root.entry.audio
                                    onToggled: { root.entry.audio = checked; root.structureChanged(); }
                                }
                                Label {
                                    text: qsTr("Separate audio file:")
                                }
                                FileField {
                                    text: root.entry ? root.entry.audioFile : ""
                                    filters: root.audioFilters
                                    dialogTitle: qsTr("Choose Default Audio File")
                                    placeholder: qsTr("Optional; otherwise the video's own audio track")
                                    onChosen: path => { root.entry.audioFile = path; root.structureChanged(); }
                                    onEdited: path => { root.entry.audioFile = path; root.changed(); }
                                }
                            }

                            // Per-node table
                            SectionHeader {
                                text: qsTr("Per node")
                            }
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                color: Kirigami.Theme.disabledTextColor
                                text: root.entryIsAudio ? qsTr("Give an audio file to each node that should play this stream. Nodes left empty play nothing for it.") : qsTr("Video file per node. Audio per node overrides the default; the audio file per node overrides the default audio file.")
                            }
                            Label {
                                visible: root.nodeIds.length === 0
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                color: Kirigami.Theme.neutralTextColor
                                text: qsTr("No nodes yet. Add node ids above, or list them in data/nodes.json.")
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                visible: root.nodeIds.length > 0

                                RowLayout {
                                    Layout.fillWidth: true
                                    Label {
                                        Layout.preferredWidth: root.nodeColumnWidth
                                        text: qsTr("Node")
                                        font.bold: true
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        Layout.horizontalStretchFactor: 3
                                        text: root.entryIsAudio ? qsTr("Audio file") : qsTr("Video file")
                                        font.bold: true
                                    }
                                    Label {
                                        Layout.preferredWidth: root.entryIsAudio ? Kirigami.Units.iconSizes.medium : root.audioColumnWidth
                                        text: root.entryIsAudio ? qsTr("Plays") : qsTr("Audio")
                                        font.bold: true
                                    }
                                    Label {
                                        visible: !root.entryIsAudio
                                        Layout.fillWidth: true
                                        Layout.horizontalStretchFactor: 2
                                        text: qsTr("Separate audio file")
                                        font.bold: true
                                    }
                                }

                                Repeater {
                                    model: root.rev >= 0 && root.entry ? root.nodeIds : []

                                    delegate: RowLayout {
                                        id: rowItem

                                        required property string modelData
                                        readonly property var e: root.entry

                                        Layout.fillWidth: true

                                        Label {
                                            Layout.preferredWidth: root.nodeColumnWidth
                                            elide: Text.ElideRight
                                            text: rowItem.modelData
                                        }
                                        FileField {
                                            Layout.horizontalStretchFactor: 3
                                            text: root.nodeValue(rowItem.e, rowItem.modelData, "file", "")
                                            filters: root.entryIsAudio ? root.audioFilters : root.videoFilters
                                            dialogTitle: qsTr("Choose File for %1").arg(rowItem.modelData)
                                            placeholder: {
                                                var t = rowItem.e ? root.resolvedFile(rowItem.e, rowItem.modelData) : "";
                                                return t !== "" ? qsTr("Template: %1").arg(t) : qsTr("(nothing on this node)");
                                            }
                                            onChosen: path => { root.setNodeValue(rowItem.e, rowItem.modelData, "file", path); root.rev++; }
                                            onEdited: path => root.setNodeValue(rowItem.e, rowItem.modelData, "file", path)
                                        }
                                        ComboBox {
                                            visible: !root.entryIsAudio
                                            Layout.preferredWidth: root.audioColumnWidth
                                            model: [rowItem.e && rowItem.e.audio ? qsTr("Default (on)") : qsTr("Default (off)"), qsTr("On"), qsTr("Off")]
                                            currentIndex: Math.max(0, ["default", "on", "off"].indexOf(root.nodeValue(rowItem.e, rowItem.modelData, "audio", "default")))
                                            onActivated: { root.setNodeValue(rowItem.e, rowItem.modelData, "audio", ["default", "on", "off"][currentIndex]); root.rev++; }
                                        }
                                        Kirigami.Icon {
                                            visible: root.entryIsAudio
                                            Layout.preferredWidth: Kirigami.Units.iconSizes.medium
                                            Layout.preferredHeight: Kirigami.Units.iconSizes.small
                                            source: rowItem.e && root.resolvedFile(rowItem.e, rowItem.modelData) !== "" ? "audio-volume-high" : "audio-volume-muted"
                                        }
                                        FileField {
                                            visible: !root.entryIsAudio
                                            Layout.horizontalStretchFactor: 2
                                            enabled: !!rowItem.e && root.audioOnNode(rowItem.e, rowItem.modelData)
                                            text: root.nodeValue(rowItem.e, rowItem.modelData, "audioFile", "")
                                            filters: root.audioFilters
                                            dialogTitle: qsTr("Choose Audio File for %1").arg(rowItem.modelData)
                                            placeholder: rowItem.e && rowItem.e.audioFile !== "" ? qsTr("Default: %1").arg(root.fileName(rowItem.e.audioFile)) : qsTr("Video's own audio")
                                            onChosen: path => { root.setNodeValue(rowItem.e, rowItem.modelData, "audioFile", path); root.rev++; }
                                            onEdited: path => root.setNodeValue(rowItem.e, rowItem.modelData, "audioFile", path)
                                        }
                                    }
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                color: Kirigami.Theme.disabledTextColor
                                visible: !!root.entry && Object.keys(root.entry.extra || {}).length > 0
                                text: root.entry ? qsTr("Kept from the file without changes: %1").arg(Object.keys(root.entry.extra || {}).join(", ")) : ""
                            }
                        }

                        Item {
                            Layout.fillHeight: true
                        }
                    }
                }
            }

            // Lower part: audio routing, checks and JSON preview
            ColumnLayout {
                SplitView.preferredHeight: 230
                SplitView.minimumHeight: 120
                spacing: 0

                TabBar {
                    id: bottomTabs
                    Layout.fillWidth: true

                    TabButton {
                        text: qsTr("Audio routing")
                    }
                    TabButton {
                        readonly property int problems: root.issues.filter(function (i) { return i.level !== "info"; }).length
                        text: problems > 0 ? qsTr("Checks (%1)").arg(problems) : qsTr("Checks")
                    }
                    TabButton {
                        text: qsTr("JSON")
                    }
                    onCurrentIndexChanged: {
                        if (currentIndex === 2)
                            jsonText.text = backend.toJson(root.comp);
                    }
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: bottomTabs.currentIndex

                    // Audio routing: where each audio source ends up
                    ScrollView {
                        clip: true
                        ListView {
                            model: root.routing
                            delegate: RowLayout {
                                required property var modelData
                                width: ListView.view.width
                                spacing: Kirigami.Units.largeSpacing

                                Kirigami.Icon {
                                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                                    Layout.preferredHeight: Kirigami.Units.iconSizes.small
                                    source: modelData.sources.length === 0 ? "audio-volume-muted" : "audio-volume-high"
                                }
                                Label {
                                    Layout.preferredWidth: 120
                                    font.bold: modelData.isMaster
                                    text: modelData.target
                                }
                                Label {
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                    text: modelData.sources.length === 0 ? qsTr("no audio") : modelData.sources.join(" + ")
                                    color: modelData.sources.length === 0 ? Kirigami.Theme.disabledTextColor : (modelData.sources.length > 1 ? Kirigami.Theme.neutralTextColor : Kirigami.Theme.textColor)
                                }
                            }
                        }
                    }

                    // Checks
                    ScrollView {
                        clip: true
                        ListView {
                            model: root.issues
                            delegate: RowLayout {
                                required property var modelData
                                width: ListView.view.width

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
                    }

                    // JSON preview
                    ScrollView {
                        clip: true
                        TextArea {
                            id: jsonText
                            readOnly: true
                            selectByMouse: true
                            font.family: "monospace"
                            wrapMode: TextEdit.NoWrap
                        }
                    }
                }
            }
        }
    }
}
