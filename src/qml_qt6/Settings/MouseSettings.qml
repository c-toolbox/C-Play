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

        spacing: Kirigami.Units.largeSpacing

        ListModel {
            id: mouseActionsModel

            ListElement {
                label: "Left"
                key: "left"
            }
            ListElement {
                label: "Left double click"
                key: "leftx2"
            }
            ListElement {
                label: "Right"
                key: "right"
            }
            ListElement {
                label: "Right double click"
                key: "rightx2"
            }
            ListElement {
                label: "Middle"
                key: "middle"
            }
            ListElement {
                label: "Middle double click"
                key: "middlex2"
            }
            ListElement {
                label: "ScrollUp"
                key: "scrollUp"
            }
            ListElement {
                label: "ScrollDown"
                key: "scrollDown"
            }
        }

        ListView {
            id: mouseButtonsListView

            property int delegateHeight

            implicitHeight: delegateHeight * (mouseButtonsListView.count + 1)
            model: mouseActionsModel

            delegate: Kirigami.BasicListItem {
                id: delegate

                label: model.label
                subtitle: MouseSettings[model.key] ? MouseSettings[model.key] : "No action set"
                icon: MouseSettings[model.key] ? "checkmark" : ""
                reserveSpaceForIcon: true
                width: content.width
                highlighted: false

                onClicked: openSelectActionPopup()
                Component.onCompleted: mouseButtonsListView.delegateHeight = height

                Connections {
                    target: selectActionPopup
                    function onActionSelected() {
                        if (selectActionPopup.buttonIndex === model.index) {
                            MouseSettings[model.key] = actionName
                            MouseSettings.save()
                        }
                    }
                }

                function openSelectActionPopup() {
                    selectActionPopup.buttonIndex = model.index
                    selectActionPopup.headerTitle = model.label
                    selectActionPopup.open()
                }
            }

        }

        Item {
            width: Kirigami.Units.gridUnit
            height: Kirigami.Units.gridUnit
        }

        SelectActionPopup { id: selectActionPopup }
    }
}
