/*
 * SPDX-FileCopyrightText:
 * 2021-2025 Erik Sunden <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

SettingsBasePage {
    id: root

    GridLayout {
        id: content

        columns: 3

        SettingsHeader {
            Layout.columnSpan: 3
            Layout.fillWidth: true
            text: qsTr("User Interface")
        }

        Item {
            height: 1
            width: 1
        }
        CheckBox {
            checked: UserInterfaceSettings.showHeader
            text: qsTr("Show Top Bar / Header")

            onCheckedChanged: {
                UserInterfaceSettings.showHeader = checked;
                UserInterfaceSettings.save();
            }
        }
        Item {
            Layout.fillWidth: true
        }

        Item {
            height: 1
            width: 1
        }
        CheckBox {
            checked: UserInterfaceSettings.showFooter
            text: qsTr("Show Bottom Bar / Footer")

            onCheckedChanged: {
                UserInterfaceSettings.showFooter = checked;
                UserInterfaceSettings.save();
            }
        }
        Item {
            Layout.fillWidth: true
        }

        Item {
            height: 1
            width: 1
        }
        CheckBox {
            id: idleModeCheckBox
            checked: UserInterfaceSettings.idleModeOn
            text: qsTr("Enable idle mode to hide UI after inactivity")

            onCheckedChanged: {
                UserInterfaceSettings.idleModeOn = checked;
                UserInterfaceSettings.save();
            }
        }
        Item {
            Layout.fillWidth: true
        }

        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Idle mode time:")
            enabled: UserInterfaceSettings.idleModeOn
        }
        RowLayout {
            SpinBox {
                editable: true
                from: 1
                to: 3600
                value: UserInterfaceSettings.idleModeTime
                enabled: UserInterfaceSettings.idleModeOn

                onValueChanged: {
                    UserInterfaceSettings.idleModeTime = value.toFixed(0);
                    UserInterfaceSettings.save();
                }
            }
            Label {
                Layout.alignment: Qt.AlignLeft
                text: qsTr("(seconds)")
                enabled: UserInterfaceSettings.idleModeOn
            }
        }
        Item {
            Layout.fillWidth: true
        }

        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Color scheme")
        }
        ComboBox {
            id: colorThemeSwitcher

            model: app.colorSchemesModel
            textRole: "display"

            delegate: ItemDelegate {
                Kirigami.Theme.colorSet: Kirigami.Theme.View
                highlighted: model.display === UserInterfaceSettings.colorScheme
                width: colorThemeSwitcher.width

                contentItem: RowLayout {
                    Kirigami.Icon {
                        Layout.preferredHeight: Kirigami.Units.iconSizes.small
                        Layout.preferredWidth: Kirigami.Units.iconSizes.small
                        source: model.decoration
                    }
                    Label {
                        color: highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                        text: model.display
                    }
                }
            }

            Component.onCompleted: currentIndex = find(UserInterfaceSettings.colorScheme)
            onActivated: {
                UserInterfaceSettings.colorScheme = colorThemeSwitcher.textAt(index);
                UserInterfaceSettings.save();
                app.activateColorScheme(UserInterfaceSettings.colorScheme);
            }
        }
        Item {
            Layout.fillWidth: true
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("GUI Style")
        }
        ComboBox {
            id: guiStyleComboBox

            textRole: "key"

            model: ListModel {
                id: stylesModel

                ListElement {
                    key: "Breeeze Dark"
                }
            }

            Component.onCompleted: {
                // populate the model with the available styles
                for (let i = 0; i < app.availableGuiStyles().length; ++i) {
                    stylesModel.append({
                        key: app.availableGuiStyles()[i]
                    });
                }

                // set the saved style as the current item in the combo box
                for (let j = 0; j < stylesModel.count; ++j) {
                    if (stylesModel.get(j).key === UserInterfaceSettings.guiStyle) {
                        currentIndex = j;
                        break;
                    }
                }
            }
            onActivated: {
                UserInterfaceSettings.guiStyle = model.get(index).key;
                app.setGuiStyle(UserInterfaceSettings.guiStyle);
                // some themes can cause a crash
                // the timer prevents saving the crashing theme,
                // which would cause the app to crash on startup
                saveGuiStyleTimer.start();
            }

            Timer {
                id: saveGuiStyleTimer

                interval: 1000
                repeat: false
                running: false

                onTriggered: UserInterfaceSettings.save()
            }
        }
        Item {
            Layout.fillWidth: true
        }
        Item {
            height: 1
            width: 1
        }
        CheckBox {
            checked: UserInterfaceSettings.useBreezeIconTheme
            text: qsTr("Use Breeze icon theme")

            onCheckedChanged: {
                UserInterfaceSettings.useBreezeIconTheme = checked;
                UserInterfaceSettings.save();
            }

            ToolTip {
                text: qsTr("Sets the icon theme to breeze.\nRequires restart.")
            }
        }
        Item {
            Layout.fillWidth: true
        }

        // OSD Font Size
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Osd font size")
        }
        Item {
            height: osdFontSize.height

            SpinBox {
                id: osdFontSize

                // used to prevent osd showing when opening the page
                property bool completed: false

                editable: true
                from: 0
                to: 100
                value: UserInterfaceSettings.osdFontSize

                Component.onCompleted: completed = true
                onValueChanged: {
                    if (completed) {
                        osd.label.font.pointSize = osdFontSize.value;
                        osd.message("Test osd font size");
                        UserInterfaceSettings.osdFontSize = osdFontSize.value;
                        UserInterfaceSettings.save();
                    }
                }
            }
        }
        Item {
            Layout.fillWidth: true
        }
    }
}
