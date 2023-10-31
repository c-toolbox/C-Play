/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.12
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.12
import QtQuick.Dialogs 1.3

import org.kde.kirigami 2.11 as Kirigami
import com.georgefb.haruna 1.0

import Haruna.Components 1.0

SettingsBasePage {
    id: root

    hasHelp: true
    helpFile: ":/GeneralSettings.html"

    FileDialog {
        id: openPrimaryFileDialogLocation

        folder: GeneralSettings.fileDialogLocation !== ""
                ? app.pathToUrl(GeneralSettings.fileDialogLocation)
                : app.pathToUrl(GeneralSettings.fileDialogLastLocation)
        selectFolder: true
        title: "Choose Primary File Dialog Location"

        onAccepted: {
            var filePath = openPrimaryFileDialogLocation.fileUrl.toString();
            // remove prefixed "file:///"
            filePath = filePath.replace(/^(file:\/{3})/,"");

            fileDialogLocation.text = filePath

            GeneralSettings.fileDialogLocation = fileDialogLocation.text
            GeneralSettings.save()

            mpv.focus = true
        }
        onRejected: mpv.focus = true
    }

    FileDialog {
        id: openCPlayFileLocation

        folder: GeneralSettings.cPlayFileLocation !== ""
                ? app.pathToUrl(GeneralSettings.cPlayFileLocation)
                : app.pathToUrl(GeneralSettings.fileDialogLastLocation)
        selectFolder: true
        title: "Choose Common C-Play File Location"

        onAccepted: {
            var filePath = openCPlayFileLocation.fileUrl.toString();
            // remove prefixed "file:///"
            filePath = filePath.replace(/^(file:\/{3})/,"");

            cPlayFileLocation.text = filePath

            GeneralSettings.cPlayFileLocation = cPlayFileLocation.text
            GeneralSettings.save()

            mpv.focus = true
        }
        onRejected: mpv.focus = true
    }

    FileDialog {
        id: openCPlayMediaLocation

        folder: GeneralSettings.cPlayMediaLocation !== ""
                ? app.pathToUrl(GeneralSettings.cPlayMediaLocation)
                : app.pathToUrl(GeneralSettings.fileDialogLastLocation)
        selectFolder: true
        title: "Choose Common C-Play Media Location"

        onAccepted: {
            var filePath = openCPlayMediaLocation.fileUrl.toString();
            // remove prefixed "file:///"
            filePath = filePath.replace(/^(file:\/{3})/,"");

            cPlayMediaLocation.text = filePath

            GeneralSettings.cPlayMediaLocation = cPlayMediaLocation.text
            GeneralSettings.save()

            mpv.focus = true
        }
        onRejected: mpv.focus = true
    }

    FileDialog {
        id: openUniviewVideoLocation

        folder: GeneralSettings.univiewVideoLocation !== ""
                ? app.pathToUrl(GeneralSettings.univiewVideoLocation)
                : app.pathToUrl(GeneralSettings.fileDialogLastLocation)
        selectFolder: true
        title: "Choose Common Uniview Media Location"

        onAccepted: {
            var filePath = openUniviewVideoLocation.fileUrl.toString();
            // remove prefixed "file:///"
            filePath = filePath.replace(/^(file:\/{3})/,"");

            univiewVideoLocation.text = filePath

            GeneralSettings.univiewVideoLocation = univiewVideoLocation.text
            GeneralSettings.save()

            mpv.focus = true
        }
        onRejected: mpv.focus = true
    }

    GridLayout {
        id: content
        columns: 3

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
            Layout.fillWidth: true
            Layout.columnSpan: 2
        }

        // Volume Step
        Label {
            text: qsTr("Volume step")
            Layout.alignment: Qt.AlignRight
        }

        Item {
            height: volumeStep.height
            SpinBox {
                id: volumeStep
                editable: true
                from: 0
                to: 100
                value: GeneralSettings.volumeStep
                onValueChanged: {
                    if (root.visible) {
                        GeneralSettings.volumeStep = volumeStep.value
                        GeneralSettings.save()
                    }
                }
            }
            Layout.fillWidth: true
            Layout.columnSpan: 2
        }

        // Seek Small Step
        Label {
            text: qsTr("Seek small step")
            Layout.alignment: Qt.AlignRight
        }

        Item {
            height: seekSmallStep.height
            SpinBox {
                id: seekSmallStep
                editable: true
                from: 0
                to: 100
                value: GeneralSettings.seekSmallStep
                onValueChanged: {
                    GeneralSettings.seekSmallStep = seekSmallStep.value
                    GeneralSettings.save()
                }
            }
            Layout.fillWidth: true
            Layout.columnSpan: 2
        }

        // Seek Medium Step
        Label {
            text: qsTr("Seek medium step")
            Layout.alignment: Qt.AlignRight
        }

        Item {
            height: seekMediumStep.height
            SpinBox {
                id: seekMediumStep
                editable: true
                from: 0
                to: 100
                value: GeneralSettings.seekMediumStep
                onValueChanged: {
                    GeneralSettings.seekMediumStep = seekMediumStep.value
                    GeneralSettings.save()
                }
            }
            Layout.fillWidth: true
            Layout.columnSpan: 2
        }

        // Seek Big Step
        Label {
            text: qsTr("Seek big step")
            Layout.alignment: Qt.AlignRight
        }

        Item {
            height: seekBigStep.height
            SpinBox {
                id: seekBigStep
                editable: true
                from: 0
                to: 100
                value: GeneralSettings.seekBigStep
                onValueChanged: {
                    GeneralSettings.seekBigStep = seekBigStep.value
                    GeneralSettings.save()
                }
            }
            Layout.fillWidth: true
            Layout.columnSpan: 2
        }

        Label {
            text: qsTr("File dialog location")
            Layout.alignment: Qt.AlignRight
        }

        RowLayout {
            height: fileDialogLocation.height
            TextField {
                id: fileDialogLocation

                text: GeneralSettings.fileDialogLocation
                onEditingFinished: {
                    GeneralSettings.fileDialogLocation = fileDialogLocation.text
                    GeneralSettings.save()
                }

                ToolTip {
                    text: qsTr("If empty the file dialog will remember the last opened location.")
                }
            }
            ToolButton {
                id: fileDialogLocationLoadButton
                text: ""
                icon.name: "system-file-manager"
                icon.height: 16
                focusPolicy: Qt.NoFocus
                onClicked: {
                    openPrimaryFileDialogLocation.open()
                }
            }
            Layout.fillWidth: true
            Layout.columnSpan: 2
        }


        Label {
            text: qsTr("Common C-play file location")
            Layout.alignment: Qt.AlignRight
        }

        RowLayout {
            height: cPlayFileLocation.height

            TextField {
                id: cPlayFileLocation

                text: GeneralSettings.cPlayFileLocation
                onEditingFinished: {
                    GeneralSettings.cPlayFileLocation = cPlayFileLocation.text
                    GeneralSettings.save()
                }

                ToolTip {
                    text: qsTr("Common directory for where the cplay_file(s) are stored.")
                }
            }
            ToolButton {
                id: cPlayFileLocationLoadButton
                text: ""
                icon.name: "system-file-manager"
                icon.height: 16
                focusPolicy: Qt.NoFocus

                onClicked: {
                    openCPlayFileLocation.open()
                }
            }
            Layout.fillWidth: true
            Layout.columnSpan: 2
        }

        Label {
            text: qsTr("Common C-play media location")
            Layout.alignment: Qt.AlignRight
        }

        RowLayout {
            height: cPlayMediaLocation.height

            TextField {
                id: cPlayMediaLocation

                text: GeneralSettings.cPlayMediaLocation
                onEditingFinished: {
                    GeneralSettings.cPlayMediaLocation = cPlayMediaLocation.text
                    GeneralSettings.save()
                }

                ToolTip {
                    text: qsTr("Common directory for where the media (video/audio/etc) are stored.")
                }
            }
            ToolButton {
                id: cPlayMediaLocationLoadButton
                text: ""
                icon.name: "system-file-manager"
                icon.height: 16
                focusPolicy: Qt.NoFocus

                onClicked: {
                    openCPlayMediaLocation.open()
                }
            }
            Layout.fillWidth: true
            Layout.columnSpan: 2
        }

        Label {
            text: qsTr("Uniview video location")
            Layout.alignment: Qt.AlignRight
        }

        RowLayout {
            height: univiewVideoLocation.height

            TextField {
                id: univiewVideoLocation

                text: GeneralSettings.univiewVideoLocation
                onEditingFinished: {
                    GeneralSettings.univiewVideoLocation = univiewVideoLocation.text
                    GeneralSettings.save()
                }

                ToolTip {
                    text: qsTr("Common directory where the Uniview video files are stored.")
                }
            }
            ToolButton {
                id: univiewVideoLocationLoadButton
                text: ""
                icon.name: "system-file-manager"
                icon.height: 16
                focusPolicy: Qt.NoFocus

                onClicked: {
                    openUniviewVideoLocation.open()
                }
            }
            Layout.fillWidth: true
            Layout.columnSpan: 2
        }

        SettingsHeader {
            text: qsTr("Interface")
            Layout.columnSpan: 3
            Layout.fillWidth: true
        }

        CheckBox {
            text: qsTr("Show MenuBar")
            checked: GeneralSettings.showMenuBar
            onCheckedChanged: {
                GeneralSettings.showMenuBar = checked
                GeneralSettings.save()
            }
            Layout.row: 10
            Layout.column: 1
        }

        CheckBox {
            text: qsTr("Show Header")
            checked: GeneralSettings.showHeader
            onCheckedChanged: {
                GeneralSettings.showHeader = checked
                GeneralSettings.save()
            }
            Layout.row: 11
            Layout.column: 1
        }

        CheckBox {
            text: qsTr("Show chapter markers")
            checked: GeneralSettings.showChapterMarkers
            onCheckedChanged: {
                GeneralSettings.showChapterMarkers = checked
                GeneralSettings.save()
            }
            Layout.row: 12
            Layout.column: 1
        }

        Item {
            // spacer item
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
                        Layout.fillWidth: true
                    }
                }
            }

            onActivated: {
                GeneralSettings.colorScheme = colorThemeSwitcher.textAt(index)
                GeneralSettings.save()
                app.activateColorScheme(GeneralSettings.colorScheme)
            }

            Component.onCompleted: currentIndex = find(GeneralSettings.colorScheme)
            Layout.columnSpan: 2
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

                ListElement { key: "Default"; }
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
            Layout.columnSpan: 2
        }

        CheckBox {
            text: qsTr("Use Breeze icon theme")
            checked: GeneralSettings.useBreezeIconTheme
            onCheckedChanged: {
                GeneralSettings.useBreezeIconTheme = checked
                GeneralSettings.save()
            }
            Layout.row: 16
            Layout.column: 1

            ToolTip {
                text: qsTr("Sets the icon theme to breeze.\nRequires restart.")
            }
        }

        Item {
            width: Kirigami.Units.gridUnit
            height: Kirigami.Units.gridUnit
        }
    }
}
