/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.15
import QtQuick.Window 2.1
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

import org.kde.kirigami 2.15 as Kirigami
import org.ctoolbox.cplay 1.0

Kirigami.ApplicationWindow {
    id: slideVisibilityWindow

    color: Kirigami.Theme.alternateBackgroundColor
    height: 630
    minimumWidth: 670
    title: qsTr("Slides Visibility Table View")
    visible: false
    width: 670

    Component.onCompleted: {
    }
    onClosing: {
    }
    onVisibilityChanged: {
    }

    // NOT CORRECTLY IMPLEMENTED IN QT5
    // TableView seems broken...
    TableView {
        anchors.fill: parent
        columnSpacing: 1
        rowSpacing: 1
        clip: true

        model: app.slides.visibilityModel

        delegate: Rectangle {
            implicitWidth: 100
            implicitHeight: 50
            Text {
                text: display
            }
        }
    }
}
