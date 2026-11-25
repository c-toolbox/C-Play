/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import org.kde.kirigami as Kirigami

// Line chart of one GPU load metric (0..100 %) over the time of a node measurement, one
// line per node. Data comes from PlayerController.nodeMeasurementTimeline(); colours and
// the shared crosshair are owned by the parent so that stacked charts stay in step.
Item {
    id: root

    // { startMs, durationMs, bucketMs, series: [ { name, t: [], render: [], decode: [], busiest: [] } ] }
    property var timeline: ({})
    // Which load array of each series to draw: "render", "decode" or "busiest".
    property string metric: "render"
    property string title: ""
    // Per series (same order as timeline.series): { color, other }. "other" series are
    // drawn as thin muted lines behind the coloured ones (the categorical palette has 8 slots).
    property var seriesStyles: []
    // Crosshair position in ms since the start, -1 = none. Set by the parent from hoverMoved.
    property real crosshairT: -1
    // Name each line at its right end (used for up to 4 coloured series).
    property bool directLabels: false

    signal hoverMoved(real t)

    readonly property var seriesList: (timeline && timeline.series) ? timeline.series : []
    readonly property real durationMs: (timeline && timeline.durationMs) ? timeline.durationMs : 0
    readonly property real bucketMs: (timeline && timeline.bucketMs) ? timeline.bucketMs : 1000
    readonly property bool hasData: seriesList.some(s => s.t.length > 0)

    // Plot area inside the item.
    readonly property real padLeft: 40
    readonly property real padRight: directLabels ? 120 : 12
    readonly property real padTop: titleLabel.implicitHeight + 6
    readonly property real padBottom: 20
    readonly property real plotW: Math.max(1, width - padLeft - padRight)
    readonly property real plotH: Math.max(1, height - padTop - padBottom)

    // Chart chrome derived from the Kirigami theme, so light and dark both work.
    readonly property color inkPrimary: Kirigami.Theme.textColor
    readonly property color inkMuted: Kirigami.Theme.disabledTextColor
    readonly property color surface: Kirigami.Theme.alternateBackgroundColor

    implicitHeight: 160

    function rgba(c, a) {
        return "rgba(" + Math.round(c.r * 255) + "," + Math.round(c.g * 255) + "," + Math.round(c.b * 255) + "," + a + ")";
    }

    function fmtTime(ms) {
        const total = Math.round(Math.max(0, ms) / 1000);
        const h = Math.floor(total / 3600);
        const m = Math.floor((total % 3600) / 60);
        const s = total % 60;
        const pad = n => (n < 10 ? "0" : "") + n;
        return h > 0 ? h + ":" + pad(m) + ":" + pad(s) : m + ":" + pad(s);
    }

    function xFor(t) {
        return padLeft + (durationMs > 0 ? t / durationMs : 0) * plotW;
    }

    function yFor(v) {
        return padTop + (1 - Math.min(100, Math.max(0, v)) / 100) * plotH;
    }

    // Index of the sample nearest to t in a sorted time array, -1 if empty.
    function nearestIndex(ts, t) {
        if (!ts || ts.length === 0)
            return -1;
        let lo = 0, hi = ts.length - 1;
        while (lo < hi) {
            const mid = (lo + hi) >> 1;
            if (ts[mid] < t)
                lo = mid + 1;
            else
                hi = mid;
        }
        if (lo > 0 && Math.abs(ts[lo - 1] - t) <= Math.abs(ts[lo] - t))
            lo--;
        return lo;
    }

    // Value of series s at t, or -1 when the node had no report near t.
    function valueAt(s, t) {
        const i = nearestIndex(s.t, t);
        if (i < 0 || Math.abs(s.t[i] - t) > Math.max(bucketMs, 1500))
            return -1;
        return s[metric][i];
    }

    // Tick step for the time axis: about one label per 90 px.
    function timeStep() {
        const steps = [1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 900, 1800, 3600, 7200].map(s => s * 1000);
        const wanted = durationMs / Math.max(1, plotW / 90);
        for (const s of steps) {
            if (s >= wanted)
                return s;
        }
        return steps[steps.length - 1];
    }

    // Readout rows for the hover tooltip: every series at the crosshair, highest first.
    readonly property var readout: {
        if (crosshairT < 0 || !hoverArea.containsMouse)
            return [];
        const rows = [];
        for (let i = 0; i < seriesList.length; ++i) {
            const v = valueAt(seriesList[i], crosshairT);
            const style = seriesStyles[i] || {};
            rows.push({
                "name": seriesList[i].name,
                "value": v >= 0 ? Math.round(v) + " %" : qsTr("n/a"),
                "sort": v,
                "color": style.color || inkMuted
            });
        }
        rows.sort((a, b) => b.sort - a.sort);
        return rows;
    }

    onTimelineChanged: canvas.requestPaint()
    onMetricChanged: canvas.requestPaint()
    onSeriesStylesChanged: canvas.requestPaint()
    onCrosshairTChanged: canvas.requestPaint()
    onDirectLabelsChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()
    onInkPrimaryChanged: canvas.requestPaint()

    Label {
        id: titleLabel
        x: root.padLeft
        text: root.title
        font.bold: true
    }

    Canvas {
        id: canvas
        anchors.fill: parent

        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            ctx.font = Math.round(Kirigami.Theme.smallFont.pixelSize > 0 ? Kirigami.Theme.smallFont.pixelSize : 11) + "px sans-serif";

            const left = root.padLeft, top = root.padTop, w = root.plotW, h = root.plotH;
            const gridColor = root.rgba(root.inkPrimary, 0.10);
            const axisColor = root.rgba(root.inkPrimary, 0.28);
            const mutedText = root.rgba(root.inkMuted, 1.0);

            // Gridlines and y labels at 0/25/50/75/100 %.
            ctx.lineWidth = 1;
            ctx.textAlign = "right";
            ctx.textBaseline = "middle";
            for (let p = 0; p <= 100; p += 25) {
                const y = Math.round(root.yFor(p)) + 0.5;
                ctx.strokeStyle = p === 0 ? axisColor : gridColor;
                ctx.beginPath();
                ctx.moveTo(left, y);
                ctx.lineTo(left + w, y);
                ctx.stroke();
                ctx.fillStyle = mutedText;
                ctx.fillText(p + " %", left - 6, y);
            }

            if (!root.hasData || root.durationMs <= 0)
                return;

            // Time axis labels.
            ctx.textAlign = "center";
            ctx.textBaseline = "top";
            const step = root.timeStep();
            for (let t = 0; t <= root.durationMs; t += step) {
                const x = root.xFor(t);
                ctx.fillStyle = mutedText;
                ctx.fillText(root.fmtTime(t), Math.min(Math.max(x, left + 14), left + w - 14), top + h + 5);
            }

            // A gap longer than this (node offline, or telemetry paused) breaks the line.
            const breakMs = Math.max(3500, root.bucketMs * 2.5);

            const drawSeries = (s, style) => {
                const ts = s.t, vs = s[root.metric];
                ctx.strokeStyle = style.other ? root.rgba(root.inkMuted, 0.55) : style.color;
                ctx.fillStyle = ctx.strokeStyle;
                ctx.lineWidth = style.other ? 1 : 2;
                ctx.lineJoin = "round";
                ctx.lineCap = "round";
                let open = false, runStart = -1;
                ctx.beginPath();
                for (let i = 0; i < ts.length; ++i) {
                    const v = vs[i];
                    const broken = v < 0 || (i > 0 && ts[i] - ts[i - 1] > breakMs);
                    if (broken && open) {
                        if (runStart === i - 1) // a lone point: show it as a dot
                            ctx.fillRect(root.xFor(ts[i - 1]) - 1.5, root.yFor(vs[i - 1]) - 1.5, 3, 3);
                        open = false;
                    }
                    if (v < 0)
                        continue;
                    const x = root.xFor(ts[i]), y = root.yFor(v);
                    if (!open) {
                        ctx.moveTo(x, y);
                        open = true;
                        runStart = i;
                    } else {
                        ctx.lineTo(x, y);
                    }
                }
                ctx.stroke();
                if (open && runStart === ts.length - 1)
                    ctx.fillRect(root.xFor(ts[runStart]) - 1.5, root.yFor(vs[runStart]) - 1.5, 3, 3);
            };

            // Folded "other" nodes behind, then the coloured ones with slot 1 on top.
            const list = root.seriesList;
            for (let i = 0; i < list.length; ++i) {
                if ((root.seriesStyles[i] || {}).other)
                    drawSeries(list[i], root.seriesStyles[i]);
            }
            for (let i = list.length - 1; i >= 0; --i) {
                const style = root.seriesStyles[i] || {};
                if (!style.other)
                    drawSeries(list[i], style);
            }

            // Direct labels at each coloured line's last value, pushed apart vertically.
            if (root.directLabels) {
                const labels = [];
                for (let i = 0; i < list.length; ++i) {
                    const style = root.seriesStyles[i] || {};
                    const vs = list[i][root.metric];
                    if (style.other)
                        continue;
                    for (let j = vs.length - 1; j >= 0; --j) {
                        if (vs[j] >= 0) {
                            labels.push({ "y": root.yFor(vs[j]), "name": list[i].name, "color": style.color });
                            break;
                        }
                    }
                }
                labels.sort((a, b) => a.y - b.y);
                const gap = 14;
                for (let i = 1; i < labels.length; ++i)
                    labels[i].y = Math.max(labels[i].y, labels[i - 1].y + gap);
                const overflow = labels.length > 0 ? labels[labels.length - 1].y - (top + h) : 0;
                if (overflow > 0) {
                    for (const l of labels)
                        l.y -= overflow;
                }
                ctx.textAlign = "left";
                ctx.textBaseline = "middle";
                const lx = left + w + 8;
                for (const l of labels) {
                    ctx.strokeStyle = l.color;
                    ctx.lineWidth = 2;
                    ctx.beginPath();
                    ctx.moveTo(lx, l.y);
                    ctx.lineTo(lx + 10, l.y);
                    ctx.stroke();
                    ctx.fillStyle = root.rgba(root.inkPrimary, 0.85);
                    const name = l.name.length > 16 ? l.name.substring(0, 15) + "…" : l.name;
                    ctx.fillText(name, lx + 14, l.y);
                }
            }

            // Crosshair: a hairline at the snapped time and a ringed dot on every line.
            if (root.crosshairT >= 0) {
                const x = Math.round(root.xFor(root.crosshairT)) + 0.5;
                ctx.strokeStyle = axisColor;
                ctx.lineWidth = 1;
                ctx.beginPath();
                ctx.moveTo(x, top);
                ctx.lineTo(x, top + h);
                ctx.stroke();
                for (let i = 0; i < list.length; ++i) {
                    const style = root.seriesStyles[i] || {};
                    const v = root.valueAt(list[i], root.crosshairT);
                    if (v < 0 || style.other)
                        continue;
                    ctx.beginPath();
                    ctx.arc(x, root.yFor(v), 4, 0, 2 * Math.PI);
                    ctx.fillStyle = style.color;
                    ctx.fill();
                    ctx.lineWidth = 2;
                    ctx.strokeStyle = root.rgba(root.surface, 1.0);
                    ctx.stroke();
                }
            }
        }
    }

    Label {
        anchors.centerIn: parent
        visible: !root.hasData
        text: qsTr("No GPU reports recorded yet.")
        opacity: 0.6
    }

    MouseArea {
        id: hoverArea
        x: root.padLeft
        y: root.padTop
        width: root.plotW
        height: root.plotH
        hoverEnabled: true
        acceptedButtons: Qt.NoButton

        onPositionChanged: mouse => {
            if (!root.hasData || root.durationMs <= 0)
                return;
            const t = Math.min(Math.max(mouse.x / width, 0), 1) * root.durationMs;
            // Snap to the centre of the nearest time slot, where the data points are.
            const b = root.bucketMs;
            root.hoverMoved(Math.min(root.durationMs, Math.floor(t / b) * b + b / 2));
        }
        onExited: root.hoverMoved(-1)
    }

    // Readout for the hovered chart: the time, then every node's value (value first).
    Rectangle {
        id: readoutBox
        visible: root.readout.length > 0
        readonly property real cx: root.xFor(root.crosshairT)
        x: cx + 12 + width > root.width ? cx - 12 - width : cx + 12
        y: root.padTop + 4
        width: readoutColumn.implicitWidth + 2 * Kirigami.Units.smallSpacing
        height: readoutColumn.implicitHeight + 2 * Kirigami.Units.smallSpacing
        radius: 4
        color: Kirigami.Theme.backgroundColor
        border.color: root.rgba(root.inkPrimary, 0.15)
        border.width: 1

        ColumnLayout {
            id: readoutColumn
            x: Kirigami.Units.smallSpacing
            y: Kirigami.Units.smallSpacing
            spacing: 2

            Label {
                text: root.fmtTime(root.crosshairT)
                color: root.inkMuted
                font: Kirigami.Theme.smallFont
            }
            Repeater {
                model: root.readout.length > 12 ? root.readout.slice(0, 12) : root.readout
                delegate: RowLayout {
                    required property var modelData
                    spacing: Kirigami.Units.smallSpacing
                    Rectangle {
                        Layout.preferredWidth: 10
                        Layout.preferredHeight: 2
                        color: modelData.color
                    }
                    Label {
                        text: modelData.value
                        font.bold: true
                        Layout.minimumWidth: 40
                    }
                    Label {
                        text: modelData.name
                        color: root.inkMuted
                    }
                }
            }
            Label {
                visible: root.readout.length > 12
                text: qsTr("+%1 more").arg(root.readout.length - 12)
                color: root.inkMuted
                font: Kirigami.Theme.smallFont
            }
        }
    }
}
