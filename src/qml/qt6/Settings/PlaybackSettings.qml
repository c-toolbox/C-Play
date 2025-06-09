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
        Item {
            height: Kirigami.Units.gridUnit
            width: Kirigami.Units.gridUnit
        }
        CheckBox {
            id: applyThresholdSyncOnLoopOnlyCheckbox

            Layout.fillWidth: true
            checked: PlaybackSettings.applyThresholdSyncOnLoopOnly
            enabled: useThresholdToSyncTimePositionCheckbox.checked
            text: qsTr("Apply sync threshold when looping only")

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
                    qsTr("ms = Stops checking threshold sync %1 seconds after loop").arg(Number((timeCheckThresholdOnLoopBox.value * 1.0) / 1000.0).toFixed(3));
                }
            }
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Remember time position:")
        }
        RowLayout {
            SpinBox {
                id: timePositionSaving

                from: -1
                to: 9999
                value: PlaybackSettings.minDurationToSavePosition

                onValueChanged: {
                    PlaybackSettings.minDurationToSavePosition = value;
                    PlaybackSettings.save();
                }
            }
            LabelWithTooltip {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: {
                    if (timePositionSaving.value === -1) {
                        return qsTr("Disabled");
                    } else if (timePositionSaving.value === 0) {
                        return qsTr("For all files");
                    } else if (timePositionSaving.value === 1) {
                        return qsTr("For files longer than %1 minute").arg(timePositionSaving.value);
                    } else {
                        return qsTr("For files longer than %1 minutes").arg(timePositionSaving.value);
                    }
                }
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
            text: qsTr("Loaded MPV configuration")
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
