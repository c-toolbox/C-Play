/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

import org.kde.kirigami 2.15 as Kirigami
import org.ctoolbox.cplay 1.0
import Haruna.Components 1.0

SettingsBasePage {
    id: root

    GridLayout {
        id: content

        columns: 3

        SettingsHeader {
            text: qsTr("Audio settings")
            Layout.columnSpan: 3
            Layout.fillWidth: true
        }

        Item {
            // spacer item
            Layout.fillWidth: true
            Layout.columnSpan: 3
        }

        CheckBox {
            id: audioOutputCheckBox
            text: qsTr("Use custom audio output")
            enabled: true
            checked: AudioSettings.useCustomAudioOutput
            onCheckedChanged: {
                AudioSettings.useCustomAudioOutput = checked
                AudioSettings.save()
            }
            Layout.alignment: Qt.AlignRight
        }
        Item {
            // spacer item
            Layout.columnSpan: 2
            Layout.fillWidth: true
        }

        RadioButton {
            id: audioOutputDeviceRadioButton
            enabled: AudioSettings.useCustomAudioOutput
            text: qsTr("Use audio device")
            checked: AudioSettings.useAudioDevice
            onCheckedChanged: {
                AudioSettings.useAudioDevice = checked
                AudioSettings.save()
            }
            Layout.alignment: Qt.AlignRight
        }
        ComboBox {
            id: audioOutputDeviceComboBox
            enabled: (AudioSettings.useAudioDevice && AudioSettings.useCustomAudioOutput)
            textRole: "description"
            valueRole: "name"
            model: mpv.audioDevices

            onActivated: {
                AudioSettings.preferredAudioOutputDevice = mpv.audioDevices[index].name
                AudioSettings.save()
                mpv.setProperty("audio-device", mpv.audioDevices[index].name)
            }

            Component.onCompleted: {
                for (let i = 0; i < mpv.audioDevices.length; ++i) {
                    if (mpv.audioDevices[i].name === AudioSettings.preferredAudioOutputDevice) {
                        currentIndex = i
                        break
                    }
                }
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        RadioButton {
            id: audioOutputDriverRadioButton
            enabled: AudioSettings.useCustomAudioOutput
            text: qsTr("Use audio driver")
            checked: AudioSettings.useAudioDriver
            onCheckedChanged: {
                AudioSettings.useAudioDriver = checked
                AudioSettings.save()
            }
            Layout.alignment: Qt.AlignRight
        }
        ComboBox {
            id: audioOutputDriverComboBox
            enabled: (AudioSettings.useAudioDriver && AudioSettings.useCustomAudioOutput)
            textRole: "driver"
            model: ListModel {
                id: audioOutputDriver
                ListElement { driver: "jack"; }
                ListElement { driver: "openal"; }
                ListElement { driver: "oss"; }
                ListElement { driver: "pcm"; }
                ListElement { driver: "pulse"; }
                ListElement { driver: "wasapi"; }
            }

            onActivated: {
                AudioSettings.preferredAudioOutputDriver = model.get(index).driver
                AudioSettings.save()
                mpv.setProperty("ao", model.get(index).driver)
            }

            Component.onCompleted: {
                for (let i = 0; i < audioOutputDriver.count; ++i) {
                    if (audioOutputDriver.get(i).driver === AudioSettings.preferredAudioOutputDriver) {
                        currentIndex = i
                        break
                    }
                }
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        Item {
            // spacer item
            height: 10
            Layout.columnSpan: 3
            Layout.fillWidth: true
        }

        Item { width: 1; height: 1 }
        CheckBox {
            id: loadAudioFilesInVideoFolder
            text: qsTr("Load audio files in same folder as video file.")
            enabled: true
            checked: AudioSettings.loadAudioFileInVideoFolder
            onCheckedChanged: {
                AudioSettings.loadAudioFileInVideoFolder = checked
                AudioSettings.save()
                if(checked){
                    mpv.setProperty("audio-file-auto", "all")
                }
                else{
                    mpv.setProperty("audio-file-auto", "no")
                }
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        Item {
            // spacer item
            height: 10
            Layout.columnSpan: 3
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("Volume at startup:")
            Layout.alignment: Qt.AlignRight
        }
        SpinBox {
            from: 0
            to: 100
            value: AudioSettings.volume
            editable: true
            onValueChanged: {
                AudioSettings.volume = value.toFixed(0)
                AudioSettings.save()
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }


        // Volume Step
        Label {
            text: qsTr("Volume step:")
            Layout.alignment: Qt.AlignRight
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
                        AudioSettings.volumeStep = volumeStep.value
                        AudioSettings.save()
                    }
                }
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }


        Label {
            text: qsTr("Preferred language:")
            Layout.alignment: Qt.AlignRight
        }
        TextField {
            text: AudioSettings.preferredLanguage
            placeholderText: "eng,ger etc."
            onTextEdited: {
                AudioSettings.preferredLanguage = text
                AudioSettings.save()
                mpv.setProperty("alang", text)
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
            text: qsTr("Preferred track:")
            Layout.alignment: Qt.AlignRight
        }
        SpinBox {
            from: 0
            to: 100
            value: AudioSettings.preferredTrack
            editable: true
            onValueChanged: {
                AudioSettings.preferredTrack = value
                AudioSettings.save()
                if (value === 0) {
                    mpv.setProperty("aid", "auto")
                } else {
                    mpv.setProperty("aid", value)
                }
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }
    }
}
