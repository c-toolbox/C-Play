/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.15 as Kirigami

Popup {
    id: root

    property int buttonIndex: -1
    property string headerTitle

    signal actionSelected(string actionName)

    anchors.centerIn: parent
    focus: true
    implicitHeight: parent.height * 0.9
    implicitWidth: parent.width * 0.9
    modal: true

    onActionSelected: close()
    onOpened: {
        filterActionsField.text = "";
        filterActionsField.focus = true;
    }

    Action {
        shortcut: "ctrl+f"

        onTriggered: filterActionsField.forceActiveFocus(Qt.ShortcutFocusReason)
    }
    ColumnLayout {
        anchors.fill: parent

        Kirigami.Heading {
            text: root.headerTitle
        }
        Label {
            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: true
            text: qsTr("Double click to set action")
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
        ListView {
            id: actionsListView

            property var actionsList: Object.keys(window.appActions).sort()

            KeyNavigation.down: filterActionsField
            KeyNavigation.up: clearActionButton
            Layout.alignment: Qt.AlignTop
            Layout.fillHeight: true
            Layout.fillWidth: true
            clip: true
            currentIndex: focus ? 0 : -1
            implicitHeight: 30 * model.count
            model: actionsList
            spacing: 1

            delegate: Kirigami.BasicListItem {
                height: 30
                label: modelData
                reserveSpaceForIcon: false
                width: root.width

                Keys.onEnterPressed: actionSelected(modelData)
                Keys.onReturnPressed: actionSelected(modelData)
                onDoubleClicked: actionSelected(modelData)
            }

            Keys.onPressed: {
                if (event.key === Qt.Key_End) {
                    actionsListView.currentIndex = actionsListView.count - 1;
                    actionsListView.positionViewAtIndex(actionsListView.currentIndex, ListView.Center);
                }
                if (event.key === Qt.Key_Home) {
                    actionsListView.currentIndex = 0;
                    actionsListView.positionViewAtIndex(actionsListView.currentIndex, ListView.Center);
                }
            }
        }
    }
}
