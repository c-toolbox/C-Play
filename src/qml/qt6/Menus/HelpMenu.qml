/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Controls
import "../Components/PopupHelpers.js" as PopupHelpers

Menu {
    id: root

    title: qsTr("&Help")

    onOpened: PopupHelpers.handlePopupOpen()
    onClosed: PopupHelpers.handlePopupClose()

    MenuItem {
        text: qsTr("Documentation")
        icon.name: "help-browser"

        onTriggered: Qt.openUrlExternally("https://c-toolbox.github.io/C-Play/")
    }
    MenuSeparator {
    }
    MenuItem {
        action: actions["aboutCPlayAction"]
    }
}
