/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
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

    implicitWidth: ListView.view.width
    highlighted: model.isPlaying

    background: Rectangle {
        anchors.fill: parent
        color: {
            if (hovered) {
                return Qt.alpha(Kirigami.Theme.hoverColor, 0.6)
            }

            if (highlighted) {
                return Qt.alpha(Kirigami.Theme.highlightColor, 0.3)
            }

            return Kirigami.Theme.backgroundColor
        }
    }

    contentItem: Kirigami.IconTitleSubtitle {
        icon.name: "drive-partition"
        icon.color: color
        icon.width: model.isPlaying ? root.height * 0.8 : 0
        title: mainText()
        subtitle: subText()
        color: root.hovered || root.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
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
    }

    function mainText() {
        const rowNumber = pad(root.rowNumber, sectionsView.count.toString().length) + ". "

        if(PlaylistSettings.showRowNumber) {
            return rowNumber + (model.title)
        }
        return (model.title)
    }

    function subText() {
        return model.startTime + " - " + model.endTime + " (" + model.duration + ")" + "\nAt end: " + model.eosMode
    } 

    function pad(number, length) {
        while (number.length < length)
            number = "0" + number;
        return number;
    }
}
