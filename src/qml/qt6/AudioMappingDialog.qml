/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

// Audio mapping of the layers that play through PortAudio (NDI, OMT, DirectShow, capture and
// WebRTC). Opened from the audio settings. The window is not modal so the speakers can be
// placed in the master 3D view while it is open.
Kirigami.ApplicationWindow {
    id: root

    color: Kirigami.Theme.alternateBackgroundColor
    height: 720
    minimumHeight: 400
    minimumWidth: 620
    title: qsTr("Audio mapping")
    visible: false
    width: 720

    readonly property var mapping: app.audioMapping
    readonly property bool surroundMode: mapping.mode === 1
    readonly property bool arrayMode: mapping.mode === 2
    readonly property bool circularArray: arrayMode && mapping.arrayLayout === 1
    // Speaker array shares (percent) for the layer selected in the Layers panel.
    property var shares: []

    function layoutName(channels) {
        switch (channels) {
        case 1: return qsTr("1.0 : Mono");
        case 2: return qsTr("2.0 : Stereo");
        case 3: return qsTr("2.1 : Stereo + LFE");
        case 4: return qsTr("4.0 : Quadraphonic");
        case 5: return qsTr("5.0 : Surround");
        case 6: return qsTr("5.1 : Surround + LFE");
        case 7: return qsTr("7.0 : Surround");
        case 8: return qsTr("7.1 : Surround + LFE");
        }
        return qsTr("%1 channels").arg(channels);
    }

    // The output channel count can change (mode, speaker channels): reopen the layer outputs.
    function refreshOutputs() {
        app.slides.runUpdateAudioOutputOnLayers();
    }

    onVisibilityChanged: {
        if (visible) {
            // Center over the main application window ("window" is that outer window here).
            x = window.x + (window.width - width) / 2;
            y = Math.max(window.y, window.y + (window.height - height) / 2);
            root.mapping.reload();
        }
    }

    // Closing the dialog hides the speakers again; the positions stay saved.
    onClosing: {
        root.mapping.stopSpeakerTest();
        root.mapping.placeOnClick = false;
        root.mapping.showSpeakersInView = false;
    }

    Timer {
        interval: 250
        repeat: true
        running: root.visible && root.arrayMode
        onTriggered: root.shares = viewLayersIn3DRenderItem.audioSharesForSelectedLayer()
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: Kirigami.Units.smallSpacing

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Layers playing audio through the PortAudio output (NDI, OMT, Capture, DirectShow and WebRTC) are mixed up or down "
                           + "to the output layout (%1). A spatial mapping also makes the sound follow where each layer is shown.")
                      .arg(root.layoutName(root.mapping.channelCount))
            }
            Kirigami.InlineMessage {
                Layout.fillWidth: true
                type: Kirigami.MessageType.Warning
                visible: !root.mapping.mixingEnabled
                text: qsTr("Turn on \"Perform audio mixing (up/down) to output device\" in the audio settings to use a spatial mapping.")
            }

            RowLayout {
                enabled: root.mapping.mixingEnabled
                Label {
                    text: qsTr("Spatial mapping:")
                }
                ComboBox {
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 18
                    model: [qsTr("Off (standard up/down mix)"),
                            qsTr("Surround panning of the output layout"),
                            qsTr("Mono to a speaker array, by distance")]
                    currentIndex: root.mapping.mode
                    onActivated: (index) => {
                        root.mapping.mode = index;
                        root.refreshOutputs();
                    }
                }
            }

            // --- Surround panning -------------------------------------------------------
            GridLayout {
                visible: root.surroundMode
                columns: 3
                enabled: root.mapping.mixingEnabled
                Layout.leftMargin: Kirigami.Units.gridUnit

                Label {
                    Layout.columnSpan: 3
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("Each channel of a layer is panned toward the speakers of the %1 layout closest to where the layer is shown.")
                          .arg(root.layoutName(root.mapping.channelCount))
                }
                Label {
                    text: qsTr("Focus:")
                }
                Slider {
                    id: focusSlider
                    Layout.fillWidth: true
                    from: 1
                    to: 20
                    stepSize: 0.5
                    value: root.mapping.focus
                    onMoved: root.mapping.focus = value

                    ToolTip.visible: hovered
                    ToolTip.delay: 500
                    ToolTip.text: qsTr("Low values spread the sound over many speakers, high values keep it at the speakers nearest the layer.")
                }
                Label {
                    text: focusSlider.value.toFixed(1)
                }
                Label {
                    text: qsTr("Stereo width:")
                }
                Slider {
                    id: widthSlider
                    Layout.fillWidth: true
                    from: 0
                    to: 100
                    stepSize: 5
                    value: root.mapping.width
                    onMoved: root.mapping.width = value

                    ToolTip.visible: hovered
                    ToolTip.delay: 500
                    ToolTip.text: qsTr("How far the left and right channels spread across a flat layer (0% plays all channels from its centre).")
                }
                Label {
                    text: qsTr("%1%").arg(widthSlider.value.toFixed(0))
                }
            }

            // --- Speaker array ----------------------------------------------------------
            GridLayout {
                visible: root.arrayMode
                columns: 3
                enabled: root.mapping.mixingEnabled
                Layout.leftMargin: Kirigami.Units.gridUnit

                Label {
                    Layout.columnSpan: 3
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("The layer is summed to mono and shared out over any number of speakers, each on its own output channel. "
                               + "The closer a speaker is to the layer, the larger its share. The output opens %n channel(s).", "",
                               root.mapping.arrayChannelCount)
                }
                Label {
                    text: qsTr("Speaker positions:")
                }
                ComboBox {
                    Layout.columnSpan: 2
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 18
                    model: [qsTr("On the dome (azimuth / elevation)"), qsTr("Along the circular screen (degrees / height)")]
                    currentIndex: root.mapping.arrayLayout
                    onActivated: (index) => root.mapping.arrayLayout = index
                }
                Kirigami.InlineMessage {
                    Layout.columnSpan: 3
                    Layout.fillWidth: true
                    type: Kirigami.MessageType.Information
                    visible: root.circularArray && !root.mapping.circularAvailable
                    text: qsTr("Enable the circular surface mapping in the Grid settings: only circular plane layers have a position on the circular screen.")
                }
                Label {
                    text: qsTr("Level drop:")
                }
                Slider {
                    id: rolloffSlider
                    Layout.fillWidth: true
                    from: 1
                    to: 24
                    stepSize: 0.5
                    value: root.mapping.rolloff
                    onMoved: root.mapping.rolloff = value

                    ToolTip.visible: hovered
                    ToolTip.delay: 500
                    ToolTip.text: qsTr("Level drop per doubling of the distance to a speaker. 6 dB is the inverse distance law; higher keeps the sound at the nearest speakers.")
                }
                Label {
                    text: qsTr("%1 dB").arg(rolloffSlider.value.toFixed(1))
                }
                Label {
                    text: qsTr("Spatial blur:")
                }
                Slider {
                    id: blurSlider
                    Layout.fillWidth: true
                    from: 0.5
                    to: 50
                    stepSize: 0.5
                    value: root.mapping.blur
                    onMoved: root.mapping.blur = value

                    ToolTip.visible: hovered
                    ToolTip.delay: 500
                    ToolTip.text: qsTr("Added to every distance, in percent of the dome/circle radius: avoids that a layer right on a speaker plays from that speaker only.")
                }
                Label {
                    text: qsTr("%1%").arg(blurSlider.value.toFixed(1))
                }
                Item {
                    width: 1
                    height: 1
                }
                CheckBox {
                    Layout.columnSpan: 2
                    checked: root.mapping.constantPower
                    text: qsTr("Constant loudness (shares of the power; off: shares of the amplitude sum to 100%)")
                    onToggled: root.mapping.constantPower = checked
                }
            }

            Kirigami.Separator {
                visible: root.surroundMode || root.arrayMode
                Layout.fillWidth: true
                Layout.topMargin: Kirigami.Units.smallSpacing
                Layout.bottomMargin: Kirigami.Units.smallSpacing
            }

            // --- Placement in the master view ---------------------------------------------
            Kirigami.Heading {
                visible: root.surroundMode || root.arrayMode
                level: 4
                text: qsTr("Speaker placement")
            }
            Label {
                visible: root.surroundMode || root.arrayMode
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: root.circularArray
                      ? qsTr("Degrees are angles on the circular screen (as the centre angle of circular layers), height is in cm like the vertical position of a layer. "
                             + "In the master 3D view the speakers are drawn on the unwrapped 360° screen at the distance of the circular layers.")
                      : qsTr("Azimuth is measured clockwise from the dome front (0°), elevation upward from the horizon (90° is the zenith).")
            }
            RowLayout {
                visible: root.surroundMode || root.arrayMode
                CheckBox {
                    checked: root.mapping.showSpeakersInView
                    text: qsTr("Show speakers in the master view")
                    onToggled: {
                        root.mapping.showSpeakersInView = checked;
                        // The speakers are drawn in the 3D view: switch to it from the flat video view.
                        if (checked && window.mainViewMode === 0)
                            window.mainViewMode = 1;
                    }
                }
                CheckBox {
                    enabled: root.mapping.showSpeakersInView && root.mapping.selectedSpeaker >= 0
                    checked: root.mapping.placeOnClick
                    text: qsTr("Click in the view places the selected speaker")
                    onToggled: root.mapping.placeOnClick = checked
                }
            }
            // Speaker test: pulsed pink noise on one output channel of this (master) computer.
            RowLayout {
                visible: root.surroundMode || root.arrayMode
                Button {
                    icon.name: root.mapping.testingAll ? "media-playback-stop" : "media-playlist-repeat"
                    text: root.mapping.testingAll ? qsTr("Stop test") : qsTr("Test all in turn")
                    onClicked: {
                        if (root.mapping.testingAll)
                            root.mapping.stopSpeakerTest();
                        else
                            root.mapping.startSpeakerTestAll();
                    }

                    ToolTip.visible: hovered
                    ToolTip.delay: 500
                    ToolTip.text: qsTr("Plays pink noise bursts on each speaker in turn (two bursts each) on the audio output of this computer. "
                                       + "The speaker playing is selected and highlighted in the master view.")
                }
                Button {
                    visible: root.mapping.testSpeaker >= 0 && !root.mapping.testingAll
                    icon.name: "media-playback-stop"
                    text: qsTr("Stop test")
                    onClicked: root.mapping.stopSpeakerTest()
                }
                Label {
                    text: qsTr("Test level:")
                }
                Slider {
                    id: testLevelSlider
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 8
                    from: -60
                    to: 0
                    stepSize: 1
                    value: root.mapping.testLevel
                    onMoved: root.mapping.testLevel = value
                }
                Label {
                    text: qsTr("%1 dB").arg(testLevelSlider.value.toFixed(0))
                }
                Label {
                    visible: root.mapping.testSpeaker >= 0
                    font.bold: true
                    text: {
                        const i = root.mapping.testSpeaker;
                        if (root.arrayMode) {
                            const spk = root.mapping.arraySpeakers[i];
                            return spk ? qsTr("Playing: speaker %1 (ch %2)").arg(i + 1).arg(spk.channel) : "";
                        }
                        const spk = root.mapping.speakers[i];
                        return spk ? qsTr("Playing: %1 (ch %2)").arg(spk.label).arg(i + 1) : "";
                    }
                }
                Item {
                    Layout.fillWidth: true
                }
            }
            Kirigami.InlineMessage {
                Layout.fillWidth: true
                type: Kirigami.MessageType.Error
                visible: (root.surroundMode || root.arrayMode) && root.mapping.testError !== ""
                text: qsTr("Speaker test: %1").arg(root.mapping.testError)
            }
            Label {
                visible: root.arrayMode
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Kirigami.Theme.disabledTextColor
                font.italic: true
                text: root.shares.length > 0
                      ? qsTr("Share: of the layer selected in the Layers panel (also shown in the master view).")
                      : qsTr("Select a 3D layer in the Layers panel to see the share of each speaker.")
            }

            // Surround layout speakers.
            Repeater {
                model: root.surroundMode ? root.mapping.channelCount : 0

                delegate: RowLayout {
                    id: speakerRow

                    required property int index
                    readonly property var speaker: root.mapping.speakers[index]
                    readonly property bool lfe: speaker ? speaker.lfe : false
                    readonly property bool selected: root.mapping.selectedSpeaker === index

                    Layout.fillWidth: true

                    RadioButton {
                        enabled: !speakerRow.lfe
                        checked: speakerRow.selected
                        onToggled: if (checked) root.mapping.selectedSpeaker = speakerRow.index

                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                        ToolTip.text: qsTr("Select this speaker (highlighted in the master view)")
                    }
                    Label {
                        Layout.preferredWidth: Kirigami.Units.gridUnit * 4
                        font.bold: speakerRow.selected
                        text: qsTr("%1 (ch %2)").arg(speakerRow.speaker ? speakerRow.speaker.label : "").arg(speakerRow.index + 1)
                    }
                    ToolButton {
                        readonly property bool testing: root.mapping.testSpeaker === speakerRow.index
                        icon.name: testing ? "media-playback-stop" : "audio-volume-high"
                        checked: testing
                        onClicked: {
                            if (testing && !root.mapping.testingAll)
                                root.mapping.stopSpeakerTest();
                            else
                                root.mapping.startSpeakerTest(speakerRow.index);
                        }

                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                        ToolTip.text: qsTr("Play a test signal on this speaker (output channel %1)").arg(speakerRow.index + 1)
                    }
                    Label {
                        visible: speakerRow.lfe
                        Layout.fillWidth: true
                        color: Kirigami.Theme.disabledTextColor
                        font.italic: true
                        text: qsTr("Low frequency effects: not placed, gets the low end of every layer")
                    }
                    // Editing a SpinBox breaks its value binding, so dragged/clicked speakers
                    // are written back explicitly.
                    Connections {
                        target: root.mapping
                        function onSpeakersChanged() {
                            if (!speakerRow.speaker)
                                return;
                            azimuthBox.value = Math.round(speakerRow.speaker.azimuth);
                            elevationBox.value = Math.round(speakerRow.speaker.elevation);
                        }
                    }
                    Label {
                        visible: !speakerRow.lfe
                        text: qsTr("Azimuth:")
                    }
                    SpinBox {
                        id: azimuthBox
                        visible: !speakerRow.lfe
                        editable: true
                        from: -180
                        to: 180
                        value: speakerRow.speaker ? Math.round(speakerRow.speaker.azimuth) : 0
                        onValueModified: root.mapping.setSpeakerPosition(speakerRow.index, value, speakerRow.speaker.elevation, true)
                    }
                    Label {
                        visible: !speakerRow.lfe
                        text: qsTr("Elevation:")
                    }
                    SpinBox {
                        id: elevationBox
                        visible: !speakerRow.lfe
                        editable: true
                        from: -90
                        to: 90
                        value: speakerRow.speaker ? Math.round(speakerRow.speaker.elevation) : 0
                        onValueModified: root.mapping.setSpeakerPosition(speakerRow.index, speakerRow.speaker.azimuth, value, true)
                    }
                    Item {
                        visible: !speakerRow.lfe
                        Layout.fillWidth: true
                    }
                }
            }

            // Speaker array speakers.
            Repeater {
                model: root.arrayMode ? root.mapping.arraySpeakers.length : 0

                delegate: RowLayout {
                    id: arrayRow

                    required property int index
                    readonly property var speaker: root.mapping.arraySpeakers[index]
                    readonly property bool selected: root.mapping.selectedSpeaker === index

                    Layout.fillWidth: true

                    Connections {
                        target: root.mapping
                        function onSpeakersChanged() {
                            if (!arrayRow.speaker)
                                return;
                            channelBox.value = arrayRow.speaker.channel;
                            firstBox.value = Math.round(root.circularArray ? arrayRow.speaker.degrees : arrayRow.speaker.azimuth);
                            secondBox.value = Math.round(root.circularArray ? arrayRow.speaker.height : arrayRow.speaker.elevation);
                        }
                    }

                    RadioButton {
                        checked: arrayRow.selected
                        onToggled: if (checked) root.mapping.selectedSpeaker = arrayRow.index

                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                        ToolTip.text: qsTr("Select this speaker (highlighted in the master view)")
                    }
                    ToolButton {
                        readonly property bool testing: root.mapping.testSpeaker === arrayRow.index
                        icon.name: testing ? "media-playback-stop" : "audio-volume-high"
                        checked: testing
                        onClicked: {
                            if (testing && !root.mapping.testingAll)
                                root.mapping.stopSpeakerTest();
                            else
                                root.mapping.startSpeakerTest(arrayRow.index);
                        }

                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                        ToolTip.text: qsTr("Play a test signal on this speaker (output channel %1)").arg(arrayRow.speaker ? arrayRow.speaker.channel : 1)
                    }
                    Label {
                        font.bold: arrayRow.selected
                        text: qsTr("Speaker %1 - output ch:").arg(arrayRow.index + 1)
                    }
                    SpinBox {
                        id: channelBox
                        editable: true
                        from: 1
                        to: 64
                        value: arrayRow.speaker ? arrayRow.speaker.channel : 1
                        onValueModified: {
                            root.mapping.setArraySpeakerChannel(arrayRow.index, value);
                            root.refreshOutputs();
                        }
                    }
                    Label {
                        text: root.circularArray ? qsTr("Degrees:") : qsTr("Azimuth:")
                    }
                    SpinBox {
                        id: firstBox
                        editable: true
                        from: root.circularArray ? 0 : -180
                        to: root.circularArray ? 360 : 180
                        value: arrayRow.speaker ? Math.round(root.circularArray ? arrayRow.speaker.degrees : arrayRow.speaker.azimuth) : 0
                        onValueModified: {
                            if (root.circularArray)
                                root.mapping.setArraySpeakerCircularPosition(arrayRow.index, value, arrayRow.speaker.height, true);
                            else
                                root.mapping.setArraySpeakerDomePosition(arrayRow.index, value, arrayRow.speaker.elevation, true);
                        }
                    }
                    Label {
                        text: root.circularArray ? qsTr("Height (cm):") : qsTr("Elevation:")
                    }
                    SpinBox {
                        id: secondBox
                        editable: true
                        from: root.circularArray ? -100000 : -90
                        to: root.circularArray ? 100000 : 90
                        value: arrayRow.speaker ? Math.round(root.circularArray ? arrayRow.speaker.height : arrayRow.speaker.elevation) : 0
                        onValueModified: {
                            if (root.circularArray)
                                root.mapping.setArraySpeakerCircularPosition(arrayRow.index, arrayRow.speaker.degrees, value, true);
                            else
                                root.mapping.setArraySpeakerDomePosition(arrayRow.index, arrayRow.speaker.azimuth, value, true);
                        }
                    }
                    Label {
                        Layout.preferredWidth: Kirigami.Units.gridUnit * 3
                        horizontalAlignment: Text.AlignRight
                        text: arrayRow.index < root.shares.length ? qsTr("%1%").arg(root.shares[arrayRow.index].toFixed(1)) : ""
                    }
                    ToolButton {
                        icon.name: "list-remove"
                        onClicked: {
                            root.mapping.removeArraySpeaker(arrayRow.index);
                            root.refreshOutputs();
                        }

                        ToolTip.visible: hovered
                        ToolTip.delay: 500
                        ToolTip.text: qsTr("Remove this speaker")
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                }
            }

            RowLayout {
                Layout.topMargin: Kirigami.Units.smallSpacing

                Button {
                    visible: root.surroundMode
                    icon.name: "edit-reset"
                    text: qsTr("Reset to default positions")
                    onClicked: root.mapping.resetSpeakerPositions()
                }
                Button {
                    visible: root.arrayMode
                    icon.name: "list-add"
                    text: qsTr("Add speaker")
                    onClicked: {
                        root.mapping.addArraySpeaker();
                        root.refreshOutputs();
                    }
                }
                Button {
                    visible: root.arrayMode
                    icon.name: "view-refresh"
                    text: qsTr("Create evenly spaced:")
                    onClicked: {
                        root.mapping.createEvenArray(evenCountBox.value);
                        root.refreshOutputs();
                    }

                    ToolTip.visible: hovered
                    ToolTip.delay: 500
                    ToolTip.text: qsTr("Replaces the speakers with this many, evenly spaced around the dome or the circular screen, on output channels 1 and up.")
                }
                SpinBox {
                    id: evenCountBox
                    visible: root.arrayMode
                    from: 1
                    to: 64
                    value: 8
                    editable: true
                }
                Item {
                    Layout.fillWidth: true
                }
                Button {
                    icon.name: "dialog-close"
                    text: qsTr("Close")
                    onClicked: root.close()
                }
            }
        }
    }
}
