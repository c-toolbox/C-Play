/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQml 2.15
import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

import org.kde.kirigami 2.15 as Kirigami
import org.ctoolbox.cplay 1.0
import Haruna.Components 1.0

SettingsBasePage {
    id: root

    hasHelp: false
    helpUrl: ""

    ColumnLayout {
        id: content

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
                label: "ScrollUp"
            }
            ListElement {
                key: "scrollDown"
                label: "ScrollDown"
            }
        }
        ListView {
            id: mouseButtonsListView

            property int delegateHeight

            implicitHeight: delegateHeight * (mouseButtonsListView.count + 1)
            model: mouseActionsModel

            delegate: Kirigami.BasicListItem {
                id: delegate

                function openSelectActionPopup() {
                    selectActionPopup.buttonIndex = model.index;
                    selectActionPopup.headerTitle = model.label;
                    selectActionPopup.open();
                }

                highlighted: false
                icon: MouseSettings[model.key] ? "checkmark" : ""
                label: model.label
                reserveSpaceForIcon: true
                subtitle: MouseSettings[model.key] ? MouseSettings[model.key] : "No action set"
                width: content.width

                Component.onCompleted: mouseButtonsListView.delegateHeight = height
                onClicked: openSelectActionPopup()

                Connections {
                    function onActionSelected() {
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

        }
    }
}
