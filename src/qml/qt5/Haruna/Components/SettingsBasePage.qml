/*
 * SPDX-FileCopyrightText: 2021 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQml 2.15

import org.kde.kirigami 2.15 as Kirigami

Kirigami.ScrollablePage {
    id: root

    property bool hasHelp: true
    property string helpUrl: "https://c-toolbox.github.io/C-Play/versions/v_2_0.html"

    actions {
        contextualActions: [
            Kirigami.Action {
                text: qsTr("Help!")
                iconName: "system-help"
                enabled: root.hasHelp
                onTriggered: Qt.openUrlExternally(helpUrl)
            }
        ]
    }

}
