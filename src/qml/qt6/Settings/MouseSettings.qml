/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQml
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

SettingsBasePage {
    id: root

    hasHelp: false
    helpUrl: ""

    ColumnLayout {
        id: content

        anchors.fill: parent
        spacing: Kirigami.Units.largeSpacing

        ListModel {
            id: mouseActionsModel

            ListElement {
                key: "left"
                label: "Left"
            }
            ListElement {
                key: "leftx2"
                label: "Left double click"
            }
            ListElement {
                key: "right"
                label: "Right"
            }
            ListElement {
                key: "rightx2"
                label: "Right double click"
            }
            ListElement {
                key: "middle"
                label: "Middle"
            }
            ListElement {
                key: "middlex2"
                label: "Middle double click"
            }
            ListElement {
                key: "scrollUp"
                label: "Scroll up"
            }
            ListElement {
                key: "scrollDown"
                label: "Scroll down"
            }
        }
        ListView {
            id: mouseButtonsListView

            property int delegateHeight

            Layout.fillHeight: true
            Layout.fillWidth: true
            model: mouseActionsModel

            delegate: ItemDelegate {
                id: delegate

                function openSelectActionPopup() {
                    selectActionPopup.buttonIndex = model.index;
                    selectActionPopup.title = model.label;
                    selectActionPopup.open();
                }

                highlighted: false
                width: content.width

                contentItem: RowLayout {
                    Kirigami.IconTitleSubtitle {
                        Layout.fillWidth: true
                        icon.name: MouseSettings[model.key] ? "checkmark" : ""
                        subtitle: MouseSettings[model.key] ? appActions[MouseSettings[model.key]].text : "No action set"
                        title: model.label
                    }
                    ToolButton {
                        icon.name: "edit-clear-all"
                        visible: MouseSettings[model.key]

                        onClicked: {
                            MouseSettings[model.key] = "";
                            MouseSettings.save();
                        }

                        ToolTip {
                            text: "Clear action"
                        }
                    }
                }

                Component.onCompleted: mouseButtonsListView.delegateHeight = height
                onClicked: openSelectActionPopup()

                Connections {
                    function onActionSelected(actionName) {
                        if (selectActionPopup.buttonIndex === model.index) {
                            MouseSettings[model.key] = actionName;
                            MouseSettings.save();
                        }
                    }

                    target: selectActionPopup
                }
            }
        }
        Item {
            height: Kirigami.Units.gridUnit
            width: Kirigami.Units.gridUnit
        }
        SelectActionPopup {
            id: selectActionPopup

            subtitle: "Double click to set action"
        }
    }
}
