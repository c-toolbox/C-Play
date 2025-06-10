/*
 * SPDX-FileCopyrightText:
 * 2021-2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Dialogs
import Qt.labs.platform 1.0 as Platform

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

Kirigami.ApplicationWindow {
    function updateValues(selectedIndex) {
        fileTextField.text = mpv.playlistModel.filePath(selectedIndex);
        mediaFileTextField.text = mpv.playlistModel.mediaFile(selectedIndex);
        durationLabel.text = mpv.playlistModel.duration(selectedIndex);
        sectionsLabel.text = qsTr("%1").arg(Number(mpv.playlistModel.numberOfSections(selectedIndex)));
        mediaTitleTextField.text = mpv.playlistModel.mediaTitle(selectedIndex);
        if (mpv.playlistModel.stereoVideo(selectedIndex) === 0) {
            stereoscopicModeLabel.text = qsTr("2D (Mono)");
        } else if (mpv.playlistModel.stereoVideo(selectedIndex) === 1) {
            stereoscopicModeLabel.text = qsTr("3D (Side-by-side)");
        } else if (mpv.playlistModel.stereoVideo(selectedIndex) === 2) {
            stereoscopicModeLabel.text = qsTr("3D (Top-Bottom)");
        } else if (mpv.playlistModel.stereoVideo(selectedIndex) === 3) {
            stereoscopicModeLabel.text = qsTr("3D (Top-Bottom+Flip)");
        }
        if (mpv.playlistModel.gridToMapOn(selectedIndex) === 0) {
            gridModeLabel.text = qsTr("None (normal for pre-splittning)");
        } else if (mpv.playlistModel.gridToMapOn(selectedIndex) === 1) {
            gridModeLabel.text = qsTr("Flat content mappend on a plane in space");
        } else if (mpv.playlistModel.gridToMapOn(selectedIndex) === 2) {
            gridModeLabel.text = qsTr("Dome with Fulldome / Fisheye projection");
        } else if (mpv.playlistModel.gridToMapOn(selectedIndex) === 3) {
            gridModeLabel.text = qsTr("360 sphere with equirectangular content");
        } else if (mpv.playlistModel.gridToMapOn(selectedIndex) === 4) {
            gridModeLabel.text = qsTr("360 sphere with equi-angular cubemap content");
        }
        separateAudioFileTextField.text = mpv.playlistModel.separateAudioFile(selectedIndex);
        separateOverlayFileTextField.text = mpv.playlistModel.separateOverlayFile(selectedIndex);
    }

    height: 300
    title: qsTr("View Playlist Item")
    visible: false
    width: 600

    GridLayout {
        anchors.fill: parent
        anchors.margins: 15
        columnSpacing: 2
        columns: 2
        rowSpacing: 8

        RowLayout {
            Layout.bottomMargin: 5
            Layout.columnSpan: 2

            Rectangle {
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
                width: Kirigami.Units.gridUnit
            }
            Label {
                text: qsTr("The properties of the selected entry in the playlist.")
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

            ToolTip {
                text: qsTr("Path to the description file.")
            }
        }
        Label {
            text: qsTr("Media file:")
        }
        TextField {
            id: mediaFileTextField

            Layout.fillWidth: true
            readOnly: true

            ToolTip {
                text: qsTr("Path to the actual media file to be played.")
            }
        }
        Label {
            text: qsTr("Duration:")
        }
        Label {
            id: durationLabel

            Layout.fillWidth: true

            ToolTip {
                text: qsTr("Duration of the clip")
            }
        }
        Label {
            text: qsTr("Sections:")
        }
        Label {
            id: sectionsLabel

            Layout.fillWidth: true

            ToolTip {
                text: qsTr("Duration of the clip")
            }
        }
        Label {
            text: qsTr("Media title:")
        }
        TextField {
            id: mediaTitleTextField

            Layout.fillWidth: true
            placeholderText: "A title for playlists etc"
            readOnly: true

            ToolTip {
                text: qsTr("A title for playlists etc")
            }
        }
        Label {
            text: qsTr("Stereoscopic mode:")
        }
        Label {
            id: stereoscopicModeLabel

            Layout.fillWidth: true

            ToolTip {
                text: qsTr("Stereoscopic mode of the clip.")
            }
        }
        Label {
            text: qsTr("Grid mode:")
        }
        Label {
            id: gridModeLabel

            Layout.fillWidth: true

            ToolTip {
                text: qsTr("Mapping mode of the clip.")
            }
        }
        Label {
            text: qsTr("Separate Audio File:")
        }
        RowLayout {
            Layout.fillWidth: true

            TextField {
                id: separateAudioFileTextField

                Layout.fillWidth: true
                readOnly: true
            }
        }
        Label {
            text: qsTr("Separate Overlay File:")
        }
        RowLayout {
            Layout.fillWidth: true

            TextField {
                id: separateOverlayFileTextField

                Layout.fillWidth: true
                readOnly: true
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
