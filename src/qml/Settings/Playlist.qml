/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.12
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.12

import org.kde.kirigami 2.11 as Kirigami
import org.ctoolbox.cplay 1.0
import Haruna.Components 1.0

SettingsBasePage {
    id: root

    GridLayout {
        id: content

        columns: 2

        SettingsHeader {
            text: qsTr("Playlist settings")
            Layout.columnSpan: 2
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
                    qsTr("ms = Plays next file (if continue to next) in %1 seconds").arg(Number((timeSetInterval.value*1.0)/1000.0).toFixed(3))
                }
                elide: Text.ElideRight
                Layout.fillWidth: true
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
            checked: PlaylistSettings.loadSiblings
            text: qsTr("Auto load videos from same folder")
            onCheckStateChanged: {
                PlaylistSettings.loadSiblings = checked
                PlaylistSettings.save()
            }
        }

        Item { width: 1; height: 1 }
        CheckBox {
            checked: PlaylistSettings.repeat
            text: qsTr("Repeat")
            onCheckStateChanged: {
                PlaylistSettings.repeat = checked
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
