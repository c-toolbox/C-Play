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

    property string iconName: "kt-set-max-upload-speed"
    property int iconWidth: 0
    property string rowNumber: (index + 1).toString()

    implicitWidth: ListView.view.width
    down: model.isPlaying
    padding: 0
    font.pointSize: 9

    background: Rectangle {
        anchors.fill: parent
        color: {
            if (highlighted) {
                return Qt.alpha(Kirigami.Theme.highlightColor, 0.6)
            }

            if (hovered) {
                return Qt.alpha(Kirigami.Theme.hoverColor, 0.4)
            }

            if(down) {
                return Qt.alpha(Kirigami.Theme.positiveBackgroundColor, 0.8)
            }

            return Kirigami.Theme.backgroundColor
        }
    }

    contentItem: Kirigami.IconTitleSubtitle {
        anchors.fill: parent
        icon.name: iconName
        icon.color: color
        icon.width: iconWidth
        title: mainText()
        subtitle: subText()
        color: root.hovered || root.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
        selected: root.down
    }

    Connections {
        target: mpv
        function onPauseChanged() {
            if (model.isPlaying) {
                if (mpv.pause) {
                    iconName = "media-playback-pause"
                }
                else {
                    iconName = "media-playback-start"
                }
                iconWidth = root.height * 0.8
            }
            else {
                iconName = "kt-set-max-upload-speed"
                iconWidth = 0
            }
        }
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
        iconWidth = Kirigami.Units.gridUnit * 1.8
        mpv.position = 0
        mpv.loadItem(index)
        mpv.playlistModel.setPlayingVideo(index)
        if(mpv.autoPlay)
            playItem.start()
    }

    function mainText() {
        const rowNumber = pad(root.rowNumber, playlistView.count.toString().length) + ". "

        if(PlaylistSettings.showRowNumber) {
            return rowNumber + (PlaylistSettings.showMediaTitle ? model.title : model.name)
        }
        return (PlaylistSettings.showMediaTitle ? model.title : model.name)
    }

    function subText() {
        if(model.hasDescriptionFile) {
            return model.duration + " : " + model.stereoVideo + " " + model.gridToMapOn + " : (" + model.eofMode + ")"
        }
        return model.duration + " : (" + model.eofMode + ")"
    } 

    function pad(number, length) {
        while (number.length < length)
            number = "0" + number;
        return number;
    }
}
