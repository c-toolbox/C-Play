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

Kirigami.ApplicationWindow {
    id: root

    color: Kirigami.Theme.alternateBackgroundColor
    height: 560
    title: qsTr("Logging")
    visible: false
    width: 720

    ListModel { id: statsModel }

    Component.onCompleted: {
        if (window.x > width) {
            x = window.x - width;
        } else {
            x = window.x;
        }
        y = window.y;
    }

    Connections {
        target: playerController.mpv
        function onPerformanceStatsChanged() {
            updateStatsTable();
        }
    }

    function updateStatsTable() {
        statsModel.clear();
        if (!playerController.mpv)
            return;
        var stats = playerController.mpv.performanceStats;
        if (!stats)
            return;
        // Show the drop counters first, then all perf-info entries in their original order.
        statsModel.append({ name: qsTr("VO dropped frames (since enabled)"), value: String(stats["vo-frame-drop-count"] ?? 0) });
        statsModel.append({ name: qsTr("Decoder dropped frames (since enabled)"), value: String(stats["decoder-frame-drop-count"] ?? 0) });
        for (var key in stats) {
            if (key === "vo-frame-drop-count" || key === "decoder-frame-drop-count")
                continue;
            statsModel.append({ name: key, value: String(stats[key]) });
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.smallSpacing

        Label {
            text: qsTr("General logging")
            font.pointSize: 14
            font.bold: true
        }
        CheckBox {
            id: generalLoggingCheck
            checked: LoggingSettings.generalLoggingEnabled
            text: qsTr("Enable general logging (application + MPV log messages)")
            onCheckedChanged: {
                LoggingSettings.generalLoggingEnabled = checked;
                LoggingSettings.save();
                if (playerController.mpv)
                    playerController.mpv.loggingEnabled = checked;
            }
        }
        Label {
            text: qsTr("When enabled, all MPV log messages (debug level) are reported to the console/log file. When disabled, only errors are reported.")
            font.italic: true
            opacity: 0.7
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("Performance metrics")
            font.pointSize: 14
            font.bold: true
            Layout.topMargin: Kirigami.Units.largeSpacing
        }
        CheckBox {
            id: perfMetricsCheck
            checked: LoggingSettings.performanceMetricsEnabled
            text: qsTr("Enable MPV performance statistics (perf-info + frame drop counters)")
            onCheckedChanged: {
                LoggingSettings.performanceMetricsEnabled = checked;
                LoggingSettings.save();
                if (playerController.mpv)
                    playerController.mpv.performanceMetricsEnabled = checked;
            }
        }

        Label {
            visible: perfMetricsCheck.checked
            text: qsTr("Statistics (updated every second while enabled)")
            font.pointSize: 14
            font.bold: true
            Layout.topMargin: Kirigami.Units.largeSpacing
        }
        Label {
            visible: perfMetricsCheck.checked
            text: qsTr("perf-info values are rates measured by MPV since the previous update. Drop counters show frames dropped since metrics were enabled.")
            font.italic: true
            opacity: 0.7
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        ListView {
            id: statsTable
            visible: perfMetricsCheck.checked
            clip: true
            Layout.fillWidth: true
            Layout.fillHeight: true

            model: statsModel

            header: RowLayout {
                width: statsTable.width
                spacing: Kirigami.Units.smallSpacing

                Label {
                    text: qsTr("Metric")
                    font.bold: true
                    Layout.fillWidth: true
                }
                Label {
                    text: qsTr("Value")
                    font.bold: true
                    Layout.preferredWidth: 140
                }
            }

            delegate: RowLayout {
                width: statsTable.width
                spacing: Kirigami.Units.smallSpacing

                Label {
                    text: model.name
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Label {
                    text: model.value
                    horizontalAlignment: Text.AlignLeft
                    Layout.preferredWidth: 140
                }
            }

            ScrollBar.vertical: ScrollBar {}
        }

        Item { Layout.fillHeight: true; visible: !perfMetricsCheck.checked }
    }
}
