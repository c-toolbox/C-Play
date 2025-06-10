/*
 * SPDX-FileCopyrightText:
 * 2021-2025 Erik Sunden <eriksunden85@gmail.com>
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

    function mainText() {
        const rowNumber = pad(root.rowNumber, sectionsView.count.toString().length) + ". ";
        if (PlaylistSettings.showRowNumber) {
            return rowNumber + (model.title);
        }
        return (model.title);
    }
    function pad(number, length) {
        while (number.length < length)
            number = "0" + number;
        return number;
    }
    function subText() {
        return model.startTime + " - " + model.endTime + " (" + model.duration + ")" + "\nAt end: " + model.eosMode;
    }

    down: model.isPlaying
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
        color: root.hovered || root.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
        icon.color: color
        icon.name: "drive-partition"
        icon.width: model.isPlaying ? root.height * 0.8 : 0
        subtitle: subText()
        title: mainText()
    }

    onDoubleClicked: {
        mpv.pause = true;
        mpv.loadSection(index);
    }

    Timer {
        id: playItem

        interval: 2000

        onTriggered: {
            mpv.pause = false;
        }
    }
}
