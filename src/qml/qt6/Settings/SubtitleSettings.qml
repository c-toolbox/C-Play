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

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

SettingsBasePage {
    id: root

    GridLayout {
        id: content

        columns: 3

        SettingsHeader {
            Layout.columnSpan: 3
            Layout.fillWidth: true
            text: qsTr("Subtitle settings (for media files)")
        }
        Item {
            Layout.columnSpan: 3
            // spacer item
            Layout.fillWidth: true
        }

        Item {
            height: 1
            width: 1
        }
        CheckBox {
            id: loadSubtitleFilesInVideoFolder

            checked: SubtitleSettings.loadSubtitleFileInVideoFolder
            enabled: true
            text: qsTr("Load subtitle files in same folder as video file.")

            onCheckedChanged: {
                SubtitleSettings.loadSubtitleFileInVideoFolder = checked;
                SubtitleSettings.save();
                if (checked) {
                    mpv.setProperty("sub-auto", "all");
                } else {
                    mpv.setProperty("sub-auto", "no");
                }
            }
        }        
        Item {
            // spacer item
            Layout.fillWidth: true
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Preferred language:")
        }
        TextField {
            placeholderText: "eng,ger etc."
            text: SubtitleSettings.preferredLanguage

            onTextEdited: {
                SubtitleSettings.preferredLanguage = text;
                SubtitleSettings.save();
                mpv.setProperty("slang", text);
            }

            ToolTip {
                text: qsTr("Do not use spaces.")
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Preferred track:")
        }
        SpinBox {
            editable: true
            from: 0
            to: 100
            value: SubtitleSettings.preferredTrack

            onValueChanged: {
                SubtitleSettings.preferredTrack = value;
                SubtitleSettings.save();
                if (value === 0) {
                    mpv.setProperty("sid", "auto");
                } else {
                    mpv.setProperty("sid", value);
                }
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }
    }
}
