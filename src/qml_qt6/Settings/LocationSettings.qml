/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Dialogs

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay



SettingsBasePage {
    id: root

    FolderDialog {
        id: openPrimaryFileDialogLocation

        currentFolder: LocationSettings.fileDialogLocation !== ""
                ? app.pathToUrl(LocationSettings.fileDialogLocation)
                : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        title: "Choose Primary File Dialog Location"

        onAccepted: {
            var filePath = openPrimaryFileDialogLocation.selectedFolder.toString();
            // remove prefixed "file:///"
            filePath = filePath.replace(/^(file:\/{3})/,"");

            fileDialogLocation.text = filePath

            LocationSettings.fileDialogLocation = fileDialogLocation.text
            LocationSettings.save()

            mpv.focus = true
        }
        onRejected: mpv.focus = true
    }

    FolderDialog {
        id: openCPlayFileLocation

        currentFolder: LocationSettings.cPlayFileLocation !== ""
                ? app.pathToUrl(LocationSettings.cPlayFileLocation)
                : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        title: "Choose Common C-Play File Location"

        onAccepted: {
            var filePath = openCPlayFileLocation.selectedFolder.toString();
            // remove prefixed "file:///"
            filePath = filePath.replace(/^(file:\/{3})/,"");

            cPlayFileLocation.text = filePath

            LocationSettings.cPlayFileLocation = cPlayFileLocation.text
            LocationSettings.save()

            mpv.focus = true
        }
        onRejected: mpv.focus = true
    }

    FolderDialog {
        id: openCPlayMediaLocation

        currentFolder: LocationSettings.cPlayMediaLocation !== ""
                ? app.pathToUrl(LocationSettings.cPlayMediaLocation)
                : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        title: "Choose Common C-Play Media Location"

        onAccepted: {
            var filePath = openCPlayMediaLocation.selectedFolder.toString();
            // remove prefixed "file:///"
            filePath = filePath.replace(/^(file:\/{3})/,"");

            cPlayMediaLocation.text = filePath

            LocationSettings.cPlayMediaLocation = cPlayMediaLocation.text
            LocationSettings.save()

            mpv.focus = true
        }
        onRejected: mpv.focus = true
    }

    FolderDialog {
        id: openUniviewVideoLocation

        currentFolder: LocationSettings.univiewVideoLocation !== ""
                ? app.pathToUrl(LocationSettings.univiewVideoLocation)
                : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        title: "Choose Common Uniview Media Location"

        onAccepted: {
            var filePath = openUniviewVideoLocation.selectedFolder.toString();
            // remove prefixed "file:///"
            filePath = filePath.replace(/^(file:\/{3})/,"");

            univiewVideoLocation.text = filePath

            LocationSettings.univiewVideoLocation = univiewVideoLocation.text
            LocationSettings.save()

            mpv.focus = true
        }
        onRejected: mpv.focus = true
    }

    GridLayout {
        id: content
        columns: 3

        SettingsHeader {
            text: qsTr("Relative path settings")
            Layout.columnSpan: 3
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("Common C-play file location")
            Layout.alignment: Qt.AlignRight
        }

        RowLayout {
            height: cPlayFileLocation.height

            TextField {
                id: cPlayFileLocation

                text: LocationSettings.cPlayFileLocation
                onEditingFinished: {
                    LocationSettings.cPlayFileLocation = cPlayFileLocation.text
                    LocationSettings.save()
                }

                placeholderText: qsTr("Set for relative cplayfile/list paths")

                ToolTip {
                    text: qsTr("Common directory for where the C-play file(s) are stored.")
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

                text: LocationSettings.cPlayMediaLocation
                onEditingFinished: {
                    LocationSettings.cPlayMediaLocation = cPlayMediaLocation.text
                    LocationSettings.save()
                }

                placeholderText: qsTr("Set for relative media paths")

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
            text: qsTr("File dialog location")
            Layout.alignment: Qt.AlignRight
        }

        RowLayout {
            height: fileDialogLocation.height
            TextField {
                id: fileDialogLocation

                text: LocationSettings.fileDialogLocation
                onEditingFinished: {
                    LocationSettings.fileDialogLocation = fileDialogLocation.text
                    LocationSettings.save()
                }

                placeholderText: qsTr("Recommended empty...")

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
            text: qsTr("Uniview video location")
            Layout.alignment: Qt.AlignRight
        }

        RowLayout {
            height: univiewVideoLocation.height

            TextField {
                id: univiewVideoLocation

                text: LocationSettings.univiewVideoLocation
                onEditingFinished: {
                    LocationSettings.univiewVideoLocation = univiewVideoLocation.text
                    LocationSettings.save()
                }

                placeholderText: qsTr("Only needed to load *.fdv files")

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

        Item {
            // spacer item
            height: 20
            Layout.columnSpan: 3
            Layout.fillWidth: true
        }

        // ------------------------------------
        // Screenshot Format
        // ------------------------------------
        SettingsHeader {
            text: qsTr("Screenshots")
            topMargin: 0
            Layout.columnSpan: 3
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("Format")
            Layout.alignment: Qt.AlignRight
        }

        ComboBox {
            id: screenshotFormat
            textRole: "key"
            model: ListModel {
                ListElement { key: "PNG"; }
                ListElement { key: "JPG"; }
                ListElement { key: "WebP"; }
            }

            onActivated: {
                LocationSettings.screenshotFormat = model.get(index).key
                LocationSettings.save()
                mpv.setProperty("screenshot-format", LocationSettings.screenshotFormat)
            }

            Component.onCompleted: {
                if (LocationSettings.screenshotFormat === "PNG") {
                    currentIndex = 0
                }
                if (LocationSettings.screenshotFormat === "JPG") {
                    currentIndex = 1
                }
                if (LocationSettings.screenshotFormat === "WebP") {
                    currentIndex = 2
                }
            }
        }
        Item {
            Layout.fillWidth: true
        }

        // ------------------------------------
        // Screenshot template
        // ------------------------------------
        Label {
            text: qsTr("Template")
            Layout.alignment: Qt.AlignRight
        }

        TextField {
            id: screenshotTemplate
            text: LocationSettings.screenshotTemplate
            onEditingFinished: {
                LocationSettings.screenshotTemplate = text
                LocationSettings.save()
                mpv.setProperty("screenshot-template", LocationSettings.screenshotTemplate)
            }
        }
        Item {
            Layout.fillWidth: true
        }
    }
}
