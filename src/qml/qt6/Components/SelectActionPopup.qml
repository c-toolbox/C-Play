/*
 * SPDX-FileCopyrightText: 2024-2025 Erik Sunden <eriksunden85@gmail.com>
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

    property int buttonIndex: -1
    property string subtitle: ""
    property string title: ""

    signal actionSelected(string actionName)

    anchors.centerIn: parent
    focus: true
    implicitHeight: parent.height * 0.9
    implicitWidth: parent.width * 0.9
    modal: true

    onActionSelected: close()
    onOpened: {
        actionsListView.positionViewAtBeginning();
        filterActionsField.selectAll();
        filterActionsField.focus = true;
    }

    ColumnLayout {
        anchors.fill: parent

        Kirigami.Heading {
            text: root.title
            visible: text !== ""
        }
        Label {
            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: true
            text: root.subtitle
            visible: text !== ""
        }
        TextField {
            id: filterActionsField

            KeyNavigation.down: clearActionButton
            KeyNavigation.up: actionsListView
            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: true
            focus: true
            placeholderText: qsTr("Type to filter...")

            onTextChanged: {
                const menuModel = actionsListView.actionsList;
                actionsListView.model = menuModel.filter(action => action.toLowerCase().includes(text));
            }
        }
        Button {
            id: clearActionButton

            KeyNavigation.down: actionsListView
            KeyNavigation.up: filterActionsField
            Layout.fillWidth: true
            text: qsTr("Clear current action")

            Keys.onEnterPressed: actionSelected("")
            Keys.onReturnPressed: actionSelected("")
            onClicked: actionSelected("")
        }
        ScrollView {
            Layout.alignment: Qt.AlignTop
            Layout.fillHeight: true
            Layout.fillWidth: true
            clip: true

            ListView {
                id: actionsListView

                property var actionsList: Object.keys(window.appActions).sort()

                KeyNavigation.down: filterActionsField
                KeyNavigation.up: filterActionsField
                clip: true
                currentIndex: focus ? 0 : -1
                implicitHeight: 30 * model.count
                model: actionsList
                spacing: 1

                delegate: ItemDelegate {
                    property string actionName: modelData

                    width: actionsListView.width

                    contentItem: RowLayout {
                        Label {
                            Layout.fillWidth: true
                            text: modelData
                        }
                    }

                    Keys.onEnterPressed: actionSelected(modelData)
                    Keys.onReturnPressed: actionSelected(modelData)
                    onClicked: actionSelected(modelData)
                }

                Keys.onPressed: {
                    if (event.key === Qt.Key_End) {
                        actionsListView.currentIndex = actionsListView.count - 1;
                        actionsListView.positionViewAtEnd();
                    }
                    if (event.key === Qt.Key_Home) {
                        actionsListView.currentIndex = 0;
                        actionsListView.positionViewAtBeginning();
                    }
                }
            }
        }
    }
}
