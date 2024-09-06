/*
 * SPDX-FileCopyrightText: 2021 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root

    property string text: ""
    property int topMargin: Kirigami.Units.gridUnit

    spacing: 0

    Item {
        height: root.topMargin
        visible: root.topMargin > 0
        width: 1
    }
    RowLayout {
        Rectangle {
            color: Kirigami.Theme.alternateBackgroundColor
            height: 1
            width: Kirigami.Units.gridUnit
        }
        Kirigami.Heading {
            text: root.text
        }
        Rectangle {
            Layout.fillWidth: true
            color: Kirigami.Theme.alternateBackgroundColor
            height: 1
        }
    }
}
