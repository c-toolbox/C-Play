/*
 * SPDX-FileCopyrightText:
 * 2021-2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Dialogs

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

Kirigami.ApplicationWindow {
    property int selectedIndex: -1
    property bool hasDescFile: false

    function updateValues(idx) {
        viewPlaylistItemWindow.selectedIndex = idx;
        if (idx < 0) return;

        hasDescFile = mpv.playlistModel.hasDescriptionFile(idx);

        fileTextField.text = mpv.playlistModel.filePath(idx);
        mediaFileTextField.text = mpv.playlistModel.mediaFile(idx);
        mediaTitleLabel.text = hasDescFile ? mpv.playlistModel.mediaTitle(idx) : qsTr("N/A");
        durationLabel.text = hasDescFile ? mpv.playlistModel.duration(idx) : qsTr("N/A");
        sectionsLabel.text = hasDescFile ? qsTr("%1").arg(Number(mpv.playlistModel.numberOfSections(idx))) : qsTr("N/A");

        if (hasDescFile) {
            if (mpv.playlistModel.stereoVideo(idx) === 0) {
                stereoscopicModeLabel.text = qsTr("2D (Mono)");
            } else if (mpv.playlistModel.stereoVideo(idx) === 1) {
                stereoscopicModeLabel.text = qsTr("3D (Side-by-side)");
            } else if (mpv.playlistModel.stereoVideo(idx) === 2) {
                stereoscopicModeLabel.text = qsTr("3D (Top-Bottom)");
            } else if (mpv.playlistModel.stereoVideo(idx) === 3) {
                stereoscopicModeLabel.text = qsTr("3D (Top-Bottom+Flip)");
            }
        } else {
            stereoscopicModeLabel.text = qsTr("N/A");
        }

        if (hasDescFile) {
            if (mpv.playlistModel.gridToMapOn(idx) === 0) {
                gridModeLabel.text = qsTr("None (normal for pre-splitting)");
            } else if (mpv.playlistModel.gridToMapOn(idx) === 1) {
                gridModeLabel.text = qsTr("Flat content mapped on a plane in space");
            } else if (mpv.playlistModel.gridToMapOn(idx) === 2) {
                gridModeLabel.text = qsTr("Dome with Fulldome / Fisheye projection");
            } else if (mpv.playlistModel.gridToMapOn(idx) === 3) {
                gridModeLabel.text = qsTr("360 sphere with equirectangular content");
            } else if (mpv.playlistModel.gridToMapOn(idx) === 4) {
                gridModeLabel.text = qsTr("360 sphere with equi-angular cubemap content");
            }
        } else {
            gridModeLabel.text = qsTr("N/A");
        }

        separateAudioFileTextField.text = hasDescFile ? mpv.playlistModel.separateAudioFile(idx) : "";
        separateOverlayFileTextField.text = hasDescFile ? mpv.playlistModel.separateOverlayFile(idx) : "";

        var eofMode = mpv.playlistModel.eofMode(idx);
        eofComboBox.currentIndex = eofMode;

        listTitleCheckBox.checked = mpv.playlistModel.useListTitle(idx);
        listTitleTextField.text = mpv.playlistModel.listTitle(idx);

        listAudioCheckBox.checked = mpv.playlistModel.useListAudioFile(idx);
        listAudioTextField.text = mpv.playlistModel.listAudioFile(idx);

        listFileStartCheckBox.checked = mpv.playlistModel.useListFileStartTime(idx);
        listFileStartTimeTextField.text = mpv.playlistModel.listFileStartTimeFormatted(idx);

        listFileEndCheckBox.checked = mpv.playlistModel.useListFileEndTime(idx);
        listFileEndTimeTextField.text = mpv.playlistModel.listFileEndTimeFormatted(idx);

        listStereoCheckBox.checked = mpv.playlistModel.useListStereoMode(idx);
        listStereoComboBox.currentIndex = mpv.playlistModel.listStereoMode(idx);

        listGridCheckBox.checked = mpv.playlistModel.useListGridMode(idx);
        listGridComboBox.currentIndex = mpv.playlistModel.listGridMode(idx);
    }

    id: viewPlaylistItemWindow
    height: 660
    title: qsTr("View Playlist Item")
    visible: false
    width: 620

    onVisibleChanged: {
        if (visible && selectedIndex < 0) {
            var idx = mpv.playlistModel.getPlayingVideo();
            if (idx < 0) idx = 0;
            if (idx >= 0 && idx < mpv.playlistModel.getPlayListSize()) {
                updateValues(idx);
            }
        }
    }

    GridLayout {
        anchors.fill: parent
        anchors.margins: 15
        columnSpacing: 2
        columns: 2
        rowSpacing: 6

        // ?? cplayfile header ??
        RowLayout {
            Layout.bottomMargin: 3
            Layout.columnSpan: 2

            Rectangle {
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
                width: Kirigami.Units.gridUnit
            }
            Label {
                text: qsTr("Properties from cplayfile (read-only)")
            }
            Rectangle {
                Layout.fillWidth: true
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
            }
        }
        Label {
            text: qsTr("File:")
        }
        TextField {
            id: fileTextField

            Layout.fillWidth: true
            readOnly: true
            opacity: 0.7

            ToolTip {
                text: qsTr("Path to the description file.")
            }
        }
        Label {
            text: qsTr("Media file:")
            font.strikeout: !hasDescFile
        }
        TextField {
            id: mediaFileTextField

            Layout.fillWidth: true
            readOnly: true
            opacity: hasDescFile ? 0.7 : 0.35
            font.strikeout: !hasDescFile

            ToolTip {
                text: qsTr("Path to the actual media file to be played.")
            }
        }
        Label {
            text: qsTr("Media title:")
            font.strikeout: !hasDescFile
        }
        Label {
            id: mediaTitleLabel

            Layout.fillWidth: true
            opacity: hasDescFile ? 0.7 : 0.35
            font.strikeout: !hasDescFile

            ToolTip {
                text: qsTr("Title from the media description file (read-only)")
            }
        }
        Label {
            text: qsTr("Duration:")
            font.strikeout: !hasDescFile
        }
        Label {
            id: durationLabel

            Layout.fillWidth: true
            opacity: hasDescFile ? 0.7 : 0.35
            font.strikeout: !hasDescFile

            ToolTip {
                text: qsTr("Duration of the clip")
            }
        }
        Label {
            text: qsTr("Sections:")
            font.strikeout: !hasDescFile
        }
        Label {
            id: sectionsLabel

            Layout.fillWidth: true
            opacity: hasDescFile ? 0.7 : 0.35
            font.strikeout: !hasDescFile

            ToolTip {
                text: qsTr("Number of sections in the clip")
            }
        }
        Label {
            text: qsTr("Stereoscopic mode:")
            font.strikeout: !hasDescFile
        }
        Label {
            id: stereoscopicModeLabel

            Layout.fillWidth: true
            opacity: hasDescFile ? 0.7 : 0.35
            font.strikeout: !hasDescFile

            ToolTip {
                text: qsTr("Stereoscopic mode of the clip.")
            }
        }
        Label {
            text: qsTr("Grid mode:")
            font.strikeout: !hasDescFile
        }
        Label {
            id: gridModeLabel

            Layout.fillWidth: true
            opacity: hasDescFile ? 0.7 : 0.35
            font.strikeout: !hasDescFile

            ToolTip {
                text: qsTr("Mapping mode of the clip.")
            }
        }
        Label {
            text: qsTr("Separate Audio File:")
            font.strikeout: !hasDescFile
        }
        TextField {
            id: separateAudioFileTextField

            Layout.fillWidth: true
            readOnly: true
            opacity: hasDescFile ? 0.7 : 0.35
            font.strikeout: !hasDescFile
        }
        Label {
            text: qsTr("Separate Overlay File:")
            font.strikeout: !hasDescFile
        }
        TextField {
            id: separateOverlayFileTextField

            Layout.fillWidth: true
            readOnly: true
            opacity: hasDescFile ? 0.7 : 0.35
            font.strikeout: !hasDescFile
        }

        // ?? cplaylist header ??
        RowLayout {
            Layout.topMargin: 6
            Layout.bottomMargin: 3
            Layout.columnSpan: 2

            Rectangle {
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
                width: Kirigami.Units.gridUnit
            }
            Label {
                font.bold: true
                text: qsTr("Playlist entry overrides (saved in cplaylist)")
            }
            Rectangle {
                Layout.fillWidth: true
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
            }
        }
        Label {
            text: qsTr("End of file:")
        }
        ComboBox {
            id: eofComboBox

            Layout.fillWidth: true
            model: ["Pause", "Continue to next", "Loop"]

            onActivated: {
                if (viewPlaylistItemWindow.selectedIndex >= 0) {
                    mpv.playlistModel.setEofMode(viewPlaylistItemWindow.selectedIndex, currentIndex);
                    mpv.playlistModel.updateItem(viewPlaylistItemWindow.selectedIndex);
                }
            }

            ToolTip {
                text: qsTr("Action when the file reaches the end")
            }
        }
        CheckBox {
            id: listTitleCheckBox

            text: qsTr("List title:")

            onToggled: {
                if (viewPlaylistItemWindow.selectedIndex >= 0) {
                    mpv.playlistModel.setUseListTitle(viewPlaylistItemWindow.selectedIndex, checked);
                    mpv.playlistModel.updateItem(viewPlaylistItemWindow.selectedIndex);
                }
            }

            ToolTip {
                text: qsTr("Enable to override the displayed title for this playlist entry (optional)")
            }
        }
        TextField {
            id: listTitleTextField

            Layout.fillWidth: true
            enabled: listTitleCheckBox.checked
            placeholderText: qsTr("Custom title for this playlist entry")

            onEditingFinished: {
                if (viewPlaylistItemWindow.selectedIndex >= 0) {
                    mpv.playlistModel.setListTitle(viewPlaylistItemWindow.selectedIndex, text);
                    if (text.length > 0 && !listTitleCheckBox.checked) {
                        listTitleCheckBox.checked = true;
                        mpv.playlistModel.setUseListTitle(viewPlaylistItemWindow.selectedIndex, true);
                    }
                    mpv.playlistModel.updateItem(viewPlaylistItemWindow.selectedIndex);
                }
            }

            ToolTip {
                text: qsTr("Overrides the displayed title for this playlist entry")
            }
        }
        CheckBox {
            id: listStereoCheckBox

            text: qsTr("List stereo mode:")

            onToggled: {
                if (viewPlaylistItemWindow.selectedIndex >= 0) {
                    mpv.playlistModel.setUseListStereoMode(viewPlaylistItemWindow.selectedIndex, checked);
                    mpv.playlistModel.updateItem(viewPlaylistItemWindow.selectedIndex);
                }
            }

            ToolTip {
                text: qsTr("Enable to override stereoscopic mode for this entry (optional)")
            }
        }
        ComboBox {
            id: listStereoComboBox

            Layout.fillWidth: true
            enabled: listStereoCheckBox.checked
            model: ["2D (Mono)", "3D (Side-by-side)", "3D (Top-Bottom)", "3D (Top-Bottom+Flip)"]

            onActivated: {
                if (viewPlaylistItemWindow.selectedIndex >= 0) {
                    mpv.playlistModel.setListStereoMode(viewPlaylistItemWindow.selectedIndex, currentIndex);
                    mpv.playlistModel.updateItem(viewPlaylistItemWindow.selectedIndex);
                }
            }

            ToolTip {
                text: qsTr("Override stereoscopic mode for this playlist entry")
            }
        }
        CheckBox {
            id: listGridCheckBox

            text: qsTr("List grid mode:")

            onToggled: {
                if (viewPlaylistItemWindow.selectedIndex >= 0) {
                    mpv.playlistModel.setUseListGridMode(viewPlaylistItemWindow.selectedIndex, checked);
                    mpv.playlistModel.updateItem(viewPlaylistItemWindow.selectedIndex);
                }
            }

            ToolTip {
                text: qsTr("Enable to override grid mode for this entry (optional)")
            }
        }
        ComboBox {
            id: listGridComboBox

            Layout.fillWidth: true
            enabled: listGridCheckBox.checked
            model: ["Pre-split", "Plane", "Dome", "Sphere EQR", "Sphere EAC"]

            onActivated: {
                if (viewPlaylistItemWindow.selectedIndex >= 0) {
                    mpv.playlistModel.setListGridMode(viewPlaylistItemWindow.selectedIndex, currentIndex);
                    mpv.playlistModel.updateItem(viewPlaylistItemWindow.selectedIndex);
                }
            }

            ToolTip {
                text: qsTr("Override grid mode for this playlist entry")
            }
        }
        CheckBox {
            id: listAudioCheckBox

            text: qsTr("List audio file:")

            onToggled: {
                if (viewPlaylistItemWindow.selectedIndex >= 0) {
                    mpv.playlistModel.setUseListAudioFile(viewPlaylistItemWindow.selectedIndex, checked);
                    mpv.playlistModel.updateItem(viewPlaylistItemWindow.selectedIndex);
                }
            }

            ToolTip {
                text: qsTr("Enable to override the audio file for this entry (optional)")
            }
        }
        RowLayout {
            Layout.fillWidth: true

            TextField {
                id: listAudioTextField

                Layout.fillWidth: true
                enabled: listAudioCheckBox.checked
                readOnly: true
                placeholderText: qsTr("No audio file selected")

                ToolTip {
                    text: qsTr("Override audio file for this playlist entry")
                }
            }
            Button {
                enabled: listAudioCheckBox.checked
                focusPolicy: Qt.NoFocus
                icon.name: "document-open"
                icon.height: 16

                onClicked: {
                    listAudioFileDialog.startFolder = mpv.playlistModel.mediaFileFolderPath(viewPlaylistItemWindow.selectedIndex);
                    Qt.callLater(function() {
                        listAudioFileDialog.open();
                    });
                }

                ToolTip {
                    text: qsTr("Browse for audio file")
                }
            }

            CPlayFileDialog {
                id: listAudioFileDialog

                property string startFolder: ""

                parentWindow: viewPlaylistItemWindow
                title: qsTr("Select Audio File")
                fileMode: CPlayFileDialog.OpenFile
                currentFolder: startFolder.length > 0 ? "file:///" + startFolder : ""
                nameFilters: [qsTr("Audio files") + " (*.wav *.mp3 *.flac *.ogg *.aac *.wma *.m4a *.opus *.aiff *.ac3 *.dts *.pcm)"]

                onAccepted: {
                    var filePath = selectedFile.toString().replace("file:///", "");
                    listAudioTextField.text = filePath;
                    if (viewPlaylistItemWindow.selectedIndex >= 0) {
                        mpv.playlistModel.setListAudioFile(viewPlaylistItemWindow.selectedIndex, filePath);
                        if (!listAudioCheckBox.checked) {
                            listAudioCheckBox.checked = true;
                            mpv.playlistModel.setUseListAudioFile(viewPlaylistItemWindow.selectedIndex, true);
                        }
                        mpv.playlistModel.updateItem(viewPlaylistItemWindow.selectedIndex);
                    }
                }
            }
        }
        CheckBox {
            id: listFileStartCheckBox

            text: qsTr("List file start:")

            onToggled: {
                if (viewPlaylistItemWindow.selectedIndex >= 0) {
                    mpv.playlistModel.setUseListFileStartTime(viewPlaylistItemWindow.selectedIndex, checked);
                    mpv.playlistModel.updateItem(viewPlaylistItemWindow.selectedIndex);
                }
            }

            ToolTip {
                text: qsTr("Enable to set a custom start time for this entry (optional)")
            }
        }
        RowLayout {
            Layout.fillWidth: true

            TextField {
                id: listFileStartTimeTextField

                Layout.fillWidth: true
                enabled: listFileStartCheckBox.checked
                placeholderText: qsTr("00:00:00")
                inputMask: "99:99:99"

                onEditingFinished: {
                    if (viewPlaylistItemWindow.selectedIndex >= 0) {
                        mpv.playlistModel.setListFileStartTimeFromString(viewPlaylistItemWindow.selectedIndex, text);
                    }
                }

                ToolTip {
                    text: qsTr("Start time in format hh:mm:ss")
                }
            }
            Button {
                enabled: listFileStartCheckBox.checked
                focusPolicy: Qt.NoFocus
                icon.name: "go-previous-skip"
                icon.height: 16

                onClicked: {
                    listFileStartTimeTextField.text = app.formatTime(mpv.position);
                    if (viewPlaylistItemWindow.selectedIndex >= 0) {
                        mpv.playlistModel.setListFileStartTimeFromString(viewPlaylistItemWindow.selectedIndex, listFileStartTimeTextField.text);
                    }
                }

                ToolTip {
                    text: qsTr("Copy current time from timeline")
                }
            }
            Button {
                enabled: listFileStartCheckBox.checked
                focusPolicy: Qt.NoFocus
                icon.name: "edit-entry"
                icon.height: 16

                onClicked: {
                    var sectionIdx = mpv.playSectionsModel.getPlayingSection();
                    if (sectionIdx >= 0) {
                        listFileStartTimeTextField.text = app.formatTime(mpv.playSectionsModel.sectionStartTime(sectionIdx));
                        if (viewPlaylistItemWindow.selectedIndex >= 0) {
                            mpv.playlistModel.setListFileStartTimeFromString(viewPlaylistItemWindow.selectedIndex, listFileStartTimeTextField.text);
                        }
                    }
                }

                ToolTip {
                    text: qsTr("Copy start time from loaded section")
                }
            }
        }
        CheckBox {
            id: listFileEndCheckBox

            text: qsTr("List file end:")

            onToggled: {
                if (viewPlaylistItemWindow.selectedIndex >= 0) {
                    mpv.playlistModel.setUseListFileEndTime(viewPlaylistItemWindow.selectedIndex, checked);
                    mpv.playlistModel.updateItem(viewPlaylistItemWindow.selectedIndex);
                }
            }

            ToolTip {
                text: qsTr("Enable to set a custom end time for this entry (optional)")
            }
        }
        RowLayout {
            Layout.fillWidth: true

            TextField {
                id: listFileEndTimeTextField

                Layout.fillWidth: true
                enabled: listFileEndCheckBox.checked
                placeholderText: qsTr("00:00:00")
                inputMask: "99:99:99"

                onEditingFinished: {
                    if (viewPlaylistItemWindow.selectedIndex >= 0) {
                        mpv.playlistModel.setListFileEndTimeFromString(viewPlaylistItemWindow.selectedIndex, text);
                    }
                }

                ToolTip {
                    text: qsTr("End time in format hh:mm:ss")
                }
            }
            Button {
                enabled: listFileEndCheckBox.checked
                focusPolicy: Qt.NoFocus
                icon.name: "go-previous-skip"
                icon.height: 16

                onClicked: {
                    listFileEndTimeTextField.text = app.formatTime(mpv.position);
                    if (viewPlaylistItemWindow.selectedIndex >= 0) {
                        mpv.playlistModel.setListFileEndTimeFromString(viewPlaylistItemWindow.selectedIndex, listFileEndTimeTextField.text);
                    }
                }

                ToolTip {
                    text: qsTr("Copy current time from timeline")
                }
            }
            Button {
                enabled: listFileEndCheckBox.checked
                focusPolicy: Qt.NoFocus
                icon.name: "edit-entry"
                icon.height: 16

                onClicked: {
                    var sectionIdx = mpv.playSectionsModel.getPlayingSection();
                    if (sectionIdx >= 0) {
                        listFileEndTimeTextField.text = app.formatTime(mpv.playSectionsModel.sectionEndTime(sectionIdx));
                        if (viewPlaylistItemWindow.selectedIndex >= 0) {
                            mpv.playlistModel.setListFileEndTimeFromString(viewPlaylistItemWindow.selectedIndex, listFileEndTimeTextField.text);
                        }
                    }
                }

                ToolTip {
                    text: qsTr("Copy end time from loaded section")
                }
            }
        }
        Item {
            Layout.columnSpan: 2
            Layout.fillHeight: true
            // spacer item
            Layout.fillWidth: true
        }
    }
}
