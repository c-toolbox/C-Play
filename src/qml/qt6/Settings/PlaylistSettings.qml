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
import Qt.labs.platform 1.0 as Platform

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

SettingsBasePage {
    id: root

    Platform.FileDialog {
        id: playlistToLoadOnStartupDialog

        fileMode: Platform.FileDialog.OpenFile
        folder: LocationSettings.cPlayFileLocation !== "" ? app.pathToUrl(LocationSettings.cPlayFileLocation) : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        nameFilters: ["C-Play playlist (*.cplaylist)", "Uniview playlist (*.playlist)"]
        title: "Choose playlist to load on startup"

        onAccepted: {
            var filePath = playerController.returnRelativeOrAbsolutePath(playlistToLoadOnStartupDialog.file.toString());
            PlaylistSettings.playlistToLoadOnStartup = filePath;
            PlaylistSettings.save();
            mpv.focus = true;
        }
        onRejected: mpv.focus = true
    }
    GridLayout {
        id: content

        columns: 2

        SettingsHeader {
            Layout.columnSpan: 2
            Layout.fillWidth: true
            text: qsTr("Playlist settings")
        }
        Label {
            text: qsTr("Playlist/file to load on startup:")
        }
        RowLayout {
            Layout.fillWidth: true

            TextField {
                id: playlistToLoadOnStartupText

                placeholderText: "Path to playlist"
                text: PlaylistSettings.playlistToLoadOnStartup

                onEditingFinished: {
                    PlaylistSettings.playlistToLoadOnStartup = text;
                    PlaylistSettings.save();
                }

                ToolTip {
                    text: qsTr("Path to playlist")
                }
            }
            ToolButton {
                id: playlistToLoadOnStartupButton

                focusPolicy: Qt.NoFocus
                icon.height: 16
                icon.name: "document-open"
                text: ""

                onClicked: {
                    playlistToLoadOnStartupDialog.open();
                }
            }
        }
        Item {
            height: 1
            width: 1
        }
        Label {
            font.italic: true
            text: qsTr("Command line argument \"--loadfile\" overrides above path.")
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Position")
        }
        ComboBox {
            textRole: "key"

            model: ListModel {
                ListElement {
                    key: "Left"
                    value: "left"
                }
                ListElement {
                    key: "Right"
                    value: "right"
                }
            }

            Component.onCompleted: {
                for (let i = 0; i < model.count; ++i) {
                    if (model.get(i).value === PlaylistSettings.position) {
                        currentIndex = i;
                        break;
                    }
                }
            }
            onActivated: {
                PlaylistSettings.position = model.get(index).value;
                PlaylistSettings.save();
                playList.position = model.get(index).value;
            }
        }
        Item {
            height: 1
            width: 1
        }
        CheckBox {
            checked: PlaylistSettings.autoPlayOnLoad
            text: qsTr("Auto-play videos in playlist (at startup)")

            onCheckStateChanged: {
                PlaylistSettings.autoPlayOnLoad = checked;
                PlaylistSettings.save();
            }
        }
        Item {
            height: 1
            width: 1
        }
        Label {
            font.italic: true
            text: qsTr("Runtime auto-play ON/OFF is available in the playlist widget")
        }
        Item {
            height: 1
            width: 1
        }
        CheckBox {
            checked: PlaylistSettings.autoPlayNext
            text: qsTr("Also auto-play when (continue to next)")

            onCheckStateChanged: {
                PlaylistSettings.autoPlayNext = checked;
                PlaylistSettings.save();
            }
        }
        Label {
            text: qsTr("Autoplay after certain seconds:")
        }
        RowLayout {
            SpinBox {
                id: autoPlayWaitime

                enabled: PlaylistSettings.autoPlayNext
                from: 0
                to: 10
                value: PlaylistSettings.autoPlayAfterTime

                onValueChanged: {
                    PlaylistSettings.autoPlayAfterTime = value;
                    PlaylistSettings.save();
                }
            }
            LabelWithTooltip {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: {
                    qsTr("s = Plays next file (if continue to next) in %1 seconds").arg(Number((autoPlayWaitime.value * 1.0)).toFixed(3));
                }
            }
        }
        Item {
            height: 1
            width: 1
        }
        CheckBox {
            checked: PlaylistSettings.repeat
            text: qsTr("Repeat playlist (when continue to next is selected)")

            onCheckStateChanged: {
                PlaylistSettings.repeat = checked;
                PlaylistSettings.save();
            }
        }
        Item {
            height: 1
            width: 1
        }
        CheckBox {
            checked: PlaylistSettings.loadSiblings
            text: qsTr("Auto load videos from same folder")

            onCheckStateChanged: {
                PlaylistSettings.loadSiblings = checked;
                PlaylistSettings.save();
            }
        }
        Item {
            height: 1
            width: 1
        }
        CheckBox {
            checked: PlaylistSettings.showMediaTitle
            text: qsTr("Show media title instead of file name")

            onCheckStateChanged: {
                PlaylistSettings.showMediaTitle = checked;
                PlaylistSettings.save();
            }
        }
        Item {
            height: 1
            width: 1
        }
        CheckBox {
            checked: PlaylistSettings.showRowNumber
            text: qsTr("Show row number")

            onCheckStateChanged: {
                PlaylistSettings.showRowNumber = checked;
                PlaylistSettings.save();
            }
        }
        Item {
            height: Kirigami.Units.gridUnit
            width: Kirigami.Units.gridUnit
        }
    }
}
