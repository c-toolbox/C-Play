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
import "Components/WindowHelpers.js" as WindowHelpers

Kirigami.ApplicationWindow {
    id: root

    color: Kirigami.Theme.alternateBackgroundColor
    height: 560
    title: qsTr("Logging")
    visible: false
    width: 900

    ListModel { id: statsModel }

    ListModel { id: nodeModel }

    ListModel { id: loadFailuresModel }

    onVisibleChanged: {
        if (visible) {
            // Center over the main application window. From this nested component "window" is
            // that outer window, not this one. The top edge is clamped so a dialog taller than
            // the main window never rises above its top corner.
            x = window.x + (window.width - width) / 2;
            y = Math.max(window.y, window.y + (window.height - height) / 2);
            if (nodeTelemetryCheck.checked)
                updateNodeTable();
            updateNodeLoadFailureTable();
        }
    }

    Connections {
        target: playerController
        function onNodeTelemetryChanged() {
            if (root.visible && nodeTelemetryCheck.checked)
                updateNodeTable();
        }
    }

    // Node load failures are always-on (independent of the telemetry toggle), so this table is
    // refreshed whenever the failure set changes and shown only while there are entries.
    Connections {
        target: playerController
        function onNodeLoadFailuresChanged() {
            if (root.visible)
                updateNodeLoadFailureTable();
        }
    }

    function updateNodeLoadFailureTable() {
        loadFailuresModel.clear();
        const rows = playerController.nodeLoadFailures;
        if (!rows)
            return;
        for (const r of rows) {
            let layerText = r.layerTitle || "-";
            if (r.layerType && r.layerType !== "")
                layerText += " (" + r.layerType + ")";
            loadFailuresModel.append({
                "rowName": r.name,
                "rowPath": r.path,
                "rowLayer": layerText,
                "rowError": r.error
            });
        }
    }

    function fmt(v, digits) {
        return (v === undefined || v === null) ? "-" : Number(v).toFixed(digits);
    }

    // --- node telemetry: cell summaries and hover details ------------------------------

    function layerName(d) {
        const name = d.layer || qsTr("video layer");
        return d.file ? name + " (" + d.file + ")" : name;
    }

    // Short decoder label: "Vulkan HW", "Vulkan SW", "d3d11va", "SW".
    function decoderLabel(d) {
        if (d.engine === "vulkan")
            return d.hw ? qsTr("Vulkan HW") : qsTr("Vulkan SW");
        return d.hw ? d.hwdec : qsTr("SW");
    }

    function decoderSummary(d) {
        const codec = (d.codec || "?").toUpperCase();
        return codec + " · " + decoderLabel(d);
    }

    function decoderCell(r) {
        const layers = r.layers || [];
        if (layers.length === 0) // node without per-layer details (older build) or nothing loaded
            return r.layerCount > 0 ? ((r.hwCurrent && r.hwCurrent !== "no" ? r.hwCurrent : "sw") + " / " + (r.videoCodec || "-")) : "-";
        const first = decoderSummary(layers[0]);
        const others = layers.slice(1).filter(d => decoderSummary(d) !== first).length;
        return others > 0 ? first + " +" + others : first;
    }

    function decoderTip(r) {
        const layers = r.layers || [];
        if (layers.length === 0)
            return qsTr("Decoder and codec of the first video layer.\nNo per-layer details from this node.");
        const blocks = [];
        for (const d of layers) {
            const lines = [layerName(d)];
            const size = d.width > 0 && d.height > 0 ? d.width + "×" + d.height : "";
            const rate = d.nominalFps > 0 ? fmt(d.nominalFps, 3) + " fps" : "";
            if (d.engine === "vulkan") {
                lines.push(qsTr("  Engine: Vulkan engine (FFmpeg + libplacebo)"));
                lines.push(d.hw ? qsTr("  Decoding: hardware (Vulkan Video)")
                                : qsTr("  Decoding: software (CPU), no Vulkan decoder for this stream"));
                lines.push(d.upload === "interop" ? qsTr("  To OpenGL: zero-copy interop (video memory)")
                                                  : qsTr("  To OpenGL: host memory upload (slower fallback)"));
                lines.push(qsTr("  Codec: %1").arg([d.codec || "?", size, rate].filter(s => s).join(" · ")));
                if (d.outputFormat)
                    lines.push(qsTr("  Output: %1").arg(d.outputFormat));
            } else {
                lines.push(qsTr("  Engine: mpv"));
                lines.push(d.hw ? qsTr("  Decoding: hardware (%1)").arg(d.hwdec)
                                : qsTr("  Decoding: software (CPU)"));
                const codec = d.codecLong && d.codecLong !== d.codec ? (d.codec || "?") + " (" + d.codecLong + ")" : (d.codec || "?");
                lines.push(qsTr("  Codec: %1").arg([codec, size, rate].filter(s => s).join(" · ")));
                if (d.pixfmt)
                    lines.push(qsTr("  Pixel format: %1").arg(d.hwPixfmt ? d.pixfmt + " (hw: " + d.hwPixfmt + ")" : d.pixfmt));
            }
            blocks.push(lines.join("\n"));
        }
        return blocks.join("\n\n");
    }

    function fpsTip(r) {
        const lines = [qsTr("Lowest frame rate shown across the video layers (worst case).")];
        for (const d of (r.layers || [])) {
            const shown = d.fps >= 0 ? fmt(d.fps, 1) : qsTr("measuring");
            const nominal = d.nominalFps > 0 ? qsTr(" of %1").arg(fmt(d.nominalFps, 3)) : "";
            lines.push("  " + layerName(d) + ": " + shown + nominal + " fps");
        }
        return lines.join("\n");
    }

    function voDropsTip(r) {
        const lines = [qsTr("Frames decoded but dropped because they were late for display, since the previous report.")];
        for (const d of (r.layers || []))
            lines.push("  " + layerName(d) + ": " + (d.voDrops ?? 0));
        return lines.join("\n");
    }

    function decDropsTip(r) {
        const lines = [qsTr("Frames the decoder skipped to keep up, since the previous report.")];
        for (const d of (r.layers || [])) {
            if (d.engine === "vulkan")
                lines.push("  " + layerName(d) + ": " + qsTr("n/a (the Vulkan engine counts late frames under VO drops)"));
            else
                lines.push("  " + layerName(d) + ": " + (d.decDrops ?? 0));
        }
        return lines.join("\n");
    }

    function layersTip(r) {
        const layers = r.layers || [];
        if (layers.length === 0)
            return qsTr("Video layers with media loaded.");
        return layers.map(d => layerName(d) + " — " + (d.engine === "vulkan" ? qsTr("Vulkan engine") : qsTr("mpv"))).join("\n");
    }

    // Render (3D engine) load; older nodes only report the summed "gpuLoad".
    function gpuRenderCell(r) {
        if (!r.online)
            return "-";
        if (r.gpuRender >= 0)
            return fmt(r.gpuRender, 0) + " %";
        return r.gpuLoad >= 0 ? fmt(r.gpuLoad, 0) + " %" : "n/a";
    }

    function gpuDecodeCell(r) {
        if (!r.online)
            return "-";
        return (r.gpuDecode >= 0) ? fmt(r.gpuDecode, 0) + " %" : "n/a";
    }

    function gpuTip(r) {
        const lines = [qsTr("GPU load per engine (Windows performance counters, n/a elsewhere).")];
        if (!r.online)
            return lines[0];
        const engines = r.gpuEngines || {};
        const types = Object.keys(engines);
        if (types.length === 0) {
            if (r.gpuLoad >= 0)
                lines.push(qsTr("This node reports only a total (older C-Play version)."));
            return lines.join("\n");
        }
        types.sort((a, b) => engines[b] - engines[a]);
        lines.push("");
        for (const t of types)
            lines.push(t + ": " + fmt(engines[t], 0) + " %");
        lines.push("");
        lines.push(qsTr("Busiest engine: %1 %").arg(fmt(r.gpuLoad, 0)));
        lines.push(qsTr("Engines run in parallel; each value is the load of the busiest engine of that type."));
        return lines.join("\n");
    }

    function statusTip(r) {
        if (r.isMaster)
            return qsTr("This computer (the master's own playback).");
        if (!r.lastSeenMs)
            return qsTr("No report received.");
        const ago = Math.max(0, (Date.now() - r.lastSeenMs) / 1000);
        return qsTr("Last report %1 s ago.").arg(fmt(ago, 1));
    }

    function updateNodeTable() {
        const rows = playerController.nodeTelemetry || [];
        // Rows are updated in place (set), not cleared: recreated delegates would close the
        // tooltip being read on every report.
        while (nodeModel.count > rows.length)
            nodeModel.remove(nodeModel.count - 1);
        for (let i = 0; i < rows.length; ++i) {
            const r = rows[i];
            const entry = {
                "rowName": r.name,
                "rowOnline": r.online,
                "rowFps": r.online ? fmt(r.fps, 1) : "-",
                "rowVoDrops": r.online ? String(r.voDrops) : "-",
                "rowDecDrops": r.online ? String(r.decoderDrops) : "-",
                "rowOffset": r.online ? fmt(r.syncOffsetMs, 1) + " ms" : "-",
                "rowGpu": gpuRenderCell(r),
                "rowGpuDec": gpuDecodeCell(r),
                "rowDecoder": r.online ? decoderCell(r) : "-",
                "rowLayers": r.online ? String(r.layerCount) : "-",
                "rowLatency": (r.online && r.latencyMs >= 0) ? fmt(r.latencyMs, 2) + " ms" : "n/a",
                "tipName": r.address ? qsTr("Address: %1").arg(r.address) : "",
                "tipFps": r.online ? fpsTip(r) : "",
                "tipVoDrops": r.online ? voDropsTip(r) : "",
                "tipDecDrops": r.online ? decDropsTip(r) : "",
                "tipDecoder": r.online ? decoderTip(r) : "",
                "tipLayers": r.online ? layersTip(r) : "",
                "tipGpu": gpuTip(r),
                "tipStatus": statusTip(r)
            };
            if (i < nodeModel.count)
                nodeModel.set(i, entry);
            else
                nodeModel.append(entry);
        }
    }

    // A table cell that shows its full details on hover.
    component TipLabel: Label {
        property string tip: ""
        HoverHandler { id: tipHover }
        ToolTip.visible: tip.length > 0 && tipHover.hovered
        ToolTip.text: tip
        ToolTip.delay: 300
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
            text: qsTr("When enabled, all MPV log messages (debug level) and application debug output are reported to the console and appended to each computer's local data/log/cplay_general.log, on the master and all nodes. When disabled, only errors are reported unless command-line logging is enabled.")
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

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.largeSpacing

            Label {
                text: qsTr("Node telemetry")
                font.pointSize: 14
                font.bold: true
            }
            Item {
                Layout.fillWidth: true
            }
            Button {
                icon.name: "network-workgroup"
                text: qsTr("Node Configuration...")
                onClicked: WindowHelpers.showWindow(nodeConfigurationWindow)
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Edit data/nodes.json: which IP address is which node id.")
            }
        }
        CheckBox {
            id: nodeTelemetryCheck
            checked: LoggingSettings.nodeTelemetryEnabled
            text: qsTr("Enable per-node telemetry (FPS, dropped frames, sync offset, GPU render/decode load, decoder state, latency)")
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
                Label { text: qsTr("Decode"); font.bold: true; Layout.preferredWidth: 52 }
                Label { text: qsTr("Decoder"); font.bold: true; Layout.preferredWidth: 170 }
                Label { text: qsTr("Layers"); font.bold: true; Layout.preferredWidth: 48 }
                Label { text: qsTr("Latency"); font.bold: true; Layout.preferredWidth: 76 }
                Label { text: qsTr("Status"); font.bold: true; Layout.preferredWidth: 60 }
            }

            delegate: RowLayout {
                width: nodeTable.width
                spacing: Kirigami.Units.smallSpacing

                TipLabel { text: model.rowName; tip: model.tipName; elide: Text.ElideRight; Layout.fillWidth: true }
                TipLabel {
                    text: model.rowFps
                    tip: model.tipFps
                    Layout.preferredWidth: 52
                    color: model.rowOnline && Number(model.rowFps) < 24 ? "orange" : Kirigami.Theme.textColor
                }
                TipLabel {
                    text: model.rowVoDrops
                    tip: model.tipVoDrops
                    Layout.preferredWidth: 70
                    color: model.rowOnline && Number(model.rowVoDrops) > 0 ? "orange" : Kirigami.Theme.textColor
                }
                TipLabel {
                    text: model.rowDecDrops
                    tip: model.tipDecDrops
                    Layout.preferredWidth: 70
                    color: model.rowOnline && Number(model.rowDecDrops) > 0 ? "orange" : Kirigami.Theme.textColor
                }
                TipLabel {
                    text: model.rowOffset
                    tip: model.rowOnline ? qsTr("Distance between this node's playback position and the master's.\nRed above 100 ms. Not measured for live streams.") : ""
                    Layout.preferredWidth: 82
                    color: model.rowOnline && parseFloat(model.rowOffset) > 100 ? "crimson" : Kirigami.Theme.textColor
                }
                TipLabel {
                    text: model.rowGpu
                    tip: qsTr("Render load (busiest 3D engine).") + "\n" + model.tipGpu
                    Layout.preferredWidth: 46
                }
                TipLabel {
                    text: model.rowGpuDec
                    tip: qsTr("Hardware video decode load (busiest VideoDecode engine, e.g. NVDEC).") + "\n" + model.tipGpu
                    Layout.preferredWidth: 52
                }
                TipLabel { text: model.rowDecoder; tip: model.tipDecoder; elide: Text.ElideRight; Layout.preferredWidth: 170 }
                TipLabel { text: model.rowLayers; tip: model.tipLayers; Layout.preferredWidth: 48 }
                TipLabel {
                    text: model.rowLatency
                    tip: qsTr("Sync round-trip time between the master and this node, measured by the master.")
                    Layout.preferredWidth: 76
                }
                TipLabel {
                    text: model.rowOnline ? qsTr("online") : qsTr("offline")
                    tip: model.tipStatus
                    Layout.preferredWidth: 60
                    color: model.rowOnline ? "lime" : "crimson"
                }
            }

            ScrollBar.vertical: ScrollBar {}
        }

        Label {
            text: qsTr("Node load failures")
            font.pointSize: 14
            font.bold: true
            Layout.topMargin: Kirigami.Units.largeSpacing
            visible: playerController.nodeLoadFailures.length > 0
        }
        Label {
            visible: playerController.nodeLoadFailures.length > 0
            text: qsTr("Files that a cluster node could not open or decode. Reported automatically (independent of the telemetry toggle); entries disappear when the node loads the file successfully, goes offline, or stays silent for 30 seconds.")
            font.italic: true
            opacity: 0.7
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        ListView {
            id: loadFailuresTable
            visible: playerController.nodeLoadFailures.length > 0
            clip: true
            Layout.fillWidth: true
            Layout.minimumHeight: 80
            model: loadFailuresModel

            header: RowLayout {
                width: loadFailuresTable.width
                spacing: Kirigami.Units.smallSpacing

                Label { text: qsTr("Node"); font.bold: true; Layout.preferredWidth: 140 }
                Label { text: qsTr("File / source"); font.bold: true; Layout.fillWidth: true }
                Label { text: qsTr("Layer"); font.bold: true; Layout.preferredWidth: 160 }
                Label { text: qsTr("Error"); font.bold: true; Layout.preferredWidth: 220 }
            }

            delegate: RowLayout {
                width: loadFailuresTable.width
                spacing: Kirigami.Units.smallSpacing

                Label { text: model.rowName; elide: Text.ElideRight; Layout.preferredWidth: 140 }
                Label { text: model.rowPath; elide: Text.ElideMiddle; Layout.fillWidth: true }
                Label { text: model.rowLayer; elide: Text.ElideRight; Layout.preferredWidth: 160 }
                Label { text: model.rowError; elide: Text.ElideRight; Layout.preferredWidth: 220; color: "crimson" }
            }

            ScrollBar.vertical: ScrollBar {}
        }

        Item { Layout.fillHeight: true; visible: !perfMetricsCheck.checked && !nodeTelemetryCheck.checked }
    }
}
