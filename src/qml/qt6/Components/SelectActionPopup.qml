/*
 * SPDX-FileCopyrightText: 2024 Erik Sundén <eriksunden85@gmail.com>
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

import org.ctoolbox.cplay

Popup {
    id: root

    property string title: ""
    property string subtitle: ""
    property int buttonIndex: -1

    signal actionSelected(string actionName)

    implicitHeight: parent.height * 0.9
    implicitWidth: parent.width * 0.9
    modal: true
    anchors.centerIn: parent
    focus: true

    onOpened: {
        actionsListView.positionViewAtBeginning()
        filterActionsField.selectAll()
        filterActionsField.focus = true
    }

    onActionSelected: close()

    ColumnLayout {
        anchors.fill: parent

        Kirigami.Heading {
            text: root.title
            visible: text !== ""
        }

        Label {
            text: root.subtitle
            visible: text !== ""
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
        }

        TextField {
            id: filterActionsField

            placeholderText: qsTr("Type to filter...")
            focus: true
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            KeyNavigation.up: actionsListView
            KeyNavigation.down: clearActionButton

            onTextChanged: {
                const menuModel = actionsListView.actionsList
                actionsListView.model = menuModel.filter(action => action.toLowerCase().includes(text))
            }
        }
        Button {
            id: clearActionButton

            Layout.fillWidth: true
            text: qsTr("Clear current action")
            KeyNavigation.up: filterActionsField
            KeyNavigation.down: actionsListView
            onClicked: actionSelected("")
            Keys.onEnterPressed: actionSelected("")
            Keys.onReturnPressed: actionSelected("")
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignTop

            ListView {
                id: actionsListView

                property var actionsList: Object.keys(window.appActions).sort()

                implicitHeight: 30 * model.count
                model: actionsList
                spacing: 1
                clip: true
                currentIndex: focus ? 0 : -1

                delegate: ItemDelegate {
                    property string actionName: modelData

                    width: actionsListView.width

                    contentItem: RowLayout {
                        Label {
                            text: modelData

                            Layout.fillWidth: true
                        }
                    }
                    onClicked: actionSelected(modelData)
                    Keys.onEnterPressed: actionSelected(modelData)
                    Keys.onReturnPressed: actionSelected(modelData)
                }

                KeyNavigation.up: filterActionsField
                KeyNavigation.down: filterActionsField
                Keys.onPressed: {
                    if (event.key === Qt.Key_End) {
                        actionsListView.currentIndex = actionsListView.count - 1
                        actionsListView.positionViewAtEnd()
                    }
                    if (event.key === Qt.Key_Home) {
                        actionsListView.currentIndex = 0
                        actionsListView.positionViewAtBeginning()
                    }
                }
            }
        }
    }
}