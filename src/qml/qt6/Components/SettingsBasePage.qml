/*
 * SPDX-FileCopyrightText: 2021 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml

import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: root

    property bool hasHelp: true
    property string helpUrl: "https://c-toolbox.github.io/C-Play/versions/v_2_2.html"

    actions: [
        Kirigami.Action {
            enabled: root.hasHelp
            icon.name: "system-help"
            text: qsTr("Help!")

            onTriggered: Qt.openUrlExternally(helpUrl)
        }
    ]
}
