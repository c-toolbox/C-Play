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
            checked: PlaybackSettings.useCustomAudioOutput
            onCheckedChanged: {
                PlaybackSettings.useCustomAudioOutput = checked
                PlaybackSettings.save()
            }
        }
        Item {
            // spacer item
            Layout.fillWidth: true
        }

        RadioButton {
            id: audioOutputDeviceRadioButton
            enabled: audioOutputCheckBox.checked
            text: qsTr("Use audio device")
            checked: false
        }
        ComboBox {
            id: audioOutputDeviceComboBox
            enabled: audioOutputDeviceRadioButton.checked
            textRole: "description"
            valueRole: "name"
            model: mpv.audioDevices

            onActivated: {
                PlaybackSettings.audioOutputDevice = model.get(index).name
                PlaybackSettings.save()
                mpv.setProperty("audio-device", model.get(index).name)
            }

            Component.onCompleted: {
                for (let i = 0; i < audioOutputDevice.count; ++i) {
                    if (audioOutputDevice.get(i).name === PlaybackSettings.audioOutputDevice) {
                        currentIndex = i
                        break
                    }
                }
            }
        }

        RadioButton {
            id: audioOutputDriverRadioButton
            enabled: audioOutputCheckBox.checked
            text: qsTr("Use audio driver")
            checked: false
        }
        ComboBox {
            id: audioOutputDriverComboBox
            enabled: audioOutputDriverRadioButton.checked
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
                PlaybackSettings.audioOutputDriver = model.get(index).driver
                PlaybackSettings.save()
                mpv.setProperty("ao", model.get(index).driver)
            }

            Component.onCompleted: {
                for (let i = 0; i < audioOutputDriver.count; ++i) {
                    if (audioOutputDriver.get(i).driver === PlaybackSettings.audioOutputDriver) {
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
            height: 100
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
