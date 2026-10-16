/*
 * SPDX-FileCopyrightText:
 * 2021-2026 Erik Sunden <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

import "Components/PopupHelpers.js" as PopupHelpers

Rectangle {
    id: root

    property int bigFont: PlaylistSettings.bigFontFullscreen
    property bool isYouTubePlaylist: false
    property alias playlistName: playlistName
    property alias playlistView: playlistView
    property string position: PlaylistSettings.position
    property int rowHeight: PlaylistSettings.rowHeight
    property alias scrollPositionTimer: scrollPositionTimer
    property bool shouldBeVisible: true

    function setPlayListScrollPosition() {
        playlistView.positionViewAtIndex(playlistView.model.playingVideo, ListView.Beginning);
        updateEofModeButton();
    }
    function updateEofModeButton() {
        var eofMode = mpv.playlistModel.eofMode(playlistView.currentIndex);
        eof_pause.checked = (eofMode === 0);
        eof_next.checked = (eofMode === 1);
        eof_loop.checked = (eofMode === 2);
    }

    visible: shouldBeVisible && !window.hideUI
    color: Kirigami.Theme.backgroundColor
    height: mpv.height
    state: "hidden"
    width: {
        const w = Kirigami.Units.gridUnit * 15;
        return w;
    }
    x: position === "right" ? parent.width : -width
    y: 0
    z: position === "right" ? 40 : 41

    states: [
        State {
            name: "hidden"

            PropertyChanges {
                target: root
                x: position === "right" ? parent.width : -width
            }
            PropertyChanges {
                target: root
                shouldBeVisible: false
            }
        },
        State {
            name: "visible-without-partner"

            PropertyChanges {
                target: root
                x: position === "right" ? parent.width - root.width : 0
            }
            PropertyChanges {
                target: root
                shouldBeVisible: true
            }
        },
        State {
            name: "visible-with-partner"

            PropertyChanges {
                target: root
                x: position === "right" ? parent.width - (root.width * 2) : 0
            }
            PropertyChanges {
                target: root
                shouldBeVisible: true
            }
        }
    ]
    transitions: [
        Transition {
            from: "visible-without-partner"
            to: "hidden"

            SequentialAnimation {
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.InQuad
                    property: "x"
                    target: root
                }
                PropertyAction {
                    property: "shouldBeVisible"
                    target: root
                    value: false
                }
            }
        },
        Transition {
            from: "hidden"
            to: "visible-without-partner"

            SequentialAnimation {
                PropertyAction {
                    property: "shouldBeVisible"
                    target: root
                    value: true
                }
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.OutQuad
                    property: "x"
                    target: root
                }
            }
        },
        Transition {
            from: "visible-with-partner"
            to: "hidden"

            SequentialAnimation {
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.InQuad
                    property: "x"
                    target: root
                }
                PropertyAction {
                    property: "shouldBeVisible"
                    target: root
                    value: false
                }
            }
        },
        Transition {
            from: "hidden"
            to: "visible-with-partner"

            SequentialAnimation {
                PropertyAction {
                    property: "shouldBeVisible"
                    target: root
                    value: true
                }
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.OutQuad
                    property: "x"
                    target: root
                }
            }
        },
        Transition {
            from: "visible-without-partner"
            to: "visible-with-partner"

            SequentialAnimation {
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.OutQuad
                    property: "x"
                    target: root
                }
            }
        },
        Transition {
            from: "visible-with-partner"
            to: "visible-without-partner"

            SequentialAnimation {
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.OutQuad
                    property: "x"
                    target: root
                }
            }
        }
    ]

    ColumnLayout {
        id: playListHeader

        spacing: 10

        RowLayout {
            anchors.rightMargin: Kirigami.Units.largeSpacing
            spacing: 1

            
            Button {
                id: openPlaylistButton
                icon.name: "document-open"

                onClicked: {
                    openCPlayPlaylistDialog.open();
                }

                ToolTip {
                    text: qsTr("Open playlist")
                }
            }
            Button {
                icon.name: "document-replace"

                onClicked: {
                    clearPlaylistDialog.open();
                }

                ToolTip {
                    text: qsTr("New playlist")
                }
                Dialog {
                    id: clearPlaylistDialog
                    standardButtons: Dialog.Ok | Dialog.Cancel

                    onOpened: PopupHelpers.handlePopupOpen()
                    onClosed: PopupHelpers.handlePopupClose()

                    Label {
                        text: "Confirm clearing of all items and start a new playlist."
                    }

                    onAccepted: {
                        mpv.clearPlaylist();
                    }
                }
            }
            Button {
                icon.name: "list-add"

                onClicked: {
                    addToPlaylistDialog.open();
                }

                ToolTip {
                    text: qsTr("Add to playlist")
                }
            }
            Button {
                icon.name: "list-remove"

                onClicked: {
                    mpv.playlistModel.removeItem(playlistView.currentIndex);
                }

                ToolTip {
                    text: qsTr("Remove from playlist")
                }
            }
            Button {
                icon.name: "view-form"

                onClicked: {
                    var idx = playlistView.currentIndex;
                    if (idx < 0) {
                        idx = mpv.playlistModel.getPlayingVideo();
                        if (idx < 0) idx = 0;
                    }
                    if (idx >= 0 && idx < mpv.playlistModel.getPlayListSize()) {
                        playlistView.currentIndex = idx;
                        viewPlaylistItemWindow.updateValues(idx);
                    }
                    viewPlaylistItemWindow.visible = true;
                }

                ToolTip {
                    text: qsTr("View playlist entry")
                }
            }
            Button {
                icon.name: "pan-up-symbolic"

                onClicked: {
                    mpv.playlistModel.moveItemUp(playlistView.currentIndex);
                }

                ToolTip {
                    text: qsTr("Move selected upwards")
                }
            }
            Button {
                icon.name: "pan-down-symbolic"

                onClicked: {
                    mpv.playlistModel.moveItemDown(playlistView.currentIndex);
                }

                ToolTip {
                    text: qsTr("Move selected downwards")
                }
            }
            Button {
                id: eofMenuButton

                focusPolicy: Qt.NoFocus
                icon.name: "media-playback-pause"

                onClicked: {
                    eofMenu.visible = !eofMenu.visible;
                }

                ToolTip {
                    text: qsTr("\"End of file\" action")
                }
                Menu {
                    id: eofMenu

                    onOpened: PopupHelpers.handlePopupOpen()
                    onClosed: PopupHelpers.handlePopupClose()

                    y: parent.height

                    MenuSeparator {
                    }
                    ButtonGroup {
                        buttons: columnEOF.children
                    }
                    Column {
                        id: columnEOF

                        RadioButton {
                            id: eof_pause

                            checked: false
                            text: qsTr("EOF: Pause")

                            onCheckedChanged: {
                                if (checked)
                                    eofMenuButton.icon.name = "media-playback-pause";
                            }
                            onClicked: {
                                mpv.playlistModel.setEofMode(playlistView.currentIndex, 0);
                                mpv.playlistModel.updateItem(playlistView.currentIndex);
                            }
                        }
                        RadioButton {
                            id: eof_next

                            checked: false
                            text: qsTr("EOF: Next ")

                            onCheckedChanged: {
                                if (checked)
                                    eofMenuButton.icon.name = "go-next";
                            }
                            onClicked: {
                                mpv.playlistModel.setEofMode(playlistView.currentIndex, 1);
                                mpv.playlistModel.updateItem(playlistView.currentIndex);
                            }
                        }
                        RadioButton {
                            id: eof_loop

                            checked: true
                            text: qsTr("EOF: Loop ")

                            onCheckedChanged: {
                                if (checked)
                                    eofMenuButton.icon.name = "media-playlist-repeat";
                            }
                            onClicked: {
                                mpv.playlistModel.setEofMode(playlistView.currentIndex, 2);
                                mpv.playlistModel.updateItem(playlistView.currentIndex);
                            }
                        }
                    }
                }
            }
            Item {
                Layout.fillHeight: true
                // spacer item
                Layout.fillWidth: true
            }
        }
        RowLayout {
            ToolButton {
                id: autoPlayButton

                checkable: true
                checked: mpv.autoPlay
                icon.color: mpv.autoPlay ? "lime" : "crimson"
                icon.name: "media-playlist-play"

                onCheckedChanged: {
                    mpv.autoPlay = autoPlayButton.checked;
                }

                ToolTip {
                    id: autoPlayTooltip

                    text: {
                        mpv.autoPlay ? qsTr("Autoplay is ON") : qsTr("Autoplay is OFF");
                    }
                }
            }
            Label {
                id: playlistName

                font.pointSize: 9
                text: qsTr("Playlist")
            }
            Rectangle {
                Layout.fillWidth: true
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
                width: Kirigami.Units.gridUnit + 10
            }
            ToolButton {
                icon.color: mpv.playlistModel.playListIsEdited ? "orange" : "lime"
                icon.name: "system-save-session"

                onClicked: {
                    saveCPlayPlaylistDialog.currentFile = mpv.playlistModel.getPlayListPathAsURL();
                    saveCPlayPlaylistDialog.open();
                }

                ToolTip {
                    text: qsTr("Save playlist")
                }
            }
            Item {
                Layout.fillHeight: true
                // spacer item
                Layout.fillWidth: true
            }
        }
    }
    Connections {
        function onFileLoaded() {
            playlistView.currentIndex = -1;
        }
        function onPlaylistModelChanged() {
            if (mpv.playlistModel.getPlayListName() !== "")
                playlistName.text = qsTr("Playlist: ") + mpv.playlistModel.getPlayListName();
            else
                playlistName.text = qsTr("Playlist");
        }

        target: mpv
    }
    ScrollView {
        id: playlistScrollView

        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        anchors.fill: parent
        anchors.topMargin: playListHeader.height + 5
        clip: true
        z: 20

        ListView {
            id: playlistView

            delegate: playListItemCompact
            model: mpv.playlistModel
            spacing: 1
        }
    }

    // Drop files anywhere on the playlist panel. The insertion line shows where the new
    // items will land: between two rows, above the first or below the last. Hovering the
    // header or the bottom edge of the list scrolls it, so a full list can reach any row.
    DropArea {
        id: dropAreaPlaylist

        property int insertRow: -1
        property int scrollDirection: 0

        // Row pitch of the list (delegate height + spacing).
        function rowPitch() {
            var first = playlistView.itemAtIndex(0);
            return (first ? first.height : rowHeight) + playlistView.spacing;
        }
        function updateInsertRow(dragY) {
            var contentY = dropAreaPlaylist.mapToItem(playlistView.contentItem, 0, dragY).y;
            var row = Math.round(contentY / rowPitch());
            insertRow = Math.max(0, Math.min(playlistView.count, row));

            var edge = 25;
            if (dragY < playlistScrollView.y + edge)
                scrollDirection = -1;
            else if (dragY > height - edge)
                scrollDirection = 1;
            else
                scrollDirection = 0;
        }
        function resetDrag() {
            insertRow = -1;
            scrollDirection = 0;
        }

        anchors.fill: parent
        keys: ["text/uri-list"]
        z: 21

        onEntered: function(drag) {
            updateInsertRow(drag.y);
        }
        onPositionChanged: function(drag) {
            updateInsertRow(drag.y);
        }
        onExited: resetDrag()

        onDropped: function(drop) {
            var insertAt = insertRow >= 0 ? insertRow : playlistView.count;
            resetDrag();
            if (!drop.urls || drop.urls.length === 0)
                return;

            // Each file goes in after the previous one, so multiple files keep their dropped order.
            var lastAdded = -1;
            for(var i in drop.urls){
                var idx = mpv.addFileToPlaylist(drop.urls[i].toString(), insertAt);
                if(idx >= 0) {
                    lastAdded = idx;
                    insertAt = idx + 1;
                }
            }
            if(lastAdded >= 0)
                playlistView.positionViewAtIndex(lastAdded, ListView.Contain);
            mpv.focus = true;
        }

        Rectangle {
            id: dropInsertionLine

            visible: dropAreaPlaylist.containsDrag && dropAreaPlaylist.insertRow >= 0
            x: 0
            width: parent.width
            height: 4
            radius: 2
            color: Kirigami.Theme.highlightColor
            opacity: 0.95
            y: {
                if (dropAreaPlaylist.insertRow < 0)
                    return 0;
                // Keep a dependency on contentY so the line follows auto-scroll.
                var contentY = playlistView.contentY;
                var lineY = dropAreaPlaylist.insertRow * dropAreaPlaylist.rowPitch() - playlistView.spacing / 2;
                var localY = playlistView.contentItem.mapToItem(dropAreaPlaylist, 0, lineY).y - height / 2;
                return Math.max(playlistScrollView.y, Math.min(dropAreaPlaylist.height - height, localY));
            }
        }

        Timer {
            interval: 30
            repeat: true
            running: dropAreaPlaylist.containsDrag && dropAreaPlaylist.scrollDirection !== 0

            onTriggered: {
                var maxY = Math.max(playlistView.originY, playlistView.originY + playlistView.contentHeight - playlistView.height);
                playlistView.contentY = Math.max(playlistView.originY, Math.min(maxY, playlistView.contentY + dropAreaPlaylist.scrollDirection * 10));
                dropAreaPlaylist.updateInsertRow(dropAreaPlaylist.drag.y);
            }
        }
    }
    Component {
        id: playListItemCompact

        PlayListItemCompact {
            highlighted: playlistView.currentIndex === index

            onClicked: {
                playlistView.currentIndex = index;
                if (viewPlaylistItemWindow.visible) {
                    viewPlaylistItemWindow.updateValues(playlistView.currentIndex);
                }
                updateEofModeButton();
            }
        }
    }
    Timer {
        id: scrollPositionTimer

        interval: 50
        repeat: true
        running: true

        onTriggered: {
            setPlayListScrollPosition();
            scrollPositionTimer.stop();
        }
    }
}
