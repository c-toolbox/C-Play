/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.15

import org.kde.kirigami 2.11 as Kirigami
import org.ctoolbox.cplay 1.0
import Haruna.Components 1.0 as HC

Kirigami.BasicListItem {
    id: root

    property string rowNumber: (index + 1).toString()

    label: mainText()
    subtitle: model.hasDescriptionFile === true ? (model.duration + " : " + model.stereoVideo + " " + model.gridToMapOn + " : (" + model.eofMode + ")") : (model.duration + " : (" + model.eofMode + ")")
    padding: 0
    icon: model.isPlaying ? "kt-set-max-upload-speed" : ""
    font.pointSize: 9
    backgroundColor: {
        let color = Kirigami.Theme.backgroundColor
        Qt.hsla(color.hslHue, color.hslSaturation, color.hslLightness, 1)
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
