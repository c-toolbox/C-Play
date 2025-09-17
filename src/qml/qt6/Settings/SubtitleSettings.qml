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

        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Subtitle font family:")
        }
        RowLayout {
            ComboBox {
                id: subtitleFontFamily
                model: app.fonts

                delegate: ItemDelegate {
                    Kirigami.Theme.colorSet: Kirigami.Theme.View
                    width: subtitleFontFamily.width
                    implicitHeight: 28

                    contentItem: RowLayout {
                        Text {
                            font.family: modelData
                            color: highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                            text: modelData;
                        }
                    }
                }

                Component.onCompleted: currentIndex = find(SubtitleSettings.subtitleFontFamily)
                onActivated: {
                    SubtitleSettings.subtitleFontFamily = subtitleFontFamily.textAt(currentIndex);
                    SubtitleSettings.save();
                    mpv.setSubtitleFont(SubtitleSettings.subtitleFontFamily);
                }
            }
            ToolButton {
                id: updateFontsButton

                focusPolicy: Qt.NoFocus
                icon.height: 16
                icon.name: "view-refresh"
                text: ""

                onClicked: {
                    updateFontsButton.enabled = false;
                    app.updateFonts();
                    updateFontsButton.enabled = true;
                }

                ToolTip {
                    text: qsTr("Update font list")
                }
            }
            Text {
                text: qsTr("Example: ")
                color: Kirigami.Theme.textColor
            }
            Text {
                text: qsTr("This is how it will look like...")
                font.family: subtitleFontFamily.textAt(subtitleFontFamily.currentIndex)
                color: Kirigami.Theme.textColor
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        Item {
            height: 1
            width: 1
        }
        CheckBox {
            id: loadSystemFontsCheckbox

            checked: SubtitleSettings.loadSystemFonts
            enabled: true
            text: qsTr("Load system fonts (instead of not only in C-Play's data/fonts folder).")

            onCheckedChanged: {
                SubtitleSettings.loadSystemFonts = checked;
                SubtitleSettings.save();
                updateFontsButton.enabled = false;
                app.updateFonts();
                updateFontsButton.enabled = true;
            }
        }        
        Item {
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
            text: qsTr("Preferred subtitle language:")
        }
        RowLayout {
            TextField {
                placeholderText: "eng,ger etc."
                text: SubtitleSettings.preferredLanguage

                onTextEdited: {
                    SubtitleSettings.preferredLanguage = text;
                    SubtitleSettings.save();
                    mpv.setProperty("slang", text);
                }
            }
            Text {
                text: qsTr("IETF language tags (Two or three characters). Do not use spaces.")
                color: Kirigami.Theme.textColor
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
