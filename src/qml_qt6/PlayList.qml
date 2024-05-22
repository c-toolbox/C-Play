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

    property alias scrollPositionTimer: scrollPositionTimer
    property alias playlistView: playlistView
    property alias playlistName: playlistName
    property string position: PlaylistSettings.position
    property int rowHeight: PlaylistSettings.rowHeight
    property int bigFont: PlaylistSettings.bigFontFullscreen
    property bool isYouTubePlaylist: false

    height: mpv.height
    width: {
            const w = Kirigami.Units.gridUnit * 19
            return (parent.width * 0.17) < w ? w : parent.width * 0.17
    }
    x: position === "right" ? parent.width : -width
    y: 0
    z: position === "right" ? 40 : 41
    state: "hidden"
    color: Kirigami.Theme.backgroundColor

    ColumnLayout{
        id: playListHeader
        spacing: 10

        RowLayout {
            spacing: 1
            anchors.rightMargin: Kirigami.Units.largeSpacing

            Button {
                icon.name: "list-add"
                onClicked: {
                    addToPlaylistDialog.open()
                }
                ToolTip {
                    text: qsTr("Add to playlist")
                }
            }
            Button {
                icon.name: "list-remove"
                onClicked: {
                    mpv.playlistModel.removeItem(playlistView.currentIndex)
                }
                ToolTip {
                    text: qsTr("Remove from playlist")
                }
            }
            Button {
                icon.name: "view-form"
                onClicked: {
                    viewPlaylistItemWindow.updateValues(playlistView.currentIndex)
                    viewPlaylistItemWindow.visible = true
                }
                ToolTip {
                    text: qsTr("View playlist entry")
                }
            }
            Button {
                id: eofMenuButton
                icon.name: "media-playback-pause"
                focusPolicy: Qt.NoFocus

                onClicked: {
                    eofMenu.visible = !eofMenu.visible
                }
                ToolTip {
                    text: qsTr("\"End of file\" action")
                }

                Menu {
                    id: eofMenu

                    y: parent.height

                    MenuSeparator {}

                    ButtonGroup {
                        buttons: columnEOF.children
                    }

                    Column {
                        id: columnEOF

                        RadioButton {
                            id: eof_pause
                            checked: false
                            text: qsTr("EOF: Pause")
                            onClicked: {
                                mpv.playlistModel.setEofMode(playlistView.currentIndex, 0)
                                mpv.playlistModel.updateItem(playlistView.currentIndex)
                            }
                            onCheckedChanged: {
                                if(checked)
                                    eofMenuButton.icon.name = "media-playback-pause";
                            }
                        }

                        RadioButton {
                            id: eof_next
                            checked: false
                            text: qsTr("EOF: Next ")
                            onClicked: {
                               mpv.playlistModel.setEofMode(playlistView.currentIndex, 1)
                               mpv.playlistModel.updateItem(playlistView.currentIndex)
                            }
                            onCheckedChanged: {
                                if(checked)
                                    eofMenuButton.icon.name = "go-next";
                            }
                        }

                        RadioButton {
                            id: eof_loop
                            checked: true
                            text: qsTr("EOF: Loop ")
                            onClicked: {
                                mpv.playlistModel.setEofMode(playlistView.currentIndex, 2)
                                mpv.playlistModel.updateItem(playlistView.currentIndex)
                            }
                            onCheckedChanged: {
                                if(checked)
                                    eofMenuButton.icon.name = "media-playlist-repeat";
                            }
                        }
                    }
                }
            }
            Button {
                icon.name: "kdenlive-zindex-up"
                onClicked: {
                    mpv.playlistModel.moveItemUp(playlistView.currentIndex)
                }
                ToolTip {
                    text: qsTr("Move selected upwards")
                }
            }
            Button {
                icon.name: "kdenlive-zindex-down"
                onClicked: {
                    mpv.playlistModel.moveItemDown(playlistView.currentIndex)
                }
                ToolTip {
                    text: qsTr("Move selected downwards")
                }
            }
            Button {
                icon.name: "system-save-session"
                icon.color: mpv.playlistModel.playListIsEdited ? "orange" : "lime"
                onClicked: {
                    saveCPlayPlaylistDialog.currentFile = mpv.playlistModel.getPlayListPathAsURL()
                    saveCPlayPlaylistDialog.open()
                }
                ToolTip {
                    text: qsTr("Save playlist")
                }
            }
            Button {
                icon.name: "trash-empty"
                icon.color: "crimson"
                onClicked: {
                    clearPlaylistDialog.open()
                }
                ToolTip {
                    text: qsTr("Clear playlist")
                }

                MessageDialog {
                    id: clearPlaylistDialog
                    title: "Clear playlist"
                    text: "Confirm clearing of all items in playlist."
                    buttons: MessageDialog.Yes | MessageDialog.No
                    onAccepted: {
                        mpv.clearPlaylist()
                    }
                    Component.onCompleted: visible = false
                }
            }
            Item {
                // spacer item
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }

        RowLayout {
            Rectangle {
                width: Kirigami.Units.gridUnit + 10
                height: 1
                color: Kirigami.Theme.alternateBackgroundColor
            }

            Label {
                id: playlistName
                text: qsTr("Playlist")
                font.pointSize: 9
            }

            Rectangle {
                width: Kirigami.Units.gridUnit + 10
                height: 1
                color: Kirigami.Theme.alternateBackgroundColor
                Layout.fillWidth: true
            }

            Item {
                // spacer item
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            Button {
                id: autoPlayButton
                checkable : true
                checked: mpv.autoPlay
                icon.name: "media-playlist-play"
                icon.color: mpv.autoPlay ? "lime" : "crimson"
                onCheckedChanged: {
                    mpv.autoPlay = autoPlayButton.checked
                }
                ToolTip {
                    id: autoPlayTooltip
                    text: {
                        mpv.autoPlay ? qsTr("Autoplay is ON") : qsTr("Autoplay is OFF")
                    }
                }
            }
        }
    }

    Connections {
        target: mpv
        function onPlaylistModelChanged() {
            if(mpv.playlistModel.getPlayListName() !== "")
                playlistName.text = qsTr("Playlist: ") + mpv.playlistModel.getPlayListName()
            else
                playlistName.text = qsTr("Playlist")
        }
        function onFileLoaded() {
            playlistView.currentIndex = -1
        }
    }

    ScrollView {
        id: playlistScrollView

        z: 20
        anchors.fill: parent
        anchors.topMargin: playListHeader.height + 5
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ListView {
            id: playlistView

            model: mpv.playlistModel
            spacing: 1
            delegate: playListItemCompact
        }
    }

    Component {
        id: playListItemCompact
        PlayListItemCompact {
            onClicked: {
                if(viewPlaylistItemWindow.visible) {
                    viewPlaylistItemWindow.updateValues(playlistView.currentIndex)
                }
                updateEofModeButton()
            }
        }
    }

    Timer {
        id: scrollPositionTimer
        interval: 50; running: true; repeat: true

        onTriggered: {
            setPlayListScrollPosition()
            scrollPositionTimer.stop()
        }
    }

    states: [
        State {
            name: "hidden"
            PropertyChanges { target: root; x: position === "right" ? parent.width : -width }
            PropertyChanges { target: root; visible: false }
        },
        State {
            name : "visible-without-partner"
            PropertyChanges { target: root; x: position === "right" ? parent.width - root.width : 0 }
            PropertyChanges { target: root; visible: true }
        },
        State {
            name : "visible-with-partner"
            PropertyChanges { target: root; x: position === "right" ? parent.width - (root.width * 2): 0 }
            PropertyChanges { target: root; visible: true }
        }
    ]

    transitions: [
        Transition {
            from: "visible-without-partner"
            to: "hidden"

            SequentialAnimation {
                NumberAnimation {
                    target: root
                    property: "x"
                    duration: 120
                    easing.type: Easing.InQuad
                }
                PropertyAction {
                    target: root
                    property: "visible"
                    value: false
                }
            }
        },
        Transition {
            from: "hidden"
            to: "visible-without-partner"

            SequentialAnimation {
                PropertyAction {
                    target: root
                    property: "visible"
                    value: true
                }
                NumberAnimation {
                    target: root
                    property: "x"
                    duration: 120
                    easing.type: Easing.OutQuad
                }
            }
        },
        Transition {
            from: "visible-with-partner"
            to: "hidden"

            SequentialAnimation {
                NumberAnimation {
                    target: root
                    property: "x"
                    duration: 120
                    easing.type: Easing.InQuad
                }
                PropertyAction {
                    target: root
                    property: "visible"
                    value: false
                }
            }
        },
        Transition {
            from: "hidden"
            to: "visible-with-partner"

            SequentialAnimation {
                PropertyAction {
                    target: root
                    property: "visible"
                    value: true
                }
                NumberAnimation {
                    target: root
                    property: "x"
                    duration: 120
                    easing.type: Easing.OutQuad
                }
            }
        },
        Transition {
            from: "visible-without-partner"
            to: "visible-with-partner"

            SequentialAnimation {
                NumberAnimation {
                    target: root
                    property: "x"
                    duration: 120
                    easing.type: Easing.OutQuad
                }
            }
        },
        Transition {
            from: "visible-with-partner"
            to: "visible-without-partner"

            SequentialAnimation {
                NumberAnimation {
                    target: root
                    property: "x"
                    duration: 120
                    easing.type: Easing.OutQuad
                }
            }
        }
    ]

    function updateEofModeButton() {
        var eofMode = mpv.playlistModel.eofMode(playlistView.currentIndex)
        eof_pause.checked = (eofMode === 0)
        eof_next.checked = (eofMode === 1)
        eof_loop.checked = (eofMode === 2)
    }

    function setPlayListScrollPosition() {
        playlistView.positionViewAtIndex(playlistView.model.playingVideo, ListView.Beginning)
        updateEofModeButton()
    }

}
