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

        SettingsHeader {
            text: qsTr("Relative path settings")
            Layout.columnSpan: 3
            Layout.fillWidth: true
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
                VideoSettings.screenshotFormat = model.get(index).key
                VideoSettings.save()
                mpv.setProperty("screenshot-format", VideoSettings.screenshotFormat)
            }

            Component.onCompleted: {
                if (VideoSettings.screenshotFormat === "PNG") {
                    currentIndex = 0
                }
                if (VideoSettings.screenshotFormat === "JPG") {
                    currentIndex = 1
                }
                if (VideoSettings.screenshotFormat === "WebP") {
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
            text: VideoSettings.screenshotTemplate
            onEditingFinished: {
                VideoSettings.screenshotTemplate = text
                VideoSettings.save()
                mpv.setProperty("screenshot-template", VideoSettings.screenshotTemplate)
            }
        }
        Item {
            Layout.fillWidth: true
        }
    }
}
