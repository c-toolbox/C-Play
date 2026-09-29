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
    width: 900

    ListModel { id: statsModel }

    ListModel { id: nodeModel }

    onVisibleChanged: {
        if (visible) {
            // Center over the main application window. From this nested component "window" is
            // that outer window, not this one. The top edge is clamped so a dialog taller than
            // the main window never rises above its top corner.
            x = window.x + (window.width - width) / 2;
            y = Math.max(window.y, window.y + (window.height - height) / 2);
            if (nodeTelemetryCheck.checked)
                updateNodeTable();
        }
    }

    Connections {
        target: playerController
        function onNodeTelemetryChanged() {
            if (root.visible && nodeTelemetryCheck.checked)
                updateNodeTable();
        }
    }

    function fmt(v, digits) {
        return (v === undefined || v === null) ? "-" : Number(v).toFixed(digits);
    }

    function updateNodeTable() {
        nodeModel.clear();
        const rows = playerController.nodeTelemetry;
        if (!rows)
            return;
        for (const r of rows) {
            nodeModel.append({
                "rowName": r.name,
                "rowOnline": r.online,
                "rowFps": r.online ? fmt(r.fps, 1) : "-",
                "rowVoDrops": r.online ? String(r.voDrops) : "-",
                "rowDecDrops": r.online ? String(r.decoderDrops) : "-",
                "rowOffset": r.online ? fmt(r.syncOffsetMs, 1) + " ms" : "-",
                "rowGpu": (r.online && r.gpuLoad >= 0) ? fmt(r.gpuLoad, 0) + " %" : "n/a",
                "rowDecoder": r.online ? ((r.hwCurrent && r.hwCurrent !== "no" ? r.hwCurrent : "sw") + " / " + (r.videoCodec || "-")) : "-",
                "rowLayers": r.online ? String(r.layerCount) : "-",
                "rowLatency": (r.online && r.latencyMs >= 0) ? fmt(r.latencyMs, 2) + " ms" : "n/a"
            });
        }
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
            text: qsTr("When enabled, all MPV log messages (debug level) and application debug output are reported to the console and appended to data/log/cplay_general.log. When disabled, only errors are reported.")
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

        Label {
            text: qsTr("Node telemetry")
            font.pointSize: 14
            font.bold: true
            Layout.topMargin: Kirigami.Units.largeSpacing
        }
        CheckBox {
            id: nodeTelemetryCheck
            checked: LoggingSettings.nodeTelemetryEnabled
            text: qsTr("Enable per-node telemetry (FPS, dropped frames, sync offset, GPU load, decoder state, latency)")
            onCheckedChanged: {
                if (checked !== LoggingSettings.nodeTelemetryEnabled) {
                    LoggingSettings.nodeTelemetryEnabled = checked;
                    LoggingSettings.save();
                }
                playerController.setNodeTelemetryEnabled(checked);
                if (checked)
                    updateNodeTable();
            }
        }
        Label {
            visible: nodeTelemetryCheck.checked
            text: qsTr("Nodes report once per second over the cluster data-transfer channel. Latency is the sync round-trip time measured by the master. GPU load is only available on Windows nodes. A node is marked offline when no report arrives for 3 seconds.")
            font.italic: true
            opacity: 0.7
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        ListView {
            id: nodeTable
            visible: nodeTelemetryCheck.checked
            clip: true
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 120

            model: nodeModel

            header: RowLayout {
                width: nodeTable.width
                spacing: Kirigami.Units.smallSpacing

                Label { text: qsTr("Node"); font.bold: true; Layout.fillWidth: true }
                Label { text: qsTr("FPS"); font.bold: true; Layout.preferredWidth: 52 }
                Label { text: qsTr("VO drops"); font.bold: true; Layout.preferredWidth: 70 }
                Label { text: qsTr("Dec drops"); font.bold: true; Layout.preferredWidth: 70 }
                Label { text: qsTr("Sync offset"); font.bold: true; Layout.preferredWidth: 82 }
                Label { text: qsTr("GPU"); font.bold: true; Layout.preferredWidth: 46 }
                Label { text: qsTr("Decoder"); font.bold: true; Layout.preferredWidth: 120 }
                Label { text: qsTr("Layers"); font.bold: true; Layout.preferredWidth: 48 }
                Label { text: qsTr("Latency"); font.bold: true; Layout.preferredWidth: 76 }
                Label { text: qsTr("Status"); font.bold: true; Layout.preferredWidth: 60 }
            }

            delegate: RowLayout {
                width: nodeTable.width
                spacing: Kirigami.Units.smallSpacing

                Label { text: model.rowName; elide: Text.ElideRight; Layout.fillWidth: true }
                Label {
                    text: model.rowFps
                    Layout.preferredWidth: 52
                    color: model.rowOnline && Number(model.rowFps) < 24 ? "orange" : Kirigami.Theme.textColor
                }
                Label {
                    text: model.rowVoDrops
                    Layout.preferredWidth: 70
                    color: model.rowOnline && Number(model.rowVoDrops) > 0 ? "orange" : Kirigami.Theme.textColor
                }
                Label {
                    text: model.rowDecDrops
                    Layout.preferredWidth: 70
                    color: model.rowOnline && Number(model.rowDecDrops) > 0 ? "orange" : Kirigami.Theme.textColor
                }
                Label {
                    text: model.rowOffset
                    Layout.preferredWidth: 82
                    color: model.rowOnline && Number(model.rowOffset) > 100 ? "crimson" : Kirigami.Theme.textColor
                }
                Label { text: model.rowGpu; Layout.preferredWidth: 46 }
                Label { text: model.rowDecoder; elide: Text.ElideRight; Layout.preferredWidth: 120 }
                Label { text: model.rowLayers; Layout.preferredWidth: 48 }
                Label { text: model.rowLatency; Layout.preferredWidth: 76 }
                Label {
                    text: model.rowOnline ? qsTr("online") : qsTr("offline")
                    Layout.preferredWidth: 60
                    color: model.rowOnline ? "lime" : "crimson"
                }
            }

            ScrollBar.vertical: ScrollBar {}
        }

        Item { Layout.fillHeight: true; visible: !perfMetricsCheck.checked && !nodeTelemetryCheck.checked }
    }
}
