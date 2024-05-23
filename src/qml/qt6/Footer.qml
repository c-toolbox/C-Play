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

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay


ToolBar {
    id: root

    property alias progressBar: progressBar
    property alias footerRow: footerRow
    property alias timeInfo: timeInfo
    property alias playPauseButton: playPauseButton

    anchors.left: parent.left
    anchors.right: parent.right
    anchors.bottom: isFullScreen() ? mpv.bottom : parent.bottom
    padding: 5
    position: ToolBar.Footer
    hoverEnabled: true
    visible: !window.isFullScreen() || mpv.mouseY > window.height - footer.height

    Component {
        id: toggleSectionsButton

        ToolButton {
            action: actions.toggleSectionsAction
        }
    }

    Component {
        id: togglePlaylistButton

        ToolButton {
            action: actions.togglePlaylistAction
        }
    }

    RowLayout {
        id: footerRow
        anchors.fill: parent

        Loader {
            sourceComponent: togglePlaylistButton
            visible: PlaylistSettings.position === "left"
        }

        Loader {
            sourceComponent: toggleSectionsButton
            visible: PlaylistSettings.position === "left"
        }

        ToolButton {
            id: playPauseButton
            action: actions.playPauseAction
            text: ""
            icon.name: "media-playback-start"
            focusPolicy: Qt.NoFocus

            ToolTip {
                id: playPauseButtonToolTip
                text: mpv.pause ? qsTr("Start Playback") : qsTr("Pause Playback")
            }
        }

        HProgressBar {
            id: progressBar
            Layout.fillWidth: true
        }

        ToolButton {
            id: rewindButton
            text: ""
            icon.name: "media-playback-stop"
            focusPolicy: Qt.NoFocus

            onClicked: {
                mpv.performRewind()
            }

            ToolTip {
                id: rewindButtonToolTip
                text: PlaybackSettings.fadeDownBeforeRewind ? qsTr("Fade down then stop/rewind") : qsTr("Stop/rewind")
            }
        }

        LabelWithTooltip {
            id: timeInfo

            text: app.formatTime(mpv.position) + " / " + app.formatTime(mpv.duration)
            font.pointSize: 9
            fontSizeMode: Text.Fit
            toolTipText: qsTr("Remaining: ") + app.formatTime(mpv.remaining)
            toolTipFontSize: timeInfo.font.pointSize + 2
            alwaysShowToolTip: true
            horizontalAlignment: Qt.AlignHCenter
        }

        Loader {
            sourceComponent: togglePlaylistButton
            visible: PlaylistSettings.position === "right"
        }

        Loader {
            sourceComponent: toggleSectionsButton
            visible: PlaylistSettings.position === "right"
        }

    }
}
