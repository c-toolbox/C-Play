/*
 * SPDX-FileCopyrightText:
 * 2021-2025 Erik Sundén <eriksunden85@gmail.com>
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

    ButtonGroup { id: useCustomAudioOutputGroup }

    GridLayout {
        id: content

        columns: 3

        SettingsHeader {
            Layout.columnSpan: 3
            Layout.fillWidth: true
            text: qsTr("Audio settings (for media files)")
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
            id: audioOutputCheckBox

            Layout.alignment: Qt.AlignRight
            checked: AudioSettings.useCustomAudioOutput
            enabled: true
            text: qsTr("Use custom audio output for video/audio playback")

            onCheckedChanged: {
                AudioSettings.useCustomAudioOutput = checked;
                AudioSettings.save();
            }

            Layout.columnSpan: 2
            Layout.fillWidth: true
        }

        Item {
            height: 1
            width: 1
        }
        RowLayout {
            RadioButton {
                id: audioOutputDeviceRadioButton

                Layout.alignment: Qt.AlignRight
                checked: AudioSettings.useAudioDevice
                enabled: AudioSettings.useCustomAudioOutput
                text: qsTr("Use audio device")
                ButtonGroup.group: useCustomAudioOutputGroup

                onCheckedChanged: {
                    AudioSettings.useAudioDevice = checked;
                    AudioSettings.save();
                    mpv.updateAudioOutput();
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
                    mpv.updateAudioOutput();
                }
            }
            Layout.columnSpan: 2
        }

        Item {
            height: 1
            width: 1
        }
        RowLayout {
            RadioButton {
                id: audioOutputDriverRadioButton

                Layout.alignment: Qt.AlignRight
                checked: AudioSettings.useAudioDriver
                enabled: AudioSettings.useCustomAudioOutput
                text: qsTr("Use audio driver")
                ButtonGroup.group: useCustomAudioOutputGroup

                onCheckedChanged: {
                    AudioSettings.useAudioDriver = checked;
                    AudioSettings.save();
                    mpv.updateAudioOutput();
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
                    mpv.updateAudioOutput();
                }
            }
            Layout.columnSpan: 2
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

        SettingsHeader {
            visible: NDI_SUPPORT
            Layout.columnSpan: 3
            Layout.fillWidth: true
            text: qsTr("Audio settings (for NDI)")
        }
        Item {
            visible: NDI_SUPPORT
            Layout.columnSpan: 3
            // spacer item
            Layout.fillWidth: true
        }

        Item {
            visible: NDI_SUPPORT
            height: 1
            width: 1
        }
        CheckBox {
            visible: NDI_SUPPORT
            id: ndiAudioOutputCheckBox

            Layout.alignment: Qt.AlignRight
            checked: AudioSettings.portAudioCustomOutput
            enabled: true
            text: qsTr("Use custom audio output for NDI playback")

            onCheckedChanged: {
                AudioSettings.portAudioCustomOutput = checked;
                if(!AudioSettings.portAudioCustomOutput){
                    AudioSettings.portAudioOutputDevice = "";
                    AudioSettings.portAudioOutpuApi = "";
                }
                AudioSettings.save();
                app.portAudioModel.currentDevice = app.portAudioModel.indexOfCurrentDevice;
                portAudioDeviceInfo.text = app.portAudioModel.deviceInfo;
                app.portAudioModel.updatePortAudioList();
                app.slides.runUpdateAudioOutputOnLayers();
            }

            Layout.columnSpan: 2
            Layout.fillWidth: true
        }

        Item {
            visible: NDI_SUPPORT
            height: 1
            width: 1
        }
        RowLayout {
            Label {
                visible: NDI_SUPPORT
                Layout.alignment: Qt.AlignRight
                enabled: AudioSettings.portAudioCustomOutput
                color: enabled ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                text: qsTr("Use audio device")
            }
            ComboBox {
                id: portAudioDeviceComboBox
                visible: NDI_SUPPORT

                enabled: AudioSettings.portAudioCustomOutput
                model: app.portAudioModel
                currentIndex: app.portAudioModel.indexOfCurrentDevice
                textRole: "name"

                Component.onCompleted: {
                    app.portAudioModel.updatePortAudioList();
                }
                onActivated: {
                    app.portAudioModel.currentDevice = portAudioDeviceComboBox.currentIndex;
                    AudioSettings.portAudioOutputDevice = app.portAudioModel.deviceName;
                    AudioSettings.portAudioOutpuApi = app.portAudioModel.apiName;
                    portAudioDeviceInfo.text = app.portAudioModel.deviceInfo;
                    AudioSettings.save();
                    app.portAudioModel.updatePortAudioList();
                    app.slides.runUpdateAudioOutputOnLayers();
                }
                onVisibleChanged: {
                    if (visible) {
                        app.portAudioModel.updatePortAudioList();
                    }
                }
            }
            ToolButton {
                id: updatePortAudioListBox

                focusPolicy: Qt.NoFocus
                icon.height: 16
                icon.name: "view-refresh"
                text: ""

                onClicked: {
                    app.portAudioModel.updatePortAudioList();
                }
            }
            Layout.columnSpan: 2
        }

        Item {
            visible: NDI_SUPPORT
            height: 1
            width: 1
        }
        Label {
            visible: NDI_SUPPORT
            id: portAudioDeviceInfo
            enabled: AudioSettings.portAudioCustomOutput
            color: enabled ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
            text: app.portAudioModel.deviceInfo
            Layout.columnSpan: 2
        }
    }
}
