/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects
import QtQuick.Dialogs

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

Rectangle {
    id: root

    property int bigFont: PlaylistSettings.bigFontFullscreen
    property bool isYouTubePlaylist: false
    property alias playlistName: playlistName
    property alias playlistView: playlistView
    property string position: PlaylistSettings.position
    property int rowHeight: PlaylistSettings.rowHeight
    property alias scrollPositionTimer: scrollPositionTimer

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

    color: Kirigami.Theme.backgroundColor
    height: mpv.height
    state: "hidden"
    width: {
        const w = Kirigami.Units.gridUnit * 17;
        return (parent.width * 0.15) < w ? w : parent.width * 0.15;
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
                visible: false
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
                visible: true
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
                visible: true
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
                    property: "visible"
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
                    property: "visible"
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
                    property: "visible"
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
                    property: "visible"
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
                    viewPlaylistItemWindow.updateValues(playlistView.currentIndex);
                    viewPlaylistItemWindow.visible = true;
                }

                ToolTip {
                    text: qsTr("View playlist entry")
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
            Button {
                icon.color: "crimson"
                icon.name: "trash-empty"

                onClicked: {
                    clearPlaylistDialog.open();
                }

                ToolTip {
                    text: qsTr("Clear playlist")
                }
                MessageDialog {
                    id: clearPlaylistDialog

                    buttons: MessageDialog.Yes | MessageDialog.No
                    text: "Confirm clearing of all items in playlist."
                    title: "Clear playlist"

                    Component.onCompleted: visible = false
                    onAccepted: {
                        mpv.clearPlaylist();
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
            Rectangle {
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
                width: Kirigami.Units.gridUnit + 10
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
            Item {
                Layout.fillHeight: true
                // spacer item
                Layout.fillWidth: true
            }
            Button {
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
