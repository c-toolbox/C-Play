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

    GridLayout {
        id: content

        columns: 2

        SettingsHeader {
            Layout.columnSpan: 2
            Layout.fillWidth: true
            text: qsTr("Playback settings")
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Media visibility at startup:")
        }
        Item {
            Layout.fillWidth: true
            height: visibilityAtStartup.height

            SpinBox {
                id: visibilityAtStartup

                editable: true
                from: 0
                to: 100
                value: PlaybackSettings.visibility

                onValueChanged: {
                    PlaybackSettings.visibility = visibilityAtStartup.value;
                    PlaybackSettings.save();
                }
            }
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Set fade duration:")
        }
        RowLayout {
            SpinBox {
                id: fadeDuration

                from: 0
                to: 20000
                value: PlaybackSettings.fadeDuration

                onValueChanged: {
                    PlaybackSettings.fadeDuration = value;
                    PlaybackSettings.save();
                }
            }
            LabelWithTooltip {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: {
                    qsTr("ms = Fades out/in %1 seconds in total when loading new content").arg(Number((fadeDuration.value * 1.0) / 1000.0).toFixed(3));
                }
            }
        }
        Item {
            height: 1
            width: 1
        }
        CheckBox {
            id: checkSyncVolumeVisibilityFading

            checked: PlaybackSettings.syncVolumeVisibilityFading
            text: qsTr("Startup: Sync media volume+visibility fading")

            onCheckedChanged: {
                PlaybackSettings.syncVolumeVisibilityFading = checked;
                PlaybackSettings.save();
            }
        }
        Item {
            height: 1
            width: 1
        }
        CheckBox {
            id: rewindMediaOnEOF

            checked: PlaybackSettings.rewindOnEOFwhenPause
            text: qsTr("Startup: Stop/Rewind media when end-of-file+pause")

            onCheckedChanged: {
                PlaybackSettings.rewindOnEOFwhenPause = checked;
                PlaybackSettings.save();
            }
        }
        Item {
            height: 1
            width: 1
        }
        CheckBox {
            id: fadeDownBeforeRewind

            checked: PlaybackSettings.fadeDownBeforeRewind
            text: qsTr("On Stop/Rewind: Fade down media visibility before")

            onCheckedChanged: {
                PlaybackSettings.fadeDownBeforeRewind = checked;
                PlaybackSettings.save();
            }
        }

               // Seek Small Step
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Seek small step:")
        }
        Item {
            Layout.fillWidth: true
            height: seekSmallStep.height

            SpinBox {
                id: seekSmallStep

                editable: true
                from: 0
                to: 100
                value: PlaybackSettings.seekSmallStep

                onValueChanged: {
                    PlaybackSettings.seekSmallStep = seekSmallStep.value;
                    PlaybackSettings.save();
                }
            }
        }

        // Seek Medium Step
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Seek medium step:")
        }
        Item {
            Layout.fillWidth: true
            height: seekMediumStep.height

            SpinBox {
                id: seekMediumStep

                editable: true
                from: 0
                to: 100
                value: PlaybackSettings.seekMediumStep

                onValueChanged: {
                    PlaybackSettings.seekMediumStep = seekMediumStep.value;
                    PlaybackSettings.save();
                }
            }
        }

        // Seek Big Step
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Seek big step:")
        }
        Item {
            Layout.fillWidth: true
            height: seekBigStep.height

            SpinBox {
                id: seekBigStep

                editable: true
                from: 0
                to: 100
                value: PlaybackSettings.seekBigStep

                onValueChanged: {
                    PlaybackSettings.seekBigStep = seekBigStep.value;
                    PlaybackSettings.save();
                }
            }
        }

        SettingsHeader {
            Layout.columnSpan: 2
            Layout.fillWidth: true
            text: qsTr("Sync correction")
            level: 4
        }

        Item {
            height: 1
            width: 1
        }
        CheckBox {
            id: useThresholdToSyncTimePositionCheckbox

            checked: PlaybackSettings.useThresholdToSyncTimePosition
            text: qsTr("Use functionality to sync time based on position threshold")

            onCheckedChanged: {
                PlaybackSettings.useThresholdToSyncTimePosition = checked;
                PlaybackSettings.save();
            }
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Time position sync threshold:")
        }
        RowLayout {
            SpinBox {
                id: timeThresholdSaving

                enabled: useThresholdToSyncTimePositionCheckbox.checked
                from: 100
                to: 5000
                value: PlaybackSettings.thresholdToSyncTimePosition

                onValueChanged: {
                    PlaybackSettings.thresholdToSyncTimePosition = value;
                    PlaybackSettings.save();
                }
            }
            LabelWithTooltip {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: {
                    qsTr("ms = Set time position if it is %1 seconds off from master").arg(Number((timeThresholdSaving.value * 1.0) / 1000.0).toFixed(3));
                }
            }
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Time position skip iterations:")
        }
        RowLayout {
            SpinBox {
                id: timeSkipIterations

                enabled: useThresholdToSyncTimePositionCheckbox.checked
                from: 1
                to: 500
                value: PlaybackSettings.thresholdToSyncTimeSkipSets

                onValueChanged: {
                    PlaybackSettings.thresholdToSyncTimeSkipSets = value;
                    PlaybackSettings.save();
                }
            }
            LabelWithTooltip {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: {
                    timeSkipIterations.value > 1 ? qsTr("Performing time correction every %1th time.").arg(Number((timeSkipIterations.value))) : qsTr("Performing time correction every time.");
                }
            }
        }
        Item {
            height: Kirigami.Units.gridUnit
            width: Kirigami.Units.gridUnit
        }
        CheckBox {
            id: applyThresholdSyncOnLoopOnlyCheckbox

            Layout.fillWidth: true
            checked: PlaybackSettings.applyThresholdSyncOnLoopOnly
            enabled: useThresholdToSyncTimePositionCheckbox.checked
            text: qsTr("Apply sync threshold when looping(or beginning of video) only")

            onCheckedChanged: {
                PlaybackSettings.applyThresholdSyncOnLoopOnly = checked;
                PlaybackSettings.save();
            }
        }
        Item {
            height: Kirigami.Units.gridUnit
            width: Kirigami.Units.gridUnit
        }
        RowLayout {
            SpinBox {
                id: timeCheckThresholdOnLoopBox

                enabled: applyThresholdSyncOnLoopOnlyCheckbox.checked && applyThresholdSyncOnLoopOnlyCheckbox.enabled
                from: 0
                to: 20000
                value: PlaybackSettings.timeToCheckThresholdSyncAfterLoop

                onValueChanged: {
                    PlaybackSettings.timeToCheckThresholdSyncAfterLoop = value;
                    PlaybackSettings.save();
                }
            }
            LabelWithTooltip {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: {
                    qsTr("ms = Stops checking threshold sync %1 seconds after video start").arg(Number((timeCheckThresholdOnLoopBox.value * 1.0) / 1000.0).toFixed(3));
                }
            }
        }

        Item {
            height: 1
            width: 1
        }
        CheckBox {
            id: useFrameSyncCorrectionCheckbox

            checked: PlaybackSettings.useFrameSyncCorrection
            text: qsTr("Use frame sync correction (progressive speed adjustment + hard seek)")

            onCheckedChanged: {
                PlaybackSettings.useFrameSyncCorrection = checked;
                PlaybackSettings.save();
            }
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Frame sync seek threshold:")
        }
        RowLayout {
            SpinBox {
                id: frameSyncSeekThresholdBox

                enabled: useFrameSyncCorrectionCheckbox.checked
                from: 1
                to: 60
                value: PlaybackSettings.frameSyncSeekThreshold

                onValueChanged: {
                    PlaybackSettings.frameSyncSeekThreshold = value;
                    PlaybackSettings.save();
                }
            }
            LabelWithTooltip {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: {
                    qsTr("s = Hard seek if time difference is more than %1 seconds").arg(Number((frameSyncSeekThresholdBox.value * 1.0)).toFixed(1));
                }
            }
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Frame sync in-sync threshold:")
        }
        RowLayout {
            SpinBox {
                id: frameSyncSpeedAdjustThresholdBox

                enabled: useFrameSyncCorrectionCheckbox.checked
                from: 10
                to: 500
                value: PlaybackSettings.frameSyncSpeedAdjustThreshold * 1000

                onValueChanged: {
                    PlaybackSettings.frameSyncSpeedAdjustThreshold = frameSyncSpeedAdjustThresholdBox.value / 1000.0;
                    PlaybackSettings.save();
                }
            }
            LabelWithTooltip {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: {
                    qsTr("ms = Considered in sync if time difference is below %1 ms").arg(Number((frameSyncSpeedAdjustThresholdBox.value * 1.0)).toFixed(0));
                }
            }
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Frame sync max speed adjustment:")
        }
        RowLayout {
            SpinBox {
                id: frameSyncMaxSpeedAdjustBox

                enabled: useFrameSyncCorrectionCheckbox.checked
                from: 1
                to: 100
                value: PlaybackSettings.frameSyncMaxSpeedAdjust * 100

                onValueChanged: {
                    PlaybackSettings.frameSyncMaxSpeedAdjust = frameSyncMaxSpeedAdjustBox.value / 100.0;
                    PlaybackSettings.save();
                }
            }
            LabelWithTooltip {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: {
                    qsTr("% = Maximum speed change (e.g. 50 = 50%% faster/slower)");
                }
            }
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Frame sync initial offset:")
        }
        RowLayout {
            SpinBox {
                id: frameSyncInitialOffsetBox

                enabled: useFrameSyncCorrectionCheckbox.checked
                from: 0
                to: 1000
                value: PlaybackSettings.frameSyncInitialOffset * 1000

                onValueChanged: {
                    PlaybackSettings.frameSyncInitialOffset = frameSyncInitialOffsetBox.value / 1000.0;
                    PlaybackSettings.save();
                }
            }
            LabelWithTooltip {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: {
                    qsTr("ms = Initial offset applied to target position on slaves");
                }
            }
        }

        SettingsHeader {
            Layout.columnSpan: 2
            Layout.fillWidth: true
            text: qsTr("Loaded MPV configuration")
            level: 4
        }
        // Render API ComboBox - only visible when --mpvapi is NOT set
        Label {
            visible: mpv.renderApiOpenglNextSupported() && !mpv.commandLineApiOverride()
            Layout.alignment: Qt.AlignRight
            text: qsTr("Render API:")
        }
        RowLayout {
            visible: mpv.renderApiOpenglNextSupported() && !mpv.commandLineApiOverride()

            ComboBox {
                id: renderApiComboBox

                textRole: "label"

                model: ListModel {
                    id: renderApiModel

                    ListElement {
                        label: "OpenGL"
                        value: 0
                    }
                    ListElement {
                        label: "OpenGL Next"
                        value: 1
                    }
                }

                Component.onCompleted: {
                    for (let i = 0; i < renderApiModel.count; ++i) {
                        if (renderApiModel.get(i).value === PlaybackSettings.renderApiType) {
                            currentIndex = i;
                            break;
                        }
                    }
                }
                onActivated: {
                    PlaybackSettings.renderApiType = model.get(index).value;
                    PlaybackSettings.save();
                }
            }
            LabelWithTooltip {
                Layout.fillWidth: true
                font.italic: true
                text: qsTr("Requires restart to take effect.")
            }
        }
        // Command-line API override status label - shown when --mpvapi IS set
        Item {
            visible: mpv.commandLineApiOverride() && mpv.renderApiOpenglNextSupported()
            Layout.fillWidth: true
        }
        Label {
            visible: mpv.commandLineApiOverride() && mpv.renderApiOpenglNextSupported()
            Layout.fillWidth: true
            font.italic: true
            color: "darkorange"
            text: qsTr("Render API overridden by --mpvapi command-line option") + ": " + (mpv.commandLineApiType() === 0 ? "OpenGL" : "OpenGL Next")
        }
        Label {
            text: qsTr("Configuration from external files:")
        }
        ScrollView {
            id: confText
            clip: true

            Layout.fillWidth: true

            TextArea {
                id: conTextArea

                Layout.fillWidth: true
                readOnly: true

                Component.onCompleted: text = mpv.getReadableExternalConfiguration()
            }
        }
    }
}
