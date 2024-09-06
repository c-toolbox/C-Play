/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.15
import QtQuick.Controls 2.15

import org.kde.kirigami 2.15 as Kirigami
import org.ctoolbox.cplay 1.0

Item {
    id: root

    property alias label: label

    function message(text) {
        const osdFontSize = parseInt(UserInterfaceSettings.osdFontSize);
        label.text = text;
        if (osdFontSize === 0) {
            return;
        }
        if (label.visible) {
            timer.restart();
        } else {
            timer.start();
        }
        label.visible = true;
    }

    Label {
        id: label

        color: Kirigami.Theme.textColor
        font.pointSize: parseInt(UserInterfaceSettings.osdFontSize)
        padding: 5
        visible: false
        x: 10
        y: 10

        background: Rectangle {
            color: Kirigami.Theme.backgroundColor
        }
    }
    Timer {
        id: timer

        interval: 3000
        repeat: false
        running: false

        onTriggered: {
            label.visible = false;
        }
    }
}
