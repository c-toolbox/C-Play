/*
 * SPDX-FileCopyrightText:
 * 2021-2025 Erik Sunden <eriksunden85@gmail.com>
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

    property alias footerRow: footerRow
    property alias playPauseButton: playPauseButton
    property alias progressBar: progressBar
    property alias timeInfo: timeInfo

    anchors.bottom: parent.bottom
    anchors.left: parent.left
    anchors.right: parent.right
    hoverEnabled: true
    padding: 5
    position: ToolBar.Footer
    visible: UserInterfaceSettings.showFooter && !window.hideUI

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
    Component {
        id: toggleSlidesButton

        ToolButton {
            action: actions.toggleSlidesAction
        }
    }
    Component {
        id: toggleLayersButton

        ToolButton {
            action: actions.toggleLayersAction
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
        Loader {
            sourceComponent: toggleSlidesButton
            visible: PlaylistSettings.position === "right"
        }
        Loader {
            sourceComponent: toggleLayersButton
            visible: PlaylistSettings.position === "right"
        }
        ToolButton {
            id: playPauseButton

            action: actions.playPauseAction
            focusPolicy: Qt.NoFocus
            icon.name: "media-playback-start"
            text: ""

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

            focusPolicy: Qt.NoFocus
            icon.name: "media-playback-stop"
            text: ""

            onClicked: {
                mpv.performRewind();
            }

            ToolTip {
                id: rewindButtonToolTip

                text: PlaybackSettings.fadeDownBeforeRewind ? qsTr("Fade down then stop/rewind") : qsTr("Stop/rewind")
            }
        }
        LabelWithTooltip {
            id: timeInfo

            alwaysShowToolTip: true
            font.pointSize: 9
            fontSizeMode: Text.Fit
            horizontalAlignment: Qt.AlignHCenter
            text: app.formatTime(mpv.position) + " / " + app.formatTime(mpv.duration)
            toolTipFontSize: timeInfo.font.pointSize + 2
            toolTipText: qsTr("Remaining: ") + app.formatTime(mpv.remaining)
        }
        Loader {
            sourceComponent: togglePlaylistButton
            visible: PlaylistSettings.position === "right"
        }
        Loader {
            sourceComponent: toggleSectionsButton
            visible: PlaylistSettings.position === "right"
        }
        Loader {
            sourceComponent: toggleSlidesButton
            visible: PlaylistSettings.position === "left"
        }
        Loader {
            sourceComponent: toggleLayersButton
            visible: PlaylistSettings.position === "left"
        }
    }
}
