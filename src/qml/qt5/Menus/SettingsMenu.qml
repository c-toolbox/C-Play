/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.15
import QtQuick.Controls 2.15

Menu {
    id: root

    title: qsTr("&Settings")

    MenuItem {
        action: actions["configureAction"]
    }
    MenuItem {
        action: actions["configureShortcutsAction"]
    }
}
