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
import Qt.labs.platform 1.0 as Platform

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay


SettingsBasePage {
    id: root

    Platform.FileDialog {
        id: playlistToLoadOnStartupDialog

        folder: LocationSettings.cPlayFileLocation !== ""
                ? app.pathToUrl(LocationSettings.cPlayFileLocation)
                : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        title: "Choose playlist to load on startup"
        fileMode: Platform.FileDialog.OpenFile
        nameFilters: [ "C-Play playlist (*.cplaylist)", "Uniview playlist (*.playlist)" ]

        onAccepted: {
            var filePath = playerController.returnRelativeOrAbsolutePath(playlistToLoadOnStartupDialog.file.toString());
            PlaylistSettings.playlistToLoadOnStartup = filePath
            PlaylistSettings.save()

            mpv.focus = true
        }
        onRejected: mpv.focus = true
    }

    GridLayout {
        id: content

        columns: 2

        SettingsHeader {
            text: qsTr("Playlist settings")
            Layout.columnSpan: 2
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("Playlist/file to load on startup:")
        }
        RowLayout {
            TextField {
                id: playlistToLoadOnStartupText
                text: PlaylistSettings.playlistToLoadOnStartup
                placeholderText: "Path to playlist"
                onEditingFinished: {
                    PlaylistSettings.playlistToLoadOnStartup = text
                    PlaylistSettings.save()
                }

                ToolTip {
                    text: qsTr("Path to playlist")
                }
            }
            ToolButton {
                id: playlistToLoadOnStartupButton
                text: ""
                icon.name: "system-file-manager"
                icon.height: 16
                focusPolicy: Qt.NoFocus

                onClicked: {
                    playlistToLoadOnStartupDialog.open()
                }
            }
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("Position")
            Layout.alignment: Qt.AlignRight
        }

        ComboBox {
            textRole: "key"
            model: ListModel {
                ListElement { key: "Left"; value: "left" }
                ListElement { key: "Right"; value: "right" }
            }
            Component.onCompleted: {
                for (let i = 0; i < model.count; ++i) {
                    if (model.get(i).value === PlaylistSettings.position) {
                        currentIndex = i
                        break
                    }
                }
            }
            onActivated: {
                PlaylistSettings.position = model.get(index).value
                PlaylistSettings.save()
                playList.position = model.get(index).value
            }
        }

        Item { width: 1; height: 1 }
        CheckBox {
            checked: PlaylistSettings.autoPlayOnLoad
            text: qsTr("Auto-play videos in playlist (at startup)")
            onCheckStateChanged: {
                PlaylistSettings.autoPlayOnLoad = checked
                PlaylistSettings.save()
            }
        }

        Item { width: 1; height: 1 }
        Label {
            text: qsTr("Runtime auto-play ON/OFF is available in the playlist widget")
            font.italic: true
        }

        Item { width: 1; height: 1 }
        CheckBox {
            checked: PlaylistSettings.autoPlayNext
            text: qsTr("Also auto-play when (continue to next)")
            onCheckStateChanged: {
                PlaylistSettings.autoPlayNext = checked
                PlaylistSettings.save()
            }
        }

        Label {
            text: qsTr("Autoplay after certain seconds:")
        }

        RowLayout {
            SpinBox {
                id: autoPlayWaitime
                from: 0
                to: 10
                value: PlaylistSettings.autoPlayAfterTime
                enabled: PlaylistSettings.autoPlayNext
                onValueChanged: {
                    PlaylistSettings.autoPlayAfterTime = value
                    PlaylistSettings.save()
                }
            }

            LabelWithTooltip {
                text: {
                    qsTr("s = Plays next file (if continue to next) in %1 seconds").arg(Number((autoPlayWaitime.value*1.0)).toFixed(3))
                }
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        Item { width: 1; height: 1 }
        CheckBox {
            checked: PlaylistSettings.repeat
            text: qsTr("Repeat playlist (when continue to next is selected)")
            onCheckStateChanged: {
                PlaylistSettings.repeat = checked
                PlaylistSettings.save()
            }
        }

        Item { width: 1; height: 1 }
        CheckBox {
            checked: PlaylistSettings.loadSiblings
            text: qsTr("Auto load videos from same folder")
            onCheckStateChanged: {
                PlaylistSettings.loadSiblings = checked
                PlaylistSettings.save()
            }
        }

        Item { width: 1; height: 1 }
        CheckBox {
            checked: PlaylistSettings.showMediaTitle
            text: qsTr("Show media title instead of file name")
            onCheckStateChanged: {
                PlaylistSettings.showMediaTitle = checked
                PlaylistSettings.save()
            }
        }

        Item { width: 1; height: 1 }
        CheckBox {
            checked: PlaylistSettings.showRowNumber
            text: qsTr("Show row number")
            onCheckStateChanged: {
                PlaylistSettings.showRowNumber = checked
                PlaylistSettings.save()
            }
        }

        Item {
            width: Kirigami.Units.gridUnit
            height: Kirigami.Units.gridUnit
        }
    }
}
