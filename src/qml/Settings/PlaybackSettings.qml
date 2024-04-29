/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.12
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.12

import org.kde.kirigami 2.11 as Kirigami
import org.ctoolbox.cplay 1.0
import Haruna.Components 1.0

SettingsBasePage {
    id: root

    GridLayout {
        id: content

        columns: 2

        SettingsHeader {
            text: qsTr("Fade settings")
            Layout.columnSpan: 2
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("Set fade duration:")
            Layout.alignment: Qt.AlignRight
        }

        RowLayout {
            SpinBox {
                id: fadeDuration
                from: 0
                to: 20000
                value: PlaybackSettings.fadeDuration

                onValueChanged: {
                    PlaybackSettings.fadeDuration = value
                    PlaybackSettings.save()
                }
            }

            LabelWithTooltip {
                text: {
                    qsTr("ms = Fades out/in %1 seconds in total when loading new content").arg(Number((fadeDuration.value*1.0)/1000.0).toFixed(3))
                }
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        Item { width: 1; height: 1 }
        CheckBox {
            id: checkSyncImageAudioFading
            text: qsTr("Startup: Sync audio+image fading")
            checked: PlaybackSettings.syncImageVideoFading
            onCheckedChanged: {
                PlaybackSettings.syncImageVideoFading = checked
                PlaybackSettings.save()
            }
        }

        Item { width: 1; height: 1 }
        CheckBox {
            id: fadeDownMediaOnEOF
            text: qsTr("Startup: Fade down media on end-of-file+pause")
            checked: PlaybackSettings.fadeMediaDownOnEOF
            onCheckedChanged: {
                PlaybackSettings.fadeMediaDownOnEOF = checked
                PlaybackSettings.save()
            }
        }

        SettingsHeader {
            text: qsTr("Time settings")
            Layout.columnSpan: 2
            Layout.fillWidth: true
        }

        Item { width: 1; height: 1 }
        CheckBox {
            id: useThresholdToSyncTimePositionCheckbox
            text: qsTr("Use functionality to sync time based on position threshold")
            checked: PlaybackSettings.useThresholdToSyncTimePosition
            onCheckedChanged: {
                PlaybackSettings.useThresholdToSyncTimePosition = checked
                PlaybackSettings.save()
            }
        }

        Label {
            text: qsTr("Time position sync threshold:")
            Layout.alignment: Qt.AlignRight
        }

        RowLayout {
            SpinBox {
                id: timeThresholdSaving
                enabled: useThresholdToSyncTimePositionCheckbox.checked
                from: 100
                to: 5000
                value: PlaybackSettings.thresholdToSyncTimePosition

                onValueChanged: {
                    PlaybackSettings.thresholdToSyncTimePosition = value
                    PlaybackSettings.save()
                }
            }

            LabelWithTooltip {
                text: {
                    qsTr("ms = Set time position if it is %1 seconds off from master").arg(Number((timeThresholdSaving.value*1.0)/1000.0).toFixed(3))
                }
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        Item {
            width: Kirigami.Units.gridUnit
            height: Kirigami.Units.gridUnit
        }
        CheckBox {
            id: applyThresholdSyncOnLoopOnlyCheckbox
            text: qsTr("Apply sync threshold when looping only")
            checked: PlaybackSettings.applyThresholdSyncOnLoopOnly
            enabled: useThresholdToSyncTimePositionCheckbox.checked
            onCheckedChanged: {
                PlaybackSettings.applyThresholdSyncOnLoopOnly = checked
                PlaybackSettings.save()
            }
            Layout.fillWidth: true
        }

        Item {
            width: Kirigami.Units.gridUnit
            height: Kirigami.Units.gridUnit
        }
        RowLayout {
            SpinBox {
                id: timeCheckThresholdOnLoopBox
                enabled: applyThresholdSyncOnLoopOnlyCheckbox.checked && applyThresholdSyncOnLoopOnlyCheckbox.enabled
                from: 0
                to: 20000
                value: PlaybackSettings.timeToCheckThresholdSyncAfterLoop

                onValueChanged: {
                    PlaybackSettings.timeToCheckThresholdSyncAfterLoop = value
                    PlaybackSettings.save()
                }
            }

            LabelWithTooltip {
                text: {
                    qsTr("ms = Stops checking threshold sync %1 seconds after loop").arg(Number((timeCheckThresholdOnLoopBox.value*1.0)/1000.0).toFixed(3))
                }
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        Label {
            text: qsTr("Remember time position:")
            Layout.alignment: Qt.AlignRight
        }

        RowLayout {
            SpinBox {
                id: timePositionSaving
                from: -1
                to: 9999
                value: PlaybackSettings.minDurationToSavePosition

                onValueChanged: {
                    PlaybackSettings.minDurationToSavePosition = value
                    PlaybackSettings.save()
                }
            }

            LabelWithTooltip {
                text: {
                    if (timePositionSaving.value === -1) {
                        return qsTr("Disabled")
                    } else if (timePositionSaving.value === 0) {
                        return qsTr("For all files")
                    } else if (timePositionSaving.value === 1) {
                        return qsTr("For files longer than %1 minute").arg(timePositionSaving.value)
                    } else {
                        return qsTr("For files longer than %1 minutes").arg(timePositionSaving.value)
                    }
                }
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        // Seek Small Step
        Label {
            text: qsTr("Seek small step:")
            Layout.alignment: Qt.AlignRight
        }

        Item {
            height: seekSmallStep.height
            SpinBox {
                id: seekSmallStep
                editable: true
                from: 0
                to: 100
                value: PlaybackSettings.seekSmallStep
                onValueChanged: {
                    PlaybackSettings.seekSmallStep = seekSmallStep.value
                    PlaybackSettings.save()
                }
            }
            Layout.fillWidth: true
        }

        // Seek Medium Step
        Label {
            text: qsTr("Seek medium step:")
            Layout.alignment: Qt.AlignRight
        }

        Item {
            height: seekMediumStep.height
            SpinBox {
                id: seekMediumStep
                editable: true
                from: 0
                to: 100
                value: PlaybackSettings.seekMediumStep
                onValueChanged: {
                    PlaybackSettings.seekMediumStep = seekMediumStep.value
                    PlaybackSettings.save()
                }
            }
            Layout.fillWidth: true
        }

        // Seek Big Step
        Label {
            text: qsTr("Seek big step:")
            Layout.alignment: Qt.AlignRight
        }

        Item {
            height: seekBigStep.height
            SpinBox {
                id: seekBigStep
                editable: true
                from: 0
                to: 100
                value: PlaybackSettings.seekBigStep
                onValueChanged: {
                    PlaybackSettings.seekBigStep = seekBigStep.value
                    PlaybackSettings.save()
                }
            }
            Layout.fillWidth: true
        }

        SettingsHeader {
            text: qsTr("Loaded MPV configuration")
            Layout.columnSpan: 2
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("Configuration from external files:")
        }
        ScrollView {
            id: confText
            TextArea {
                id: conTextArea
                Component.onCompleted: text = mpv.getReadableExternalConfiguration()
                readOnly: true
                Layout.fillWidth: true
            }
            Layout.fillWidth: true
        }
    }
}
