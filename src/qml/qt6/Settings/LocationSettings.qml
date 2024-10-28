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

        currentFolder: LocationSettings.fileDialogLocation !== "" ? app.pathToUrl(LocationSettings.fileDialogLocation) : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        title: "Choose Primary File Dialog Location"

        onAccepted: {
            var filePath = openPrimaryFileDialogLocation.selectedFolder.toString();
            // remove prefixed "file:///"
            filePath = filePath.replace(/^(file:\/{3})/, "");
            fileDialogLocationText.text = filePath;
            LocationSettings.fileDialogLocation = fileDialogLocationText.text;
            LocationSettings.save();
            mpv.focus = true;
        }
        onRejected: mpv.focus = true
    }
    FolderDialog {
        id: openCPlayFileLocation

        currentFolder: LocationSettings.cPlayFileLocation !== "" ? app.pathToUrl(LocationSettings.cPlayFileLocation) : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        title: "Choose Common C-Play File Location"

        onAccepted: {
            var filePath = openCPlayFileLocation.selectedFolder.toString();
            // remove prefixed "file:///"
            filePath = filePath.replace(/^(file:\/{3})/, "");
            cPlayFileLocationText.text = filePath;
            LocationSettings.cPlayFileLocation = cPlayFileLocationText.text;
            LocationSettings.save();
            mpv.focus = true;
        }
        onRejected: mpv.focus = true
    }
    FolderDialog {
        id: openCPlayMediaLocation

        currentFolder: LocationSettings.cPlayMediaLocation !== "" ? app.pathToUrl(LocationSettings.cPlayMediaLocation) : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        title: "Choose Common C-Play Media Location"

        onAccepted: {
            var filePath = openCPlayMediaLocation.selectedFolder.toString();
            // remove prefixed "file:///"
            filePath = filePath.replace(/^(file:\/{3})/, "");
            cPlayMediaLocationText.text = filePath;
            LocationSettings.cPlayMediaLocation = cPlayMediaLocationText.text;
            LocationSettings.save();
            mpv.focus = true;
        }
        onRejected: mpv.focus = true
    }
    FolderDialog {
        id: openUniviewVideoLocation

        currentFolder: LocationSettings.univiewVideoLocation !== "" ? app.pathToUrl(LocationSettings.univiewVideoLocation) : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        title: "Choose Common Uniview Media Location"

        onAccepted: {
            var filePath = openUniviewVideoLocation.selectedFolder.toString();
            // remove prefixed "file:///"
            filePath = filePath.replace(/^(file:\/{3})/, "");
            univiewVideoLocationText.text = filePath;
            LocationSettings.univiewVideoLocation = univiewVideoLocationText.text;
            LocationSettings.save();
            mpv.focus = true;
        }
        onRejected: mpv.focus = true
    }
    FolderDialog {
        id: videoFileDialogLocation

        currentFolder: LocationSettings.videoFileDialogLocation !== "" ? app.pathToUrl(LocationSettings.videoFileDialogLocation) : app.pathToUrl(LocationSettings.videoFileDialogLastLocation)
        title: "Choose Common Media Location For Video Files"

        onAccepted: {
            var filePath = videoFileDialogLocation.selectedFolder.toString();
            // remove prefixed "file:///"
            filePath = filePath.replace(/^(file:\/{3})/, "");
            videoFileDialogLocationText.text = filePath;
            LocationSettings.videoFileDialogLocation = videoFileDialogLocationText.text;
            LocationSettings.save();
            mpv.focus = true;
        }
        onRejected: mpv.focus = true
    }
    FolderDialog {
        id: imageFileDialogLocation

        currentFolder: LocationSettings.imageFileDialogLocation !== "" ? app.pathToUrl(LocationSettings.imageFileDialogLocation) : app.pathToUrl(LocationSettings.imageFileDialogLastLocation)
        title: "Choose Common Media Location For Image Files"

        onAccepted: {
            var filePath = imageFileDialogLocation.selectedFolder.toString();
            // remove prefixed "file:///"
            filePath = filePath.replace(/^(file:\/{3})/, "");
            imageFileDialogLocationText.text = filePath;
            LocationSettings.imageFileDialogLocation = imageFileDialogLocationText.text;
            LocationSettings.save();
            mpv.focus = true;
        }
        onRejected: mpv.focus = true
    }
    FolderDialog {
        id: audioFileDialogLocation

        currentFolder: LocationSettings.audioFileDialogLocation !== "" ? app.pathToUrl(LocationSettings.audioFileDialogLocation) : app.pathToUrl(LocationSettings.audioFileDialogLastLocation)
        title: "Choose Common Media Location For Audio Files"

        onAccepted: {
            var filePath = audioFileDialogLocation.selectedFolder.toString();
            // remove prefixed "file:///"
            filePath = filePath.replace(/^(file:\/{3})/, "");
            audioFileDialogLocationText.text = filePath;
            LocationSettings.audioFileDialogLocation = audioFileDialogLocationText.text;
            LocationSettings.save();
            mpv.focus = true;
        }
        onRejected: mpv.focus = true
    }
    FolderDialog {
        id: pdfFileDialogLocation

        currentFolder: LocationSettings.pdfFileDialogLocation !== "" ? app.pathToUrl(LocationSettings.pdfFileDialogLocation) : app.pathToUrl(LocationSettings.pdfFileDialogLastLocation)
        title: "Choose Common Media Location For PDF Files"

        onAccepted: {
            var filePath = pdfFileDialogLocation.selectedFolder.toString();
            // remove prefixed "file:///"
            filePath = filePath.replace(/^(file:\/{3})/, "");
            pdfFileDialogLocationText.text = filePath;
            LocationSettings.pdfFileDialogLocation = pdfFileDialogLocationText.text;
            LocationSettings.save();
            mpv.focus = true;
        }
        onRejected: mpv.focus = true
    }
    GridLayout {
        id: content

        columns: 3

        SettingsHeader {
            Layout.columnSpan: 3
            Layout.fillWidth: true
            text: qsTr("Relative path settings")
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Common C-play file location")
        }
        RowLayout {
            Layout.columnSpan: 2
            Layout.fillWidth: true
            height: cPlayFileLocation.height

            TextField {
                id: cPlayFileLocationText

                placeholderText: qsTr("Set for relative cplayfile/list paths")
                text: LocationSettings.cPlayFileLocation

                onEditingFinished: {
                    LocationSettings.cPlayFileLocation = cPlayFileLocationText.text;
                    LocationSettings.save();
                }

                ToolTip {
                    text: qsTr("Common directory for where the C-play file(s) are stored.")
                }
            }
            ToolButton {
                id: cPlayFileLocationLoadButton

                focusPolicy: Qt.NoFocus
                icon.height: 16
                icon.name: "system-file-manager"
                text: ""

                onClicked: {
                    openCPlayFileLocation.open();
                }
            }
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Common C-play media location")
        }
        RowLayout {
            Layout.columnSpan: 2
            Layout.fillWidth: true
            height: cPlayMediaLocation.height

            TextField {
                id: cPlayMediaLocationText

                placeholderText: qsTr("Set for relative media paths")
                text: LocationSettings.cPlayMediaLocation

                onEditingFinished: {
                    LocationSettings.cPlayMediaLocation = cPlayMediaLocationText.text;
                    LocationSettings.save();
                }

                ToolTip {
                    text: qsTr("Common directory for where the media (video/audio/etc) are stored.")
                }
            }
            ToolButton {
                id: cPlayMediaLocationLoadButton

                focusPolicy: Qt.NoFocus
                icon.height: 16
                icon.name: "system-file-manager"
                text: ""

                onClicked: {
                    openCPlayMediaLocation.open();
                }
            }
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("File dialog location")
        }
        RowLayout {
            Layout.columnSpan: 2
            Layout.fillWidth: true
            height: fileDialogLocation.height

            TextField {
                id: fileDialogLocationText

                placeholderText: qsTr("Recommended empty...")
                text: LocationSettings.fileDialogLocation

                onEditingFinished: {
                    LocationSettings.fileDialogLocation = fileDialogLocationText.text;
                    LocationSettings.save();
                }

                ToolTip {
                    text: qsTr("If empty the file dialog will remember the last opened location.")
                }
            }
            ToolButton {
                id: fileDialogLocationLoadButton

                focusPolicy: Qt.NoFocus
                icon.height: 16
                icon.name: "system-file-manager"
                text: ""

                onClicked: {
                    openPrimaryFileDialogLocation.open();
                }
            }
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Video file dialog location")
        }
        RowLayout {
            Layout.columnSpan: 2
            Layout.fillWidth: true
            height: videoFileDialogLocation.height

            TextField {
                id: videoFileDialogLocationText

                placeholderText: qsTr("For first lookup...")
                text: LocationSettings.videoFileDialogLocation

                onEditingFinished: {
                    LocationSettings.videoFileDialogLocation = videoFileDialogLocationText.text;
                    LocationSettings.save();
                }

                ToolTip {
                    text: qsTr("Will be used first time after launch. Then last opened location will be used.")
                }
            }
            ToolButton {
                id: videoFileDialogLocationLoadButton

                focusPolicy: Qt.NoFocus
                icon.height: 16
                icon.name: "system-file-manager"
                text: ""

                onClicked: {
                    videoFileDialogLocation.open();
                }
            }
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Image file dialog location")
        }
        RowLayout {
            Layout.columnSpan: 2
            Layout.fillWidth: true
            height: imageFileDialogLocation.height

            TextField {
                id: imageFileDialogLocationText

                placeholderText: qsTr("For first lookup...")
                text: LocationSettings.imageFileDialogLocation

                onEditingFinished: {
                    LocationSettings.imageFileDialogLocation = imageFileDialogLocationText.text;
                    LocationSettings.save();
                }

                ToolTip {
                    text: qsTr("Will be used first time after launch. Then last opened location will be used.")
                }
            }
            ToolButton {
                id: imageFileDialogLocationLoadButton

                focusPolicy: Qt.NoFocus
                icon.height: 16
                icon.name: "system-file-manager"
                text: ""

                onClicked: {
                    imageFileDialogLocation.open();
                }
            }
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Audio file dialog location")
        }
        RowLayout {
            Layout.columnSpan: 2
            Layout.fillWidth: true
            height: audioFileDialogLocation.height

            TextField {
                id: audioFileDialogLocationText

                placeholderText: qsTr("For first lookup...")
                text: LocationSettings.audioFileDialogLocation

                onEditingFinished: {
                    LocationSettings.audioFileDialogLocation = audioFileDialogLocationText.text;
                    LocationSettings.save();
                }

                ToolTip {
                    text: qsTr("Will be used first time after launch. Then last opened location will be used.")
                }
            }
            ToolButton {
                id: audioFileDialogLocationLoadButton

                focusPolicy: Qt.NoFocus
                icon.height: 16
                icon.name: "system-file-manager"
                text: ""

                onClicked: {
                    audioFileDialogLocation.open();
                }
            }
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("PDF file dialog location")
        }
        RowLayout {
            Layout.columnSpan: 2
            Layout.fillWidth: true
            height: pdfFileDialogLocation.height

            TextField {
                id: pdfFileDialogLocationText

                placeholderText: qsTr("For first lookup...")
                text: LocationSettings.pdfFileDialogLocation

                onEditingFinished: {
                    LocationSettings.pdfFileDialogLocation = pdfFileDialogLocationText.text;
                    LocationSettings.save();
                }

                ToolTip {
                    text: qsTr("Will be used first time after launch. Then last opened location will be used.")
                }
            }
            ToolButton {
                id: pdfFileDialogLocationLoadButton

                focusPolicy: Qt.NoFocus
                icon.height: 16
                icon.name: "system-file-manager"
                text: ""

                onClicked: {
                    pdfFileDialogLocation.open();
                }
            }
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Uniview video location")
        }
        RowLayout {
            Layout.columnSpan: 2
            Layout.fillWidth: true
            height: univiewVideoLocation.height

            TextField {
                id: univiewVideoLocationText

                placeholderText: qsTr("Only needed to load *.fdv files")
                text: LocationSettings.univiewVideoLocation

                onEditingFinished: {
                    LocationSettings.univiewVideoLocation = univiewVideoLocationText.text;
                    LocationSettings.save();
                }

                ToolTip {
                    text: qsTr("Common directory where the Uniview video files are stored.")
                }
            }
            ToolButton {
                id: univiewVideoLocationLoadButton

                focusPolicy: Qt.NoFocus
                icon.height: 16
                icon.name: "system-file-manager"
                text: ""

                onClicked: {
                    openUniviewVideoLocation.open();
                }
            }
        }
        Item {
            Layout.columnSpan: 3
            Layout.fillWidth: true
            // spacer item
            height: 20
        }

        // ------------------------------------
        // Screenshot Format
        // ------------------------------------
        SettingsHeader {
            Layout.columnSpan: 3
            Layout.fillWidth: true
            text: qsTr("Screenshots")
            topMargin: 0
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Format")
        }
        ComboBox {
            id: screenshotFormat

            textRole: "key"

            model: ListModel {
                ListElement {
                    key: "PNG"
                }
                ListElement {
                    key: "JPG"
                }
                ListElement {
                    key: "WebP"
                }
            }

            Component.onCompleted: {
                if (LocationSettings.screenshotFormat === "PNG") {
                    currentIndex = 0;
                }
                if (LocationSettings.screenshotFormat === "JPG") {
                    currentIndex = 1;
                }
                if (LocationSettings.screenshotFormat === "WebP") {
                    currentIndex = 2;
                }
            }
            onActivated: {
                LocationSettings.screenshotFormat = model.get(index).key;
                LocationSettings.save();
                mpv.setProperty("screenshot-format", LocationSettings.screenshotFormat);
            }
        }
        Item {
            Layout.fillWidth: true
        }

        // ------------------------------------
        // Screenshot template
        // ------------------------------------
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Template")
        }
        TextField {
            id: screenshotTemplate

            text: LocationSettings.screenshotTemplate

            onEditingFinished: {
                LocationSettings.screenshotTemplate = text;
                LocationSettings.save();
                mpv.setProperty("screenshot-template", LocationSettings.screenshotTemplate);
            }
        }
        Item {
            Layout.fillWidth: true
        }
    }
}
