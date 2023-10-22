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
    subtitle: model.startTime + " - " + model.endTime + " (" + model.duration + ")" + " - At end: " + model.eosMode
    padding: 0
    icon: model.isPlaying ? "media-playback-start" : ""
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
        mpv.loadSection(index)
        mpv.playSectionsModel.setPlayingSection(index)
    }

    ToolTip {
        text: model.title
        visible: root.containsMouse
        font.pointSize: Kirigami.Units.gridUnit - 5
    }

    function mainText() {
        const rowNumber = pad(root.rowNumber, sectionsView.count.toString().length) + ". "

        if(PlaylistSettings.showRowNumber) {
            return rowNumber + (model.title)
        }
        return (model.title)
    }

    function pad(number, length) {
        while (number.length < length)
            number = "0" + number;
        return number;
    }
}
