/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtGraphicalEffects 1.12

import org.kde.kirigami 2.11 as Kirigami
import com.georgefb.haruna 1.0
import Haruna.Components 1.0 as HC

Kirigami.BasicListItem {
    id: root

    property string rowNumber: (index + 1).toString()

    label: mainText()
    subtitle: model.duration + " : " + model.stereoVideo + " " + model.gridToMapOn + " : (" + model.loopMode + ")"
    padding: 0
    icon: model.isPlaying ? "kt-set-max-upload-speed" : ""
    font.pointSize: 9
    backgroundColor: {
        let color = Kirigami.Theme.backgroundColor
        Qt.hsla(color.hslHue, color.hslSaturation, color.hslLightness, 1)
    }
    Timer {
       id: playItem
       interval: 2000
       onTriggered: {
           mpv.pause = false
       }
    }

    onDoubleClicked: {
        mpv.pause = true
        mpv.position = 0
        mpv.loadItem(index)
        mpv.playlistModel.setPlayingVideo(index)
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
