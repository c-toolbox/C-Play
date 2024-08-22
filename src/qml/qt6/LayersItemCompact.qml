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
    //down: model.isPlaying
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
        return model.type + " - " + model.stereoVideo + " " + model.gridToMapOn
    } 

    function pad(number, length) {
        while (number.length < length)
            number = "0" + number;
        return number;
    }
}
