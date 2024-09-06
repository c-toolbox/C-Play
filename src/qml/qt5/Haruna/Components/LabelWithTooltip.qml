/*
 * SPDX-FileCopyrightText: 2021 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 1.15

QQC2.Label {
    id: root

    property bool alwaysShowToolTip
    property int toolTipFontSize
    property string toolTipText

    QQC2.ToolTip {
        id: toolTip

        font.pointSize: root.toolTipFontSize ? root.toolTipFontSize : root.font.pointSize
        text: root.toolTipText ? root.toolTipText : root.text
        visible: (root.alwaysShowToolTip && mouseArea.containsMouse) || (mouseArea.containsMouse && root.truncated)
    }
    MouseArea {
        id: mouseArea

        acceptedButtons: Qt.NoButton
        anchors.fill: parent
        hoverEnabled: true
    }
}
