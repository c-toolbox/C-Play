/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Controls
import "../Components/MenuHelpers.js" as MenuHelpers

Menu {
    id: root

    title: qsTr("&Help")

    onOpened: MenuHelpers.handleMenuOpen()
    onClosed: MenuHelpers.handleMenuClose()

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
