/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.15

import org.kde.kirigami 2.15 as Kirigami
import org.ctoolbox.cplay 1.0
import Haruna.Components 1.0 as HC

Kirigami.BasicListItem {
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

    backgroundColor: {
        let color = Kirigami.Theme.backgroundColor;
        Qt.hsla(color.hslHue, color.hslSaturation, color.hslLightness, 1);
    }
    icon: model.isPlaying ? "office-chart-pie" : ""
    label: mainText()
    padding: 0
    subtitle: model.startTime + " - " + model.endTime + " (" + model.duration + ")" + " - At end: " + model.eosMode

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
