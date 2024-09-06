/*
 * SPDX-FileCopyrightText: 2021 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

RowLayout {
    id: root

    property int value: 0

    signal sliderValueChanged(int value)

    Slider {
        id: slider

        from: -100
        stepSize: 1
        to: 100
        value: root.value
        wheelEnabled: true

        Component.onCompleted: background.activeControl = ""
        onValueChanged: root.sliderValueChanged(value.toFixed(0))

        MouseArea {
            acceptedButtons: Qt.MiddleButton
            anchors.fill: parent

            onClicked: slider.value = 0
        }
    }
    Label {
        Layout.preferredWidth: 40
        font.pointSize: 9
        horizontalAlignment: Qt.AlignHCenter
        text: slider.value
    }
}
