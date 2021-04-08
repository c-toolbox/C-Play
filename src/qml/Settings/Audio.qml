/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.0
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.12

import org.kde.kirigami 2.11 as Kirigami
import com.georgefb.haruna 1.0
import Haruna.Components 1.0

SettingsBasePage {
    id: root

    hasHelp: false
    helpFile: ""

    GridLayout {
        id: content

        columns: 2

        CheckBox {
            id: audioOutputCheckBox
            text: qsTr("Use custom audio output")
            enabled: true
            checked: AudioSettings.useCustomAudioOutput
            onCheckedChanged: {
                AudioSettings.useCustomAudioOutput = checked
                AudioSettings.save()
            }
        }
        Item {
            // spacer item
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

        RadioButton {
            id: audioOutputDriverRadioButton
            enabled: AudioSettings.useCustomAudioOutput
            text: qsTr("Use audio driver")
            checked: AudioSettings.useAudioDriver
            onCheckedChanged: {
                AudioSettings.useAudioDriver = checked
                AudioSettings.save()
            }
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

        /*Button {
            text: qsTr("Update audio settings")
            onClicked: model.submit()
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }*/

        Item {
            // spacer item
            Layout.fillWidth: true
            height: 30
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }

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
            Layout.fillWidth: true
            height: 30
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("Preferred language")
        }
        TextField {
            text: AudioSettings.preferredLanguage
            placeholderText: "eng,ger etc."
            Layout.fillWidth: true
            onTextEdited: {
                AudioSettings.preferredLanguage = text
                AudioSettings.save()
                mpv.setProperty("alang", text)
            }

            ToolTip {
                text: qsTr("Do not use spaces.")
            }
        }


        Label {
            text: qsTr("Preferred track")
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
            width: Kirigami.Units.gridUnit
            height: Kirigami.Units.gridUnit
        }
    }
}
