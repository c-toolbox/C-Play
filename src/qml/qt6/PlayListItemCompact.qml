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

ItemDelegate {
    id: root

    property string iconName: "kt-set-max-upload-speed"
    property int iconWidth: 0
    property string rowNumber: (index + 1).toString()

    function mainText() {
        const rowNumber = pad(root.rowNumber, playlistView.count.toString().length) + ". ";
        if (PlaylistSettings.showRowNumber) {
            return rowNumber + (PlaylistSettings.showMediaTitle ? model.title : model.name);
        }
        return (PlaylistSettings.showMediaTitle ? model.title : model.name);
    }
    function pad(number, length) {
        while (number.length < length)
            number = "0" + number;
        return number;
    }
    function subText() {
        if (model.hasDescriptionFile) {
            return model.duration + " : " + model.stereoVideo + " " + model.gridToMapOn + " : (" + model.eofMode + ")";
        }
        return model.duration + " : (" + model.eofMode + ")";
    }

    down: model.isPlaying
    font.pointSize: 9
    implicitWidth: ListView.view.width
    padding: 0

    background: Rectangle {
        anchors.fill: parent
        color: {
            if (highlighted) {
                return Qt.alpha(Kirigami.Theme.highlightColor, 0.6);
            }
            if (hovered) {
                return Qt.alpha(Kirigami.Theme.hoverColor, 0.4);
            }
            if (down) {
                return Qt.alpha(Kirigami.Theme.positiveBackgroundColor, 0.8);
            }
            return Kirigami.Theme.backgroundColor;
        }
    }
    contentItem: Kirigami.IconTitleSubtitle {
        anchors.fill: parent
        color: root.hovered || root.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
        icon.color: color
        icon.name: iconName
        icon.width: iconWidth
        selected: root.down
        subtitle: subText()
        title: mainText()
    }

    onDoubleClicked: {
        mpv.pause = true;
        iconWidth = Kirigami.Units.gridUnit * 1.8;
        mpv.position = 0;
        mpv.loadItem(index);
        mpv.playlistModel.setPlayingVideo(index);
        if (mpv.autoPlay)
            playItem.start();
    }

    Connections {
        function onPauseChanged() {
            if (model.isPlaying) {
                if (mpv.pause) {
                    iconName = "media-playback-pause";
                } else {
                    iconName = "media-playback-start";
                }
                iconWidth = root.height * 0.8;
            } else {
                iconName = "kt-set-max-upload-speed";
                iconWidth = 0;
            }
        }

        target: mpv
    }
    Timer {
        id: playItem

        interval: PlaylistSettings.autoPlayAfterTime * 1000

        onTriggered: {
            mpv.pause = false;
        }
    }
}
