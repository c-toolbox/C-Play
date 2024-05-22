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

ItemDelegate {
    id: root

    property string rowNumber: (index + 1).toString()

    padding: 0
    font.pointSize: 9

    background: Rectangle {
        anchors.fill: parent
        color: {
            let color = Kirigami.Theme.backgroundColor
            Qt.hsla(color.hslHue, color.hslSaturation, color.hslLightness, 1)
        }
    }

    contentItem: Kirigami.IconTitleSubtitle {
        icon.name: model.isPlaying ? "kt-set-max-upload-speed" : ""
        icon.color: color
        title: mainText()
        subtitle: model.hasDescriptionFile === true ? (model.duration + " : " + model.stereoVideo + " " + model.gridToMapOn + " : (" + model.eofMode + ")") : (model.duration + " : (" + model.eofMode + ")")
        color: root.hovered || root.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
        ToolTip.text: title
        ToolTip.visible: root.hovered
    }

    Timer {
       id: playItem
       interval: PlaylistSettings.autoPlayAfterTime * 1000
       onTriggered: {
           mpv.pause = false
       }
    }

    onDoubleClicked: {
        mpv.pause = true
        mpv.position = 0
        mpv.loadItem(index)
        mpv.playlistModel.setPlayingVideo(index)
        if(mpv.autoPlay)
            playItem.start()
    }

    Connections {
        target: mpv
        function onPauseChanged() {
            if (model.isPlaying) {
                if (mpv.pause) {
                    root.icon = "media-playback-pause"
                }
                else {
                    root.icon = "media-playback-start"
                }
            }
            else if (root.icon !== "") {
                root.icon = ""
            }
        }
    }

    ToolTip {
        text: (PlaylistSettings.showMediaTitle ? model.title : model.name)
        visible: root.containsMouse
        font.pointSize: Kirigami.Units.gridUnit - 5
    }

    function mainText() {
        const rowNumber = pad(root.rowNumber, playlistView.count.toString().length) + ". "

        if(PlaylistSettings.showRowNumber) {
            return rowNumber + (PlaylistSettings.showMediaTitle ? model.title : model.name)
        }
        return (PlaylistSettings.showMediaTitle ? model.title : model.name)
    }

    function pad(number, length) {
        while (number.length < length)
            number = "0" + number;
        return number;
    }
}
