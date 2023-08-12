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

    property bool isPlaying: model.isPlaying
    property string rowNumber: (index + 1).toString()
    property var alpha: PlaylistSettings.overlayVideo ? 0.6 : 1

    label: mainText()
    subtitle: model.duration + " : " + stereoVideoText(index) + " " + gridToMapOnText(index) + " : (" + loopModeText(index) + ")"
    padding: 0
    icon: model.isPlaying ? "media-playback-start" : ""
    backgroundColor: {
        let color = model.isPlaying ? Kirigami.Theme.highlightColor : Kirigami.Theme.backgroundColor
        Qt.hsla(color.hslHue, color.hslSaturation, color.hslLightness, alpha)
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
        //playItem.start()
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

    function stereoVideoText(i) {
        const stereoVideo = mpv.playlistModel.stereoVideo(i)
        if(stereoVideo===0){
            return "2D";
        }
        else {
            return "3D";
        }
    }

    function gridToMapOnText(i) {
        const gridToMapOn = mpv.playlistModel.gridToMapOn(i)
        if(gridToMapOn===0){
            return "Split";
        }
        else if(gridToMapOn===1){
            return "Flat";
        }
        else if(gridToMapOn===2){
            return "Dome";
        }
        else {
            return "Sphere";
        }
    }

    function loopModeText(i) {
        const loopMode = mpv.playlistModel.loopMode(i)
        if(loopMode===1){ //Pause
            return "Pause at end";
        }
        else if(loopMode===2){ //Loop
            return "Loop video";
        }
        else { // Continue (0)
            return "Continue to next";
        }
    }
}
