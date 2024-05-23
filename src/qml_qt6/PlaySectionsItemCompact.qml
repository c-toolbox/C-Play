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

    background: Rectangle {
        anchors.fill: parent
        color: {
            let color = Kirigami.Theme.backgroundColor
            Qt.hsla(color.hslHue, color.hslSaturation, color.hslLightness, 1)
        }
    }

    contentItem: Kirigami.IconTitleSubtitle {
        icon.name: model.isPlaying ? "office-chart-pie" : ""
        icon.color: color
        title: mainText()
        subtitle: model.startTime + " - " + model.endTime + " (" + model.duration + ")" + " - At end: " + model.eosMode
        color: root.hovered || root.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
        ToolTip.text: title
        ToolTip.visible: root.hovered
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

    ToolTip {
        text: model.title
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
