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
            text: qsTr("Audio settings")
        }
        Item {
            Layout.columnSpan: 3
            // spacer item
            Layout.fillWidth: true
        }
        CheckBox {
            id: audioOutputCheckBox

            Layout.alignment: Qt.AlignRight
            checked: AudioSettings.useCustomAudioOutput
            enabled: true
            text: qsTr("Use custom audio output")

            onCheckedChanged: {
                AudioSettings.useCustomAudioOutput = checked;
                AudioSettings.save();
            }
        }
        Item {
            // spacer item
            Layout.columnSpan: 2
            Layout.fillWidth: true
        }
        RadioButton {
            id: audioOutputDeviceRadioButton

            Layout.alignment: Qt.AlignRight
            checked: AudioSettings.useAudioDevice
            enabled: AudioSettings.useCustomAudioOutput
            text: qsTr("Use audio device")

            onCheckedChanged: {
                AudioSettings.useAudioDevice = checked;
                AudioSettings.save();
            }
        }
        ComboBox {
            id: audioOutputDeviceComboBox

            enabled: (AudioSettings.useAudioDevice && AudioSettings.useCustomAudioOutput)
            model: mpv.audioDevices
            textRole: "description"
            valueRole: "name"

            Component.onCompleted: {
                for (let i = 0; i < mpv.audioDevices.length; ++i) {
                    if (mpv.audioDevices[i].name === AudioSettings.preferredAudioOutputDevice) {
                        currentIndex = i;
                        break;
                    }
                }
            }
            onActivated: {
                AudioSettings.preferredAudioOutputDevice = mpv.audioDevices[index].name;
                AudioSettings.save();
                mpv.setProperty("audio-device", mpv.audioDevices[index].name);
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }
        RadioButton {
            id: audioOutputDriverRadioButton

            Layout.alignment: Qt.AlignRight
            checked: AudioSettings.useAudioDriver
            enabled: AudioSettings.useCustomAudioOutput
            text: qsTr("Use audio driver")

            onCheckedChanged: {
                AudioSettings.useAudioDriver = checked;
                AudioSettings.save();
            }
        }
        ComboBox {
            id: audioOutputDriverComboBox

            enabled: (AudioSettings.useAudioDriver && AudioSettings.useCustomAudioOutput)
            textRole: "driver"

            model: ListModel {
                id: audioOutputDriver

                ListElement {
                    driver: "jack"
                }
                ListElement {
                    driver: "openal"
                }
                ListElement {
                    driver: "oss"
                }
                ListElement {
                    driver: "pcm"
                }
                ListElement {
                    driver: "pulse"
                }
                ListElement {
                    driver: "wasapi"
                }
            }

            Component.onCompleted: {
                for (let i = 0; i < audioOutputDriver.count; ++i) {
                    if (audioOutputDriver.get(i).driver === AudioSettings.preferredAudioOutputDriver) {
                        currentIndex = i;
                        break;
                    }
                }
            }
            onActivated: {
                AudioSettings.preferredAudioOutputDriver = model.get(index).driver;
                AudioSettings.save();
                mpv.setProperty("ao", model.get(index).driver);
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }
        Item {
            Layout.columnSpan: 3
            Layout.fillWidth: true
            // spacer item
            height: 10
        }
        Item {
            height: 1
            width: 1
        }
        CheckBox {
            id: loadAudioFilesInVideoFolder

            checked: AudioSettings.loadAudioFileInVideoFolder
            enabled: true
            text: qsTr("Load audio files in same folder as video file.")

            onCheckedChanged: {
                AudioSettings.loadAudioFileInVideoFolder = checked;
                AudioSettings.save();
                if (checked) {
                    mpv.setProperty("audio-file-auto", "all");
                } else {
                    mpv.setProperty("audio-file-auto", "no");
                }
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }
        Item {
            Layout.columnSpan: 3
            Layout.fillWidth: true
            // spacer item
            height: 10
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Volume at startup:")
        }
        SpinBox {
            editable: true
            from: 0
            to: 100
            value: AudioSettings.volume

            onValueChanged: {
                AudioSettings.volume = value.toFixed(0);
                AudioSettings.save();
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        // Volume Step
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Volume step:")
        }
        Item {
            height: volumeStep.height

            SpinBox {
                id: volumeStep

                editable: true
                from: 0
                to: 100
                value: AudioSettings.volumeStep

                onValueChanged: {
                    if (root.visible) {
                        AudioSettings.volumeStep = volumeStep.value;
                        AudioSettings.save();
                    }
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
            text: AudioSettings.preferredLanguage

            onTextEdited: {
                AudioSettings.preferredLanguage = text;
                AudioSettings.save();
                mpv.setProperty("alang", text);
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
            value: AudioSettings.preferredTrack

            onValueChanged: {
                AudioSettings.preferredTrack = value;
                AudioSettings.save();
                if (value === 0) {
                    mpv.setProperty("aid", "auto");
                } else {
                    mpv.setProperty("aid", value);
                }
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }
    }
}
