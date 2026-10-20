/*
 * SPDX-FileCopyrightText:
 * 2021-2026 Erik Sunden <eriksunden85@gmail.com>
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

    // Layers playing through PortAudio with the audio mapping: the live layers, and the mpv
    // layers (Video/Audio/Stream/YouTube) routed through JACK.
    readonly property bool portAudioLayers: NDI_SUPPORT || OMT_SUPPORT || CAPTURE_SUPPORT || JACK_AUDIO_MAPPING_SUPPORT
    readonly property bool mpvUsesJack: AudioSettings.useCustomAudioOutput && AudioSettings.useAudioDriver
                                        && AudioSettings.preferredAudioOutputDriver === "jack"

    ButtonGroup { id: useCustomAudioOutputGroup }

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

        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Enable audio on:")
        }
        RowLayout{
            Item {
                height: 1
                width: 10
            }
            CheckBox {
                id: enableAudioOnMasterCheckbox

                checked: AudioSettings.enableAudioOnMaster
                enabled: true
                text: qsTr("Master")

                onCheckedChanged: {
                    if(!checked){
                        mpv.setProperty("mute", true);
                    }
                    AudioSettings.enableAudioOnMaster = checked;
                    AudioSettings.save();
                    app.slides.runUpdateAudioOutputOnLayers();
                    if(checked){
                        mpv.setProperty("mute", false);
                    }
                }
            }
            Item {
                height: 1
                width: 10
            }
            CheckBox {
                id: enableAudioOnNodesCheckbox

                checked: AudioSettings.enableAudioOnNodes
                enabled: true
                text: qsTr("Nodes / Clients")

                onCheckedChanged: {
                    AudioSettings.enableAudioOnNodes = checked;
                    AudioSettings.save();
                    mpv.enableAudioOnNodes(checked);
                }
            }
            Item {
                height: 1
                width: 10
            }
            Label {
                Layout.alignment: Qt.AlignLeft
                font.italic: true
                text: qsTr("Requires restart to work with live layers")
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

        SettingsHeader {
            Layout.columnSpan: 3
            Layout.fillWidth: true
            text: qsTr("Audio settings for media files")
            level: 4
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
            id: loadAudioFilesInVideoFolder

            checked: AudioSettings.loadAudioFileInVideoFolder
            enabled: true
            text: qsTr("Load audio files in same folder as video file.")

            onCheckedChanged: {
                AudioSettings.loadAudioFileInVideoFolder = checked;
                AudioSettings.save();
                mpv.setLoadAudioInVidFolder(checked);
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
                    if (index < 0 || index >= mpv.audioDevices.length || !mpv.audioDevices[index])
                        return;
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

                // No selection until a driver is stored: showing "jack" (index 0) for an empty
                // setting would claim a driver mpv does not use.
                currentIndex: -1
                displayText: currentIndex < 0 ? qsTr("Select driver") : currentText

                Component.onCompleted: {
                    for (let i = 0; i < audioOutputDriver.count; ++i) {
                        if (audioOutputDriver.get(i).driver === AudioSettings.preferredAudioOutputDriver) {
                            currentIndex = i;
                            break;
                        }
                    }
                }
                onActivated: function(index) {
                    AudioSettings.preferredAudioOutputDriver = model.get(index).driver;
                    AudioSettings.save();
                    mpv.updateAudioOutput();
                }
            }
            Layout.columnSpan: 2
        }

        Item {
            visible: JACK_AUDIO_MAPPING_SUPPORT
            height: 1
            width: 1
        }
        Label {
            visible: JACK_AUDIO_MAPPING_SUPPORT
            color: Kirigami.Theme.disabledTextColor
            font.italic: true
            wrapMode: Text.WordWrap
            text: qsTr("With the \"jack\" driver, each Video, Audio, Stream and YouTube layer can use the audio mixing and spatial mapping of the live layers, see \"mpv layers via JACK\" below.")
            Layout.columnSpan: 2
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
            visible: root.portAudioLayers
            Layout.columnSpan: 3
            Layout.topMargin: 20
            Layout.fillWidth: true
            text: JACK_AUDIO_MAPPING_SUPPORT
                  ? qsTr("Audio output and mixing for Live layers (NDI/OMT/Capture/DirectShow/WebRTC) and mpv layers via JACK (Video/Audio/Stream/YouTube)")
                  : qsTr("Audio settings for Live video layers (NDI/OMT/Capture/DirectShow/WebRTC)")
            level: 4
        }
        Item {
            visible: root.portAudioLayers
            Layout.columnSpan: 3
            // spacer item
            Layout.fillWidth: true
        }

        Item {
            visible: root.portAudioLayers
            height: 1
            width: 1
        }
        CheckBox {
            visible: root.portAudioLayers
            id: ndiAudioOutputCheckBox

            Layout.alignment: Qt.AlignRight
            checked: AudioSettings.portAudioCustomOutput
            enabled: true
            text: qsTr("Use custom audio output")

            onCheckedChanged: {
                AudioSettings.portAudioCustomOutput = checked;
                if(!AudioSettings.portAudioCustomOutput){
                    AudioSettings.portAudioOutputDevice = "";
                    AudioSettings.portAudioOutputApi = "";
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
            visible: root.portAudioLayers
            height: 1
            width: 1
        }
        RowLayout {
            Label {
                visible: root.portAudioLayers
                Layout.alignment: Qt.AlignRight
                enabled: AudioSettings.portAudioCustomOutput
                color: enabled ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                text: qsTr("Use audio device")
            }
            ComboBox {
                id: portAudioDeviceComboBox
                visible: root.portAudioLayers

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
                    AudioSettings.portAudioOutputApi = app.portAudioModel.apiName;
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
            visible: root.portAudioLayers
            height: 1
            width: 1
        }
        Label {
            visible: root.portAudioLayers
            id: portAudioDeviceInfo
            enabled: AudioSettings.portAudioCustomOutput
            color: enabled ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
            text: app.portAudioModel.deviceInfo
            Layout.columnSpan: 2
        }

        Item {
            visible: root.portAudioLayers
            height: 1
            width: 1
        }
        CheckBox {
            visible: root.portAudioLayers
            id: ndiAudioMixOutputCheckBox

            Layout.alignment: Qt.AlignRight
            checked: AudioSettings.portAudioMixInputToOutput
            enabled: true
            text: qsTr("Perform audio mixing (up/down) to output device")

            onCheckedChanged: {
                AudioSettings.portAudioMixInputToOutput = checked;
                AudioSettings.save();
                app.audioMapping.reload();
                app.portAudioModel.updatePortAudioList();
                app.slides.runUpdateAudioOutputOnLayers();
            }

            Layout.columnSpan: 2
            Layout.fillWidth: true
        }

        Item {
            visible: root.portAudioLayers
            height: 1
            width: 1
        }
        RowLayout {
            Label {
                visible: root.portAudioLayers
                Layout.alignment: Qt.AlignRight
                enabled: AudioSettings.portAudioMixInputToOutput
                color: enabled ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                text: qsTr("Output channels:")
            }
            ComboBox {
                visible: root.portAudioLayers
                id: ndiAudioTargetChannelsComboBox

                enabled: AudioSettings.portAudioMixInputToOutput
                textRole: "mode"

                model: ListModel {
                    id: ndiAudioTargetChannelsModel

                    ListElement {
                        mode: "2.0 : Stereo"
                        value: 2
                    }
                    ListElement {
                        mode: "2.1 : Stereo + LFE"
                        value: 3
                    }
                    ListElement {
                        mode: "5.0 : Surround"
                        value: 5
                    }
                    ListElement {
                        mode: "5.1 : Surround + LFE"
                        value: 6
                    }
                    ListElement {
                        mode: "7.0 : Surround"
                        value: 7
                    }
                    ListElement {
                        mode: "7.1 : Surround + LFE"
                        value: 8
                    }
                }

                Component.onCompleted: {
                    for (let i = 0; i < ndiAudioTargetChannelsModel.count; ++i) {
                        if (ndiAudioTargetChannelsModel.get(i).value === AudioSettings.portAudioOutputChannels) {
                            currentIndex = i;
                            break;
                        }
                    }
                }
                onActivated: {
                    AudioSettings.portAudioOutputChannels = model.get(index).value;
                    AudioSettings.save();
                    app.audioMapping.reload();
                    app.portAudioModel.updatePortAudioList();
                    app.slides.runUpdateAudioOutputOnLayers();
                }
            }
            Layout.columnSpan: 2
        }

        Item {
            visible: root.portAudioLayers
            height: 1
            width: 1
        }
        RowLayout {
            visible: root.portAudioLayers
            Button {
                icon.name: "audio-speakers-symbolic"
                text: qsTr("Audio mapping...")

                onClicked: {
                    audioMappingDialog.visible = true;
                    audioMappingDialog.raise();
                }

                ToolTip.visible: hovered
                ToolTip.delay: 500
                ToolTip.text: qsTr("Optional spatial mapping: place the speakers around the dome and let the sound of each layer follow where the layer is shown.")
            }
            Label {
                color: Kirigami.Theme.disabledTextColor
                font.italic: true
                text: !AudioSettings.portAudioMixInputToOutput ? qsTr("Spatial mapping requires audio mixing to the output device")
                      : app.audioMapping.spatialEnabled ? qsTr("Spatial mapping is on")
                      : qsTr("Spatial mapping is off")
            }
            Layout.columnSpan: 2
        }

        Item {
            visible: root.portAudioLayers
            height: 1
            width: 1
        }
        Label {
            visible: root.portAudioLayers
            wrapMode: Text.WordWrap
            Layout.columnSpan: 2
            Layout.fillWidth: true
            font.italic: true
            color: root.mpvUsesJack ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.neutralTextColor
            text: root.mpvUsesJack
                    ? qsTr("mpv uses the \"jack\" audio driver: layers can use \"Audio mapping via JACK\".")
                    : qsTr("mpv does not use the \"jack\" audio driver yet: \"Audio mapping via JACK\" has no effect until it does.")
        }
    }
}
