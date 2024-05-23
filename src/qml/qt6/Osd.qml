/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Controls

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

Item {
    id: root

    property alias label: label

    Label {
        id: label
        x: 10
        y: 10
        visible: false
        color: Kirigami.Theme.textColor
        background: Rectangle {
            color: Kirigami.Theme.backgroundColor
        }
        padding: 5
        font.pointSize: parseInt(UserInterfaceSettings.osdFontSize)
    }

    Timer {
        id: timer
        running: false
        repeat: false
        interval: 3000

        onTriggered: {
            label.visible = false
        }
    }

    function message(text) {
        const osdFontSize = parseInt(UserInterfaceSettings.osdFontSize)
        label.text = text
        if (osdFontSize === 0) {
            return;
        }

        if(label.visible) {
            timer.restart()
        } else {
            timer.start()
        }
        label.visible = true
    }

}
