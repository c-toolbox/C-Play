/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

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

    HorizontalHeaderView {
        id: horizontalHeader
        anchors.left: tableView.left
        anchors.top: parent.top
        syncView: tableView
        clip: true
    }

    VerticalHeaderView {
        id: verticalHeader
        anchors.top: tableView.top
        anchors.left: parent.left
        syncView: tableView
        clip: true
    }

    TableView {
        id: tableView
        anchors.left: verticalHeader.right
        anchors.top: horizontalHeader.bottom
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        columnSpacing: 1
        rowSpacing: 1
        clip: true

        model: app.slides.visibilityModel

        delegate: Rectangle {
            id: cellRect
            implicitWidth: 100
            implicitHeight: 50
            color: model.color

            Text {
                anchors.fill: parent
                text: model.text
                color: model.color === "white" ? "black" : "white"
                font.pointSize: 8
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            MouseArea{
                anchors.fill: parent
                onClicked: app.slides.visibilityModel.cellClicked(model.column, model.row)
            }
        }

        ScrollBar.horizontal: ScrollBar { }
        ScrollBar.vertical: ScrollBar { }
    }

    Label {
        anchors.left: parent.left
        anchors.top: parent.top
        text: qsTr("˅ Layers | Slides >")
        width: 90
        height: 25
        font.pointSize: 7.5
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
