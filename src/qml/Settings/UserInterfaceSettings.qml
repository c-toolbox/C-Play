/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.12
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.12
import QtQuick.Dialogs 1.3

import org.kde.kirigami 2.11 as Kirigami
import org.ctoolbox.cplay 1.0
import Haruna.Components 1.0

SettingsBasePage {
    id: root

    GridLayout {
        id: content
        columns: 3

        SettingsHeader {
            text: qsTr("User Interface")
            Layout.columnSpan: 3
            Layout.fillWidth: true
        }

        Item { width: 1; height: 1 }
        CheckBox {
            text: qsTr("Show MenuBar")
            checked: GeneralSettings.showMenuBar
            onCheckedChanged: {
                GeneralSettings.showMenuBar = checked
                GeneralSettings.save()
            }
        }
        Item {
            Layout.fillWidth: true
        }

        Item { width: 1; height: 1 }
        CheckBox {
            text: qsTr("Show Header")
            checked: GeneralSettings.showHeader
            onCheckedChanged: {
                GeneralSettings.showHeader = checked
                GeneralSettings.save()
            }
        }
        Item {
            Layout.fillWidth: true
        }

        Item { width: 1; height: 1 }
        CheckBox {
            text: qsTr("Show chapter markers")
            checked: GeneralSettings.showChapterMarkers
            onCheckedChanged: {
                GeneralSettings.showChapterMarkers = checked
                GeneralSettings.save()
            }
        }
        Item {
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("Color scheme")
            Layout.alignment: Qt.AlignRight
        }
        ComboBox {
            id: colorThemeSwitcher

            textRole: "display"
            model: app.colorSchemesModel
            delegate: ItemDelegate {
                Kirigami.Theme.colorSet: Kirigami.Theme.View
                width: colorThemeSwitcher.width
                highlighted: model.display === GeneralSettings.colorScheme
                contentItem: RowLayout {
                    Kirigami.Icon {
                        source: model.decoration
                        Layout.preferredHeight: Kirigami.Units.iconSizes.small
                        Layout.preferredWidth: Kirigami.Units.iconSizes.small
                    }
                    Label {
                        text: model.display
                        color: highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                    }
                }
            }

            onActivated: {
                GeneralSettings.colorScheme = colorThemeSwitcher.textAt(index)
                GeneralSettings.save()
                app.activateColorScheme(GeneralSettings.colorScheme)
            }

            Component.onCompleted: currentIndex = find(GeneralSettings.colorScheme)
        }
        Item {
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("GUI Style")
            Layout.alignment: Qt.AlignRight
        }
        ComboBox {
            id: guiStyleComboBox

            textRole: "key"
            model: ListModel {
                id: stylesModel

                ListElement { key: "Breeeze Dark"; }
            }

            onActivated: {
                GeneralSettings.guiStyle = model.get(index).key
                app.setGuiStyle(GeneralSettings.guiStyle)
                // some themes can cause a crash
                // the timer prevents saving the crashing theme,
                // which would cause the app to crash on startup
                saveGuiStyleTimer.start()
            }

            Timer {
                id: saveGuiStyleTimer

                interval: 1000
                running: false
                repeat: false
                onTriggered: GeneralSettings.save()
            }

            Component.onCompleted: {
                // populate the model with the available styles
                for (let i = 0; i < app.availableGuiStyles().length; ++i) {
                    stylesModel.append({key: app.availableGuiStyles()[i]})
                }

                // set the saved style as the current item in the combo box
                for (let j = 0; j < stylesModel.count; ++j) {
                    if (stylesModel.get(j).key === GeneralSettings.guiStyle) {
                        currentIndex = j
                        break
                    }
                }
            }
        }
        Item {
            Layout.fillWidth: true
        }

        Item { width: 1; height: 1 }
        CheckBox {
            text: qsTr("Use Breeze icon theme")
            checked: GeneralSettings.useBreezeIconTheme
            onCheckedChanged: {
                GeneralSettings.useBreezeIconTheme = checked
                GeneralSettings.save()
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
            text: qsTr("Osd font size")
            Layout.alignment: Qt.AlignRight
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
                value: GeneralSettings.osdFontSize
                onValueChanged: {
                    if (completed) {
                        osd.label.font.pointSize = osdFontSize.value
                        osd.message("Test osd font size")
                        GeneralSettings.osdFontSize = osdFontSize.value
                        GeneralSettings.save()
                    }
                }
                Component.onCompleted: completed = true
            }
        }
        Item {
            Layout.fillWidth: true
        }
    }
}
