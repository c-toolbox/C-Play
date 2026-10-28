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

// Manages VDO.Ninja arena rooms (Arena-Call): remote callers publish their camera and/or screen
// into a slot of the room from their browser, and every slot stream becomes a WebRTC layer.
// The streams reach C-Play either directly through whep.vdo.ninja or through an own MediaMTX
// relay configured in Settings -> MediaMTX Streams.
Kirigami.ApplicationWindow {
    id: root

    color: Kirigami.Theme.alternateBackgroundColor
    height: 720
    title: qsTr("VDO.Ninja Arena Rooms")
    visible: false
    width: 900

    property int selectedRoomIndex: -1
    property int selectedStreamIndex: -1
    // A MediaMTX status fetch was started from this window (the MediaMTX model is shared with
    // the MediaMTX editor, so only react to fetches that were asked for here).
    property bool checkingOnline: false

    // The rooms are built with WebRTC support (BUILD_CPLAY_WITH_WEBRTC); the MediaMTX relay
    // additionally needs BUILD_CPLAY_WITH_MEDIA_MTX. Without them the models do not exist on app.
    property bool roomsAvailable: !!app.vdoNinjaRoomsModel && !!app.vdoNinjaSlotsModel
    property bool mediaMtxAvailable: !!app.mediaMtxServersModel && !!app.mediaMtxModel
    property bool selectedIsMediaMtx: transportCombo.currentIndex === 1

    onVisibilityChanged: {
        if (visible && root.roomsAvailable) {
            x = window.x + (window.width - width) / 2;
            y = Math.max(window.y, window.y + (window.height - height) / 2);
            if (root.mediaMtxAvailable)
                app.mediaMtxServersModel.updateServersList();
            app.vdoNinjaRoomsModel.updateRoomsList();
            if (app.vdoNinjaRoomsModel.numberOfRooms > 0) {
                root.selectRoom(0);
            } else {
                root.selectedRoomIndex = -1;
                root.clearRoomFields();
                app.vdoNinjaSlotsModel.setRoom(-1);
            }
        }
    }

    Connections {
        target: root.mediaMtxAvailable ? app.mediaMtxModel : null
        function onStreamsListChanged() {
            if (!root.checkingOnline)
                return;
            root.checkingOnline = false;
            app.vdoNinjaSlotsModel.setOnlinePaths(app.mediaMtxModel.onlineStreamNames());
            statusLabel.text = qsTr("%1 of %2 stream(s) live").arg(app.vdoNinjaSlotsModel.onlineCount).arg(app.vdoNinjaSlotsModel.numberOfStreams);
            statusLabel.color = "green";
        }
        function onResponseChanged() {
            if (root.checkingOnline && app.mediaMtxModel.lastError !== "") {
                root.checkingOnline = false;
                app.vdoNinjaSlotsModel.clearOnline();
                statusLabel.text = qsTr("Error: ") + app.mediaMtxModel.lastError;
                statusLabel.color = "red";
            }
        }
    }

    Timer {
        id: autoRefreshTimer
        interval: 5000
        repeat: true
        running: root.visible && autoRefreshCheckBox.checked && root.selectedIsMediaMtx
        onTriggered: root.checkOnline()
    }

    function clearRoomFields() {
        roomName.text = "";
        roomTitle.text = "Decision Arena";
        roomOrg.text = "LIU";
        roomRoom.text = "DecisionArena";
        roomKey.text = "";
        roomSlots.value = 8;
        roomArenaUrl.text = "";
        transportCombo.currentIndex = 0;
        mediaMtxServerCombo.currentIndex = -1;
        mediaMtxPublicHost.text = "";
        mediaMtxPublishAuth.text = "";
        roomEnabled.checked = true;
    }

    function loadRoom(index) {
        if (index < 0 || index >= app.vdoNinjaRoomsModel.numberOfRooms)
            return;
        var r = app.vdoNinjaRoomsModel.roomAt(index);
        roomName.text = r.name;
        roomTitle.text = r.title;
        roomOrg.text = r.org;
        roomRoom.text = r.room;
        roomKey.text = r.key;
        roomSlots.value = r.slots;
        roomArenaUrl.text = r.arenaUrl;
        transportCombo.currentIndex = r.transport === "mediamtx" ? 1 : 0;
        mediaMtxServerCombo.currentIndex = root.mediaMtxServerIndex(r.mediaMtxServer);
        mediaMtxPublicHost.text = r.mediaMtxPublicHost;
        mediaMtxPublishAuth.text = r.mediaMtxPublishAuth;
        roomEnabled.checked = r.enabled;
    }

    function selectRoom(index) {
        root.selectedRoomIndex = index;
        roomsList.currentIndex = index;
        root.loadRoom(index);
        root.rebuildSlots();
    }

    function roomFromFields() {
        return {
            name: roomName.text,
            title: roomTitle.text,
            org: roomOrg.text,
            room: roomRoom.text,
            key: roomKey.text,
            slots: roomSlots.value,
            arenaUrl: roomArenaUrl.text,
            transport: transportCombo.currentIndex === 1 ? "mediamtx" : "direct",
            mediaMtxServer: mediaMtxServerCombo.currentIndex >= 0 ? mediaMtxServerCombo.currentText : "",
            mediaMtxPublicHost: mediaMtxPublicHost.text,
            mediaMtxPublishAuth: mediaMtxPublishAuth.text,
            enabled: roomEnabled.checked
        };
    }

    function mediaMtxServerIndex(name) {
        if (!root.mediaMtxAvailable || name === "")
            return -1;
        for (var i = 0; i < app.mediaMtxServersModel.numberOfServers; i++) {
            if (app.mediaMtxServersModel.serverAt(i).name === name)
                return i;
        }
        return -1;
    }

    // WebRTC base URL of the room's MediaMTX server as C-Play reaches it (usually the LAN address,
    // while callers publish to the public host). Empty for direct rooms.
    function mediaMtxWhepBase(index) {
        var r = app.vdoNinjaRoomsModel.roomAt(index);
        if (r.transport !== "mediamtx")
            return "";
        var serverIndex = root.mediaMtxServerIndex(r.mediaMtxServer);
        if (serverIndex < 0)
            return "";
        var s = app.mediaMtxServersModel.serverAt(serverIndex);
        return s.webrtcScheme + "://" + s.host + ":" + s.webrtcPort;
    }

    function rebuildSlots() {
        root.selectedStreamIndex = -1;
        layerTitleField.text = "";
        if (root.selectedRoomIndex < 0) {
            app.vdoNinjaSlotsModel.setRoom(-1);
            return;
        }
        app.vdoNinjaSlotsModel.setRoom(root.selectedRoomIndex, root.mediaMtxWhepBase(root.selectedRoomIndex));
        if (app.vdoNinjaRoomsModel.roomAt(root.selectedRoomIndex).transport === "mediamtx") {
            statusLabel.text = "";
            root.checkOnline();
        } else {
            statusLabel.text = qsTr("direct via whep.vdo.ninja, live state unknown until a layer connects");
            statusLabel.color = Kirigami.Theme.disabledTextColor;
        }
    }

    function checkOnline() {
        if (!root.mediaMtxAvailable || root.selectedRoomIndex < 0 || app.mediaMtxModel.refreshInProgress)
            return;
        var serverIndex = root.mediaMtxServerIndex(app.vdoNinjaRoomsModel.roomAt(root.selectedRoomIndex).mediaMtxServer);
        if (serverIndex < 0) {
            statusLabel.text = qsTr("Select the MediaMTX server C-Play pulls the streams from");
            statusLabel.color = "orange";
            return;
        }
        root.checkingOnline = true;
        app.mediaMtxModel.refresh(serverIndex);
    }

    function webRtcLayerType() {
        var m = app.slides.selected.layersTypeModel;
        for (var i = 0; i < m.rowCount(); i++) {
            if (m.data(m.index(i, 0), Qt.UserRole) === "WebRTC")
                return i + 1;
        }
        return -1;
    }

    // Adds one WebRTC layer pulling the given slot stream to the selected slide.
    function addStreamLayer(title, whepUrl) {
        var layerType = root.webRtcLayerType();
        if (layerType < 0 || whepUrl === "")
            return false;
        layerView.layerItem.layerIdx = app.slides.selected.addLayer(title, layerType, whepUrl,
            PresentationSettings.defaultStereoModeForLayers, PresentationSettings.defaultGridModeForLayers);
        if (layerView.layerItem.layerIdx < 0)
            return false;
        // A MediaMTX relay with authentication: attach its HTTP Basic auth to the layer, like the
        // MediaMTX editor does. Direct whep.vdo.ninja endpoints need no credentials.
        var r = app.vdoNinjaRoomsModel.roomAt(root.selectedRoomIndex);
        if (r.transport === "mediamtx") {
            var serverIndex = root.mediaMtxServerIndex(r.mediaMtxServer);
            if (serverIndex >= 0) {
                var creds = app.mediaMtxServersModel.authCredentials(serverIndex);
                if (creds.username !== "") {
                    layerView.layerItem.layerWhepAuthUsername = creds.username;
                    layerView.layerItem.layerWhepAuthPassword = creds.password;
                }
            }
        }
        return true;
    }

    function copyText(text, what) {
        if (text === "") {
            statusLabel.text = qsTr("Set the Arena-Call URL of the room first");
            statusLabel.color = "orange";
            return;
        }
        app.copyToClipboard(text);
        statusLabel.text = qsTr("%1 copied to clipboard").arg(what);
        statusLabel.color = "green";
    }

    function openLink(url) {
        if (url === "") {
            statusLabel.text = qsTr("Set the Arena-Call URL of the room first");
            statusLabel.color = "orange";
            return;
        }
        Qt.openUrlExternally(url);
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8
        visible: root.roomsAvailable

        RowLayout {
            Layout.fillWidth: true

            Rectangle {
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
                width: Kirigami.Units.gridUnit
            }
            Kirigami.Heading {
                text: qsTr("Arena Rooms")
            }
            Rectangle {
                Layout.fillWidth: true
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            Layout.minimumHeight: 30
            Layout.maximumHeight: 90
            spacing: 8

            ColumnLayout {
                Layout.alignment: Qt.AlignTop
                spacing: 4

                ToolButton {
                    icon.name: "go-up"
                    enabled: root.selectedRoomIndex > 0
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Move up")
                    onClicked: {
                        app.vdoNinjaRoomsModel.moveRoom(root.selectedRoomIndex, root.selectedRoomIndex - 1);
                        root.selectRoom(root.selectedRoomIndex - 1);
                    }
                }
                ToolButton {
                    icon.name: "go-down"
                    enabled: root.roomsAvailable && root.selectedRoomIndex >= 0 && root.selectedRoomIndex < app.vdoNinjaRoomsModel.numberOfRooms - 1
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Move down")
                    onClicked: {
                        app.vdoNinjaRoomsModel.moveRoom(root.selectedRoomIndex, root.selectedRoomIndex + 1);
                        root.selectRoom(root.selectedRoomIndex + 1);
                    }
                }
            }

            ListView {
                id: roomsList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: root.roomsAvailable ? app.vdoNinjaRoomsModel : null
                currentIndex: root.selectedRoomIndex

                delegate: ItemDelegate {
                    width: roomsList.width
                    highlighted: index === root.selectedRoomIndex
                    contentItem: RowLayout {
                        Label {
                            Layout.fillWidth: true
                            text: model.name
                            elide: Text.ElideRight
                            color: model.enabled ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                        }
                        Label {
                            text: model.org + "/" + model.room
                            color: Kirigami.Theme.disabledTextColor
                        }
                        Label {
                            text: qsTr("%1 slots").arg(model.slots)
                            color: Kirigami.Theme.disabledTextColor
                        }
                        Label {
                            text: model.transport === "mediamtx" ? qsTr("MediaMTX") + (model.mediaMtxServer !== "" ? " (" + model.mediaMtxServer + ")" : "") : qsTr("direct")
                            color: Kirigami.Theme.disabledTextColor
                        }
                    }
                    onClicked: root.selectRoom(index)
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
                id: roomName
                Layout.fillWidth: true
                placeholderText: qsTr("Room name in C-Play")
            }
            Label { text: qsTr("Title:"); Layout.alignment: Qt.AlignRight }
            TextField {
                id: roomTitle
                Layout.fillWidth: true
                placeholderText: qsTr("Shown to the callers and used for layer titles")
            }

            Label { text: qsTr("Organisation:"); Layout.alignment: Qt.AlignRight }
            TextField {
                id: roomOrg
                Layout.fillWidth: true
                placeholderText: "LIU"
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Globally unique organisation name, part of the VDO.Ninja room id")
            }
            Label { text: qsTr("Room:"); Layout.alignment: Qt.AlignRight }
            TextField {
                id: roomRoom
                Layout.fillWidth: true
                placeholderText: "DecisionArena"
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Room name, unique per organisation")
            }

            Label { text: qsTr("Key:"); Layout.alignment: Qt.AlignRight }
            TextField {
                id: roomKey
                Layout.fillWidth: true
                placeholderText: qsTr("Optional secret")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Mixed into the room and stream ids, so they cannot be guessed from the organisation and room name alone. It is part of the caller links.")
            }
            Label { text: qsTr("Slots:"); Layout.alignment: Qt.AlignRight }
            DeferredSpinBox {
                id: roomSlots
                from: 1
                to: 32
                value: 8
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Number of callers that can be connected at the same time")
            }

            Label { text: qsTr("Arena-Call URL:"); Layout.alignment: Qt.AlignRight }
            TextField {
                id: roomArenaUrl
                Layout.fillWidth: true
                Layout.columnSpan: 3
                placeholderText: "https://arena.example.org/"
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Address of the deployed Arena-Call page, used to build the caller links")
            }

            Label { text: qsTr("Transport:"); Layout.alignment: Qt.AlignRight }
            ComboBox {
                id: transportCombo
                Layout.fillWidth: true
                model: root.mediaMtxAvailable ? [qsTr("Direct (whep.vdo.ninja)"), qsTr("MediaMTX relay")] : [qsTr("Direct (whep.vdo.ninja)")]
                currentIndex: 0
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Direct: each caller's browser serves WHEP through whep.vdo.ninja, no own server needed. MediaMTX: callers publish WHIP to an own MediaMTX server, which C-Play pulls from.")
            }
            Label { text: qsTr("Enabled:"); Layout.alignment: Qt.AlignRight }
            CheckBox {
                id: roomEnabled
                checked: true
            }

            Label {
                text: qsTr("MediaMTX server:")
                Layout.alignment: Qt.AlignRight
                visible: root.selectedIsMediaMtx
            }
            ComboBox {
                id: mediaMtxServerCombo
                Layout.fillWidth: true
                visible: root.selectedIsMediaMtx
                model: root.mediaMtxAvailable ? app.mediaMtxServersModel : null
                textRole: "name"
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Server C-Play pulls the streams from (configured in Settings -> MediaMTX Streams)")
            }
            Label {
                text: qsTr("Public host:")
                Layout.alignment: Qt.AlignRight
                visible: root.selectedIsMediaMtx
            }
            TextField {
                id: mediaMtxPublicHost
                Layout.fillWidth: true
                visible: root.selectedIsMediaMtx
                placeholderText: "mediamtx.example.org:8889"
                ToolTip.visible: hovered
                ToolTip.text: qsTr("host[:port] the callers' browsers publish WHIP to. Must be reachable from the Internet over HTTPS (webrtcEncryption) and allow publishing to the arena/ paths.")
            }

            Label {
                text: qsTr("Publish auth:")
                Layout.alignment: Qt.AlignRight
                visible: root.selectedIsMediaMtx
            }
            TextField {
                id: mediaMtxPublishAuth
                Layout.fillWidth: true
                Layout.columnSpan: 3
                visible: root.selectedIsMediaMtx
                placeholderText: qsTr("Optional user:pass, publish-only on arena/ paths")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("MediaMTX credentials the callers publish with. They become part of every caller link, so only grant them the publish permission on ~^arena/.")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                text: qsTr("Add New")
                icon.name: "list-add"
                enabled: roomName.text !== "" && roomOrg.text !== "" && roomRoom.text !== ""
                onClicked: {
                    app.vdoNinjaRoomsModel.addRoom(root.roomFromFields());
                    root.selectRoom(app.vdoNinjaRoomsModel.numberOfRooms - 1);
                }
            }
            Button {
                text: qsTr("Update")
                icon.name: "document-save"
                enabled: root.selectedRoomIndex >= 0 && roomName.text !== "" && roomOrg.text !== "" && roomRoom.text !== ""
                onClicked: {
                    app.vdoNinjaRoomsModel.updateRoom(root.selectedRoomIndex, root.roomFromFields());
                    root.selectRoom(root.selectedRoomIndex);
                }
            }
            Button {
                text: qsTr("Remove")
                icon.name: "list-remove"
                enabled: root.selectedRoomIndex >= 0
                onClicked: {
                    app.vdoNinjaRoomsModel.removeRoom(root.selectedRoomIndex);
                    root.selectedRoomIndex = -1;
                    root.clearRoomFields();
                    root.rebuildSlots();
                }
            }
            Item { Layout.fillWidth: true }
            Button {
                text: qsTr("Copy Invite Link")
                icon.name: "edit-copy"
                enabled: root.selectedRoomIndex >= 0
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Arena-Call page listing all slots of this room, to share with the callers")
                onClicked: root.copyText(app.vdoNinjaRoomsModel.inviteUrl(root.selectedRoomIndex, false), qsTr("Invite link"))
            }
            Button {
                text: qsTr("Operator Page")
                icon.name: "internet-services"
                enabled: root.selectedRoomIndex >= 0
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Opens the Arena-Call operator page (director, return feed and WHEP endpoints) in the browser")
                onClicked: root.openLink(app.vdoNinjaRoomsModel.inviteUrl(root.selectedRoomIndex, true))
            }
            Button {
                text: qsTr("Director")
                icon.name: "view-media-artist"
                enabled: root.selectedRoomIndex >= 0
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Opens the VDO.Ninja director view of the room in the browser, to mute, remove or talk to callers")
                onClicked: root.openLink(app.vdoNinjaRoomsModel.directorUrl(root.selectedRoomIndex))
            }
            Button {
                text: qsTr("Return Feed")
                icon.name: "video-display"
                enabled: root.selectedRoomIndex >= 0
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Opens a screen share sender in the browser whose picture all callers see (e.g. the arena view)")
                onClicked: root.openLink(app.vdoNinjaRoomsModel.returnFeedUrl(root.selectedRoomIndex))
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
                text: qsTr("Slot Streams")
            }
            Label {
                id: statusLabel
                text: ""
                font.bold: true
                elide: Text.ElideRight
                Layout.maximumWidth: root.width / 2
            }
            Rectangle {
                Layout.fillWidth: true
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
            }
            CheckBox {
                id: autoRefreshCheckBox
                text: qsTr("Auto refresh")
                visible: root.selectedIsMediaMtx
                checked: false
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Polls the MediaMTX server every 5 seconds for live slot streams")
            }
            Button {
                text: root.mediaMtxAvailable && app.mediaMtxModel.refreshInProgress ? qsTr("Checking...") : qsTr("Check Live")
                icon.name: "view-refresh"
                visible: root.selectedIsMediaMtx
                enabled: root.selectedRoomIndex >= 0 && root.mediaMtxAvailable && !app.mediaMtxModel.refreshInProgress
                onClicked: root.checkOnline()
            }
        }

        ListView {
            id: streamsList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 100
            clip: true
            model: root.roomsAvailable ? app.vdoNinjaSlotsModel : null
            currentIndex: root.selectedStreamIndex

            delegate: ItemDelegate {
                width: streamsList.width
                highlighted: index === root.selectedStreamIndex
                contentItem: RowLayout {
                    Rectangle {
                        width: 10
                        height: 10
                        radius: 5
                        color: model.online === 1 ? "green" : (model.online === 0 ? "red" : Kirigami.Theme.disabledTextColor)
                        ToolTip.visible: dotMouse.containsMouse
                        ToolTip.text: model.online === 1 ? qsTr("live") : (model.online === 0 ? qsTr("not published") : qsTr("unknown"))
                        MouseArea {
                            id: dotMouse
                            anchors.fill: parent
                            hoverEnabled: true
                        }
                    }
                    Label {
                        Layout.preferredWidth: 40
                        text: model.slot
                        font.bold: true
                    }
                    Label {
                        Layout.preferredWidth: 60
                        text: model.kind
                        color: Kirigami.Theme.disabledTextColor
                    }
                    Label {
                        Layout.fillWidth: true
                        text: model.whepUrl !== "" ? model.whepUrl : qsTr("(select the MediaMTX server)")
                        color: Kirigami.Theme.disabledTextColor
                        elide: Text.ElideMiddle
                    }
                }
                onClicked: {
                    root.selectedStreamIndex = index;
                    layerTitleField.text = model.layerTitle;
                }
            }

            Kirigami.PlaceholderMessage {
                anchors.centerIn: parent
                visible: root.roomsAvailable && streamsList.count === 0
                icon.name: "im-user-online"
                icon.height: Kirigami.Units.iconSizes.medium
                text: qsTr("No room selected")
                explanation: qsTr("Add a room above, then share its invite link with the callers.")
            }

            ScrollBar.vertical: ScrollBar {}
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label { text: qsTr("Title:") }
            TextField {
                id: layerTitleField
                Layout.fillWidth: true
                placeholderText: qsTr("Title used for the layer")
                enabled: root.selectedStreamIndex >= 0
            }
            ComboBox {
                id: callerShareCombo
                model: [qsTr("Camera + Screen"), qsTr("Camera"), qsTr("Screen")]
                currentIndex: 0
                ToolTip.visible: hovered
                ToolTip.text: qsTr("What the copied caller link asks the caller to share")
            }
            Button {
                text: qsTr("Copy Caller Link")
                icon.name: "edit-copy"
                enabled: root.selectedStreamIndex >= 0
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Personal Arena-Call link for the slot of the selected stream")
                onClicked: {
                    var share = ["both", "camera", "screen"][callerShareCombo.currentIndex];
                    root.copyText(app.vdoNinjaRoomsModel.callerUrl(root.selectedRoomIndex,
                        app.vdoNinjaSlotsModel.rowAt(root.selectedStreamIndex).slot, share), qsTr("Caller link"));
                }
            }
            Button {
                text: qsTr("Copy WHEP URL")
                icon.name: "edit-copy"
                enabled: root.selectedStreamIndex >= 0
                onClicked: root.copyText(app.vdoNinjaSlotsModel.rowAt(root.selectedStreamIndex).whepUrl, qsTr("WHEP URL"))
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Item { Layout.fillWidth: true }
            ComboBox {
                id: addAllKindCombo
                model: [qsTr("Cameras and screens"), qsTr("Cameras only"), qsTr("Screens only")]
                currentIndex: 0
            }
            Button {
                text: qsTr("Add All As Layers")
                icon.name: "layer-new"
                enabled: root.selectedRoomIndex >= 0 && app.vdoNinjaSlotsModel.numberOfStreams > 0 && app.slides.selected.layersEnabled
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Adds one WebRTC layer per slot stream to the selected slide. Layers of slots without a caller keep retrying until the caller joins.")
                onClicked: {
                    if (root.webRtcLayerType() < 0) {
                        statusLabel.text = qsTr("WebRTC layers are not available in this build");
                        statusLabel.color = "red";
                        return;
                    }
                    var wanted = ["", "camera", "screen"][addAllKindCombo.currentIndex];
                    var added = 0;
                    for (var i = 0; i < app.vdoNinjaSlotsModel.numberOfStreams; i++) {
                        var row = app.vdoNinjaSlotsModel.rowAt(i);
                        if (wanted !== "" && row.kind !== wanted)
                            continue;
                        if (root.addStreamLayer(row.layerTitle, row.whepUrl))
                            added++;
                    }
                    app.slides.updateSelectedSlide();
                    statusLabel.text = qsTr("%1 layer(s) added to the selected slide").arg(added);
                    statusLabel.color = added > 0 ? "green" : "red";
                }
            }
            Button {
                text: qsTr("Add As Layer")
                icon.name: "layer-new"
                enabled: root.selectedStreamIndex >= 0 && layerTitleField.text !== "" && app.slides.selected.layersEnabled
                onClicked: {
                    if (root.webRtcLayerType() < 0) {
                        statusLabel.text = qsTr("WebRTC layers are not available in this build");
                        statusLabel.color = "red";
                        return;
                    }
                    var row = app.vdoNinjaSlotsModel.rowAt(root.selectedStreamIndex);
                    if (root.addStreamLayer(layerTitleField.text, row.whepUrl)) {
                        app.slides.updateSelectedSlide();
                        statusLabel.text = qsTr("Layer added to the selected slide");
                        statusLabel.color = "green";
                    } else {
                        statusLabel.text = qsTr("Could not add the layer (is the MediaMTX server selected?)");
                        statusLabel.color = "red";
                    }
                }
            }
        }
    }
}
