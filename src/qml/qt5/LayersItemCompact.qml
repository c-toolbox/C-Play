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

import org.kde.kirigami 2.15 as Kirigami
import org.ctoolbox.cplay 1.0
import Haruna.Components 1.0 as HC

Kirigami.BasicListItem {
    id: root

    property string rowNumber: (index + 1).toString()

    label: mainText()
    subtitle: model.type + " - " + model.stereoVideo + " " + model.gridToMapOn
    padding: 0
    //icon: "kt-set-max-upload-speed"
    font.pointSize: 9
    backgroundColor: {
        let color = Kirigami.Theme.backgroundColor
        Qt.hsla(color.hslHue, color.hslSaturation, color.hslLightness, 1)
    }

    onDoubleClicked: {
        layerView.layerItem.layerIdx = index
        layerView.title = layerView.layerItem.layerTitle
        layerView.visible = true
    }

    function mainText() {
        const rowNumber = pad(root.rowNumber, layersView.count.toString().length) + ". "
        return rowNumber + model.title
    }

    function subText() {
        return model.stereoVideo + " " + model.gridToMapOn
    } 

    function pad(number, length) {
        while (number.length < length)
            number = "0" + number;
        return number;
    }
}
