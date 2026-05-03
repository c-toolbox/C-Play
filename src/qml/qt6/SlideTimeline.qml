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
import org.ctoolbox.cplay

Window {
    id: root

    // -----------------------------------------------------------------------
    // Public API
    // -----------------------------------------------------------------------
    property int slideIndex: -1

    readonly property var slideModel: (slideIndex >= 0 && slideIndex < app.slides.numberOfSlides())
                                      ? app.slides.slide(slideIndex) : null

    // -----------------------------------------------------------------------
    // Window chrome
    // -----------------------------------------------------------------------
    function slideText() {
        return root.slideModel ? (slideIndex+1).toString() + ". " + root.slideModel.layersName : "";
    }

    title: slideModel ? qsTr("Timeline: ") + slideText() : qsTr("Timeline")
    width:  860
    height: 480
    minimumWidth:  560
    minimumHeight: 300
    flags: Qt.Window | Qt.WindowTitleHint | Qt.WindowCloseButtonHint | Qt.WindowMinMaxButtonsHint

    // -----------------------------------------------------------------------
    // Layout constants
    // -----------------------------------------------------------------------
    readonly property int    trackHeight:  40
    readonly property int    headerWidth:  140
    readonly property int    rulerHeight:  26
    readonly property int    kfSize:       12   // keyframe diamond bounding box

    // -----------------------------------------------------------------------
    // Timeline state
    // -----------------------------------------------------------------------
    property real  pxPerMs:    0.08
    property int   duration:   slideModel ? slideModel.timelineDuration : 5000
    property int   outroStartMs: slideModel ? slideModel.timelineOutroStart : -1
    property int   playheadMs: 0

    // true while the C++ timer is running — prevents onPlayheadMsChanged from
    // double-applying applyTimelineAt (the tick already does it)
    property bool  timelinePlaying: false

    // Currently selected keyframe: { layerIdx, kfIdx }  or  null
    property var   selectedKf: null

    // Bumped to force inspector spinboxes to re-read from the model
    property int   inspectorRevision: 0

    // Live drag state — updated every frame while dragging a keyframe
    property bool  draggingKf: false
    property int   dragLiveMs: 0
    property real  dragLiveAlpha: 0.0

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------
    function msToX(ms)  { return ms * pxPerMs; }
    function xToMs(x)   { return Math.round(x / pxPerMs); }
    function clampMs(ms){ return Math.max(0, Math.min(ms, duration)); }
    function alphaToY(a, h) { return (1.0 - a) * (h - kfSize) + kfSize * 0.5; }
    function yToAlpha(y, h) { return Math.max(0, Math.min(1, 1.0 - (y - kfSize * 0.5) / (h - kfSize))); }

    // Helper: read the currently selected keyframe data from the model
    function selectedKfData() {
        // Touch inspectorRevision so bindings using this function re-evaluate when it changes
        var _rev = root.inspectorRevision;
        if (!root.selectedKf || !root.slideModel) return null;
        var kfs = root.slideModel.getKeyframes(root.selectedKf.layerIdx);
        if (root.selectedKf.kfIdx < 0 || root.selectedKf.kfIdx >= kfs.length) return null;
        return kfs[root.selectedKf.kfIdx];
    }

    // Helper: commit inspector edits back to the model
    function commitSelectedKf(timeMs, alpha,
                               hasRotate, rx, ry, rz,
                               hasTranslate, tx, ty, tz) {
        if (!root.selectedKf || !root.slideModel) return;
        if (root.selectedKf.layerIdx < 0 || root.selectedKf.kfIdx < 0) return;
        root.slideModel.updateKeyframe(
            root.selectedKf.layerIdx, root.selectedKf.kfIdx,
            timeMs, alpha,
            hasRotate, rx, ry, rz,
            hasTranslate, tx, ty, tz);
        root.rebuildTrack(root.selectedKf.layerIdx);
        root.inspectorRevision++;
    }

    // layerCount: live count driven by the model so contentHeight updates correctly
    readonly property int layerCount: slideModel ? slideModel.rowCount() : 0

    function rebuildTrack(layerIdx) {
        var item = keyframesRepeater.itemAt(layerIdx);
        if (item) item.reload();
    }

    function rebuildAllTracks() {
        for (var i = 0; i < keyframesRepeater.count; i++) {
            var item = keyframesRepeater.itemAt(i);
            if (item) item.reload();
        }
    }

    // -----------------------------------------------------------------------
    // Reload tracks when the window becomes visible
    // -----------------------------------------------------------------------
    onVisibleChanged: {
        if (visible) {
            rebuildAllTracksTimer.restart();
        }
    }

    // Apply timeline alpha only when the user scrubs manually (not during C++ tick)
    onPlayheadMsChanged: {
        if (!timelinePlaying && root.slideModel && root.slideModel.hasTimeline) {
            root.slideModel.applyTimelineAt(root.playheadMs);
        }
    }

    Timer {
        id: rebuildAllTracksTimer
        interval: 50
        repeat: false
        onTriggered: root.rebuildAllTracks()
    }

    // -----------------------------------------------------------------------
    // Connections
    // -----------------------------------------------------------------------
    Connections {
        target: root.slideModel
        function onTimelineStarted() {
            root.timelinePlaying = true;
            playButton.checked   = true;
        }
        function onTimelinePositionChanged(posMs) {
            root.playheadMs = posMs;
        }
        function onTimelineStopped() {
            root.timelinePlaying = false;
            playButton.checked   = false;
        }
    }

    Connections {
        target: root.slideModel
        function onTimelineChanged() {
            if (root.slideModel) {
                root.duration = root.slideModel.timelineDuration;
                root.outroStartMs = root.slideModel.timelineOutroStart;
            }
            app.slides.slideContentChanged();
            rebuildAllTracksTimer.restart();
        }
        // Fired by LayersModel whenever a layer is added, removed or moved
        function onLayersModelChanged() {
            rebuildAllTracksTimer.restart();
        }
    }

    // -----------------------------------------------------------------------
    // Root layout
    // -----------------------------------------------------------------------
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ===================================================================
        // Toolbar
        // ===================================================================
        ToolBar {
            Layout.fillWidth: true

            RowLayout {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6

                CheckBox {
                    id: enableCheck
                    text: qsTr("Timeline enabled")
                    checked: root.slideModel ? root.slideModel.hasTimeline : false
                    onCheckedChanged: {
                        if (root.slideModel && root.slideModel.hasTimeline !== checked)
                            root.slideModel.hasTimeline = checked;
                    }
                }

                ToolSeparator {}

                // All controls below are greyed out when timeline is disabled
                Label {
                    text: qsTr("Duration (ms):")
                    enabled: enableCheck.checked
                }
                SpinBox {
                    id: durationSpin
                    from: 100; to: 3600000; stepSize: 100
                    editable: true
                    enabled: enableCheck.checked
                    value: root.duration
                    onValueModified: {
                        if (root.slideModel) root.slideModel.timelineDuration = value;
                    }
                }

                ToolSeparator { enabled: enableCheck.checked }

                Label {
                    text: qsTr("Outro start (ms):")
                    enabled: enableCheck.checked
                }
                SpinBox {
                    id: outroStartSpin
                    from: -1; to: root.duration - 1; stepSize: 100
                    editable: true
                    enabled: enableCheck.checked
                    value: root.outroStartMs
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Set where the outro begins. -1 = no outro / whole timeline is intro.")
                    onValueModified: {
                        if (root.slideModel) root.slideModel.timelineOutroStart = value;
                    }
                }

                ToolSeparator { enabled: enableCheck.checked }

                Button {
                    id: rewindButton
                    icon.name: "media-skip-backward"
                    text: qsTr("Rewind")
                    enabled: enableCheck.checked && !root.timelinePlaying
                    onClicked: {
                        root.playheadMs = 0;
                    }
                }

                Button {
                    id: playButton
                    checkable: true
                    icon.name: checked ? "media-playback-stop" : "media-playback-start"
                    text: checked ? qsTr("Stop") : qsTr("Play")
                    enabled: enableCheck.checked
                    onClicked: {
                        if (checked) {
                            app.slides.startTimelineFrom(root.slideIndex, root.playheadMs);
                        } else {
                            app.slides.stopTimeline(root.slideIndex);
                        }
                    }
                }

                ToolSeparator { enabled: enableCheck.checked }

                Label {
                    text: qsTr("Zoom:")
                    enabled: enableCheck.checked
                }
                Slider {
                    from: 0.01; to: 0.80; stepSize: 0.005
                    implicitWidth: 110
                    enabled: enableCheck.checked
                    value: root.pxPerMs
                    onValueChanged: root.pxPerMs = value
                }

                Label {
                    text: (root.playheadMs / 1000).toFixed(2) + " s"
                    font.bold: true
                    color: enableCheck.checked ? "#f77" : "#777"
                    leftPadding: 8
                }

                Item { Layout.fillWidth: true }
            }
        }

        // ===================================================================
        // Selected-keyframe inspector
        // ===================================================================
        Rectangle {
            Layout.fillWidth: true
            height: root.selectedKf !== null ? inspectorLayout.implicitHeight + 8 : 0
            visible: root.selectedKf !== null
            color: Qt.rgba(0.10, 0.10, 0.10, 1)
            clip: true

            ColumnLayout {
                id: inspectorLayout
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: root.headerWidth + 8
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                visible: root.selectedKf !== null

                // ---- Row 1: time, alpha, delete ----
                RowLayout {
                    spacing: 10
                    Layout.fillWidth: true

                    Label { text: qsTr("Selected keyframe:"); color: "#ccc"; font.pointSize: 8 }

                    Label { text: qsTr("Time (ms):"); color: "#aaa"; font.pointSize: 8 }
                    SpinBox {
                        id: kfTimeSpin
                        from: 0; to: root.duration; stepSize: 10
                        editable: true
                        value: {
                            if (root.draggingKf) return root.dragLiveMs;
                            var _r = root.inspectorRevision; var d = root.selectedKfData(); return d ? d.timeMs : 0;
                        }
                        implicitWidth: 90
                        onValueModified: {
                            var d = root.selectedKfData(); if (!d) return;
                            root.commitSelectedKf(value, d.alpha,
                                d.hasRotate,    d.rotateX,    d.rotateY,    d.rotateZ,
                                d.hasTranslate, d.translateX, d.translateY, d.translateZ);
                        }
                    }

                    Label { text: qsTr("Alpha:"); color: "#aaa"; font.pointSize: 8 }
                    SpinBox {
                        id: kfAlphaSpin
                        from: 0; to: 100; stepSize: 1
                        editable: true
                        value: {
                            if (root.draggingKf) return Math.round(root.dragLiveAlpha * 100);
                            var _r = root.inspectorRevision; var d = root.selectedKfData(); return d ? Math.round(d.alpha * 100) : 0;
                        }
                        implicitWidth: 72
                        onValueModified: {
                            var d = root.selectedKfData(); if (!d) return;
                            root.commitSelectedKf(d.timeMs, value / 100.0,
                                d.hasRotate,    d.rotateX,    d.rotateY,    d.rotateZ,
                                d.hasTranslate, d.translateX, d.translateY, d.translateZ);
                        }
                    }

                    Button {
                        text: qsTr("Delete")
                        icon.name: "edit-delete"
                        onClicked: {
                            if (root.selectedKf && root.slideModel) {
                                root.slideModel.removeKeyframe(root.selectedKf.layerIdx, root.selectedKf.kfIdx);
                                root.rebuildTrack(root.selectedKf.layerIdx);
                                root.selectedKf = null;
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                // ---- Row 2: optional rotation ----
                RowLayout {
                    spacing: 10
                    Layout.fillWidth: true

                    CheckBox {
                        id: hasRotateCheck
                        text: qsTr("Rotation")
                        font.pointSize: 8
                        checked: { var _r = root.inspectorRevision; var d = root.selectedKfData(); return d ? d.hasRotate : false; }
                        onCheckedChanged: {
                            var d = root.selectedKfData(); if (!d) return;
                            root.commitSelectedKf(d.timeMs, d.alpha,
                                checked, d.rotateX, d.rotateY, d.rotateZ,
                                d.hasTranslate, d.translateX, d.translateY, d.translateZ);
                        }
                    }

                    Label { text: qsTr("X:"); color: "#aaa"; font.pointSize: 8; enabled: hasRotateCheck.checked }
                    SpinBox {
                        from: -36000; to: 36000; stepSize: 10
                        editable: true
                        enabled: hasRotateCheck.checked
                        value: { var _r = root.inspectorRevision; var d = root.selectedKfData(); return d ? Math.round(d.rotateX * 10) : 0; }
                        implicitWidth: 80
                        textFromValue: function(v) { return (v / 10.0).toFixed(1) + "\u00b0"; }
                        valueFromText: function(t) {
                            var parsed = parseFloat(t);
                            return isNaN(parsed) ? value : Math.round(parsed * 10);
                        }
                        onValueModified: {
                            var d = root.selectedKfData(); if (!d) return;
                            root.commitSelectedKf(d.timeMs, d.alpha,
                                hasRotateCheck.checked, value / 10.0, d.rotateY, d.rotateZ,
                                d.hasTranslate, d.translateX, d.translateY, d.translateZ);
                        }
                    }

                    Label { text: qsTr("Y:"); color: "#aaa"; font.pointSize: 8; enabled: hasRotateCheck.checked }
                    SpinBox {
                        from: -36000; to: 36000; stepSize: 10
                        editable: true
                        enabled: hasRotateCheck.checked
                        value: { var _r = root.inspectorRevision; var d = root.selectedKfData(); return d ? Math.round(d.rotateY * 10) : 0; }
                        implicitWidth: 80
                        textFromValue: function(v) { return (v / 10.0).toFixed(1) + "\u00b0"; }
                        valueFromText: function(t) {
                            var parsed = parseFloat(t);
                            return isNaN(parsed) ? value : Math.round(parsed * 10);
                        }
                        onValueModified: {
                            var d = root.selectedKfData(); if (!d) return;
                            root.commitSelectedKf(d.timeMs, d.alpha,
                                hasRotateCheck.checked, d.rotateX, value / 10.0, d.rotateZ,
                                d.hasTranslate, d.translateX, d.translateY, d.translateZ);
                        }
                    }

                    Label { text: qsTr("Z:"); color: "#aaa"; font.pointSize: 8; enabled: hasRotateCheck.checked }
                    SpinBox {
                        from: -36000; to: 36000; stepSize: 10
                        editable: true
                        enabled: hasRotateCheck.checked
                        value: { var _r = root.inspectorRevision; var d = root.selectedKfData(); return d ? Math.round(d.rotateZ * 10) : 0; }
                        implicitWidth: 80
                        textFromValue: function(v) { return (v / 10.0).toFixed(1) + "\u00b0"; }
                        valueFromText: function(t) {
                            var parsed = parseFloat(t);
                            return isNaN(parsed) ? value : Math.round(parsed * 10);
                        }
                        onValueModified: {
                            var d = root.selectedKfData(); if (!d) return;
                            root.commitSelectedKf(d.timeMs, d.alpha,
                                hasRotateCheck.checked, d.rotateX, d.rotateY, value / 10.0,
                                d.hasTranslate, d.translateX, d.translateY, d.translateZ);
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                // ---- Row 3: optional translation ----
                RowLayout {
                    spacing: 10
                    Layout.fillWidth: true

                    CheckBox {
                        id: hasTranslateCheck
                        text: qsTr("Translation")
                        font.pointSize: 8
                        checked: { var _r = root.inspectorRevision; var d = root.selectedKfData(); return d ? d.hasTranslate : false; }
                        onCheckedChanged: {
                            var d = root.selectedKfData(); if (!d) return;
                            root.commitSelectedKf(d.timeMs, d.alpha,
                                d.hasRotate, d.rotateX, d.rotateY, d.rotateZ,
                                checked, d.translateX, d.translateY, d.translateZ);
                        }
                    }

                    Label { text: qsTr("X:"); color: "#aaa"; font.pointSize: 8; enabled: hasTranslateCheck.checked }
                    SpinBox {
                        from: -100000; to: 100000; stepSize: 10
                        editable: true
                        enabled: hasTranslateCheck.checked
                        value: { var _r = root.inspectorRevision; var d = root.selectedKfData(); return d ? Math.round(d.translateX * 100) : 0; }
                        implicitWidth: 90
                        textFromValue: function(v) { return (v / 100.0).toFixed(2); }
                        valueFromText: function(t) {
                            var parsed = parseFloat(t);
                            return isNaN(parsed) ? value : Math.round(parsed * 100);
                        }
                        onValueModified: {
                            var d = root.selectedKfData(); if (!d) return;
                            root.commitSelectedKf(d.timeMs, d.alpha,
                                d.hasRotate, d.rotateX, d.rotateY, d.rotateZ,
                                d.hasTranslate, value / 100.0, d.translateY, d.translateZ);
                        }
                    }

                    Label { text: qsTr("Y:"); color: "#aaa"; font.pointSize: 8; enabled: hasTranslateCheck.checked }
                    SpinBox {
                        from: -100000; to: 100000; stepSize: 10
                        editable: true
                        enabled: hasTranslateCheck.checked
                        value: { var _r = root.inspectorRevision; var d = root.selectedKfData(); return d ? Math.round(d.translateY * 100) : 0; }
                        implicitWidth: 90
                        textFromValue: function(v) { return (v / 100.0).toFixed(2); }
                        valueFromText: function(t) {
                            var parsed = parseFloat(t);
                            return isNaN(parsed) ? value : Math.round(parsed * 100);
                        }
                        onValueModified: {
                            var d = root.selectedKfData(); if (!d) return;
                            root.commitSelectedKf(d.timeMs, d.alpha,
                                d.hasRotate, d.rotateX, d.rotateY, d.rotateZ,
                                d.hasTranslate, d.translateX, value / 100.0, d.translateZ);
                        }
                    }

                    Label { text: qsTr("Z:"); color: "#aaa"; font.pointSize: 8; enabled: hasTranslateCheck.checked }
                    SpinBox {
                        from: -100000; to: 100000; stepSize: 10
                        editable: true
                        enabled: hasTranslateCheck.checked
                        value: { var _r = root.inspectorRevision; var d = root.selectedKfData(); return d ? Math.round(d.translateZ * 100) : 0; }
                        implicitWidth: 90
                        textFromValue: function(v) { return (v / 100.0).toFixed(2); }
                        valueFromText: function(t) {
                            var parsed = parseFloat(t);
                            return isNaN(parsed) ? value : Math.round(parsed * 100);
                        }
                        onValueModified: {
                            var d = root.selectedKfData(); if (!d) return;
                            root.commitSelectedKf(d.timeMs, d.alpha,
                                d.hasRotate, d.rotateX, d.rotateY, d.rotateZ,
                                d.hasTranslate, d.translateX, d.translateY, value / 100.0);
                        }
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }

        // ===================================================================
        // Main editing area
        // ===================================================================
        Item {
            id: mainArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            opacity: enableCheck.checked ? 1.0 : 0.35

            // -----------------------------------------------------------------
            // Fixed header column (layer names) — scrolls vertically with Flickable
            // -----------------------------------------------------------------
            Rectangle {
                id: headerColumn
                x: 0
                y: root.rulerHeight
                width: root.headerWidth
                height: mainArea.height - root.rulerHeight
                color: Qt.rgba(0.12, 0.12, 0.12, 1)
                z: 3
                clip: true

                Column {
                    id: headerTrackColumn
                    // Mirror the vertical scroll of the Flickable
                    y: -trackArea.contentY
                    width: parent.width
                    spacing: 1

                    // ----------------------------------------------------------
                    // Use slideModel (QAbstractListModel) as model so row
                    // insertions / removals are reflected automatically.
                    // The 'title' role is provided by LayersModel::roleNames().
                    // ----------------------------------------------------------
                    Repeater {
                        model: root.slideModel

                        Rectangle {
                            width: root.headerWidth
                            height: root.trackHeight
                            color: index % 2 === 0 ? Qt.rgba(0.14, 0.14, 0.14, 1)
                                                   : Qt.rgba(0.18, 0.18, 0.18, 1)

                            Label {
                                anchors.fill: parent
                                anchors.leftMargin: 6
                                anchors.rightMargin: 30
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                                // 'title' is the TitleRole exposed by LayersModel
                                text: model.title !== undefined ? model.title : ""
                                color: Kirigami.Theme.textColor
                                font.pointSize: 8
                            }

                            Label {
                                anchors.right: parent.right
                                anchors.rightMargin: 4
                                anchors.verticalCenter: parent.verticalCenter
                                // 'visibility' role is 0-100 integer
                                text: model.visibility !== undefined ? model.visibility + "%" : ""
                                color: "#888"
                                font.pointSize: 7
                            }
                        }
                    }
                }
            }

            // Corner piece above the header column
            Rectangle {
                x: 0; y: 0
                width: root.headerWidth
                height: root.rulerHeight
                color: Qt.rgba(0.10, 0.10, 0.10, 1)
                z: 4
            }

            // -----------------------------------------------------------------
            // Scrollable timeline canvas
            // -----------------------------------------------------------------
            Flickable {
                id: trackArea
                x: root.headerWidth
                y: 0
                width:  mainArea.width  - root.headerWidth
                height: mainArea.height
                contentWidth:  Math.max(root.msToX(root.duration) + 120, width)
                // Use keyframesRepeater.count (updated reactively) for height
                contentHeight: root.rulerHeight
                               + keyframesRepeater.count * (root.trackHeight + 1)
                               + 20
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOn }
                ScrollBar.vertical:   ScrollBar { policy: ScrollBar.AsNeeded }

                // =============================================================
                // Ruler
                // =============================================================
                Canvas {
                    id: ruler
                    x: 0; y: 0
                    width: trackArea.contentWidth
                    height: root.rulerHeight
                    z: 2

                    readonly property int tickInterval: {
                        if      (root.pxPerMs >= 0.4)  return 100
                        else if (root.pxPerMs >= 0.08) return 500
                        else if (root.pxPerMs >= 0.03) return 1000
                        else                            return 5000
                    }

                    onPaint: {
                        var ctx = getContext("2d");
                        ctx.clearRect(0, 0, width, height);
                        ctx.fillStyle = Qt.rgba(0.10, 0.10, 0.10, 1);
                        ctx.fillRect(0, 0, width, height);

                        var step = tickInterval;
                        var ms = 0;
                        while (ms <= root.duration + step) {
                            var px = root.msToX(ms);
                            var major = (ms % (step * 5) === 0);
                            ctx.strokeStyle = major ? "#bbb" : "#555";
                            ctx.lineWidth = major ? 1.5 : 0.8;
                            ctx.beginPath();
                            ctx.moveTo(px, major ? 0 : height * 0.55);
                            ctx.lineTo(px, height);
                            ctx.stroke();
                            if (major || step <= 500) {
                                ctx.fillStyle = "#ccc";
                                ctx.font = "9px sans-serif";
                                ctx.fillText((ms / 1000).toFixed(ms % 1000 === 0 ? 0 : 1) + "s", px + 2, height - 3);
                            }
                            ms += step;
                        }
                        var ex = root.msToX(root.duration);
                        ctx.strokeStyle = "#f44";
                        ctx.lineWidth = 2;
                        ctx.beginPath();
                        ctx.moveTo(ex, 0);
                        ctx.lineTo(ex, height);
                        ctx.stroke();

                        // Outro divider in ruler
                        if (root.outroStartMs > 0 && root.outroStartMs < root.duration) {
                            var ox = root.msToX(root.outroStartMs);
                            ctx.strokeStyle = "#fa0";
                            ctx.lineWidth = 2;
                            ctx.beginPath();
                            ctx.moveTo(ox, 0);
                            ctx.lineTo(ox, height);
                            ctx.stroke();
                            ctx.fillStyle = "#fa0";
                            ctx.font = "bold 8px sans-serif";
                            ctx.fillText(qsTr("OUT"), ox + 3, height - 3);
                        }
                    }

                    Connections {
                        target: root
                        function onPxPerMsChanged() { ruler.requestPaint(); }
                        function onDurationChanged() { ruler.requestPaint(); }
                        function onOutroStartMsChanged() { ruler.requestPaint(); }
                    }
                }

                // =============================================================
                // Track rows — driven by the same QAbstractListModel
                // =============================================================
                Column {
                    id: tracksColumn
                    x: 0
                    y: root.rulerHeight
                    spacing: 1

                    Repeater {
                        id: keyframesRepeater
                        // Use slideModel here too — guaranteed same row count as header
                        model: root.slideModel

                        // --------------------------------------------------------- 
                        // One track row per layer
                        // ---------------------------------------------------------
                        Item {
                            id: trackRow
                            // 'index' is the row index in the model — this is the layerIdx
                            readonly property int layerIdx: index

                            // kfData: array of { timeMs, alpha }
                            property var kfData: []

                            width: trackArea.contentWidth
                            height: root.trackHeight

                            function reload() {
                                kfData = root.slideModel ? root.slideModel.getKeyframes(layerIdx) : [];
                            }

                            Component.onCompleted: reload()

                            // ---- Track background ----
                            Rectangle {
                                anchors.fill: parent
                                color: index % 2 === 0 ? Qt.rgba(0.16, 0.16, 0.16, 1)
                                                       : Qt.rgba(0.20, 0.20, 0.20, 1)
                            }

                            Rectangle {
                                x: 0; y: parent.height * 0.5 - 1
                                width: parent.width; height: 1
                                color: Qt.rgba(1, 1, 1, 0.06)
                            }

                            // ---- Clip block (first-to-last keyframe span) ----
                            readonly property int  clipStartMs: kfData.length > 0 ? kfData[0].timeMs : 0
                            readonly property int  clipEndMs:   kfData.length > 0 ? kfData[kfData.length - 1].timeMs : 0
                            readonly property real clipX: root.msToX(clipStartMs)
                            readonly property real clipW: Math.max(0, root.msToX(clipEndMs) - clipX)

                            Rectangle {
                                id: clipBody
                                x:      trackRow.clipX
                                y:      3
                                width:  Math.max(trackRow.clipW, 0)
                                height: trackRow.height - 6
                                color:  Qt.rgba(0.20, 0.45, 0.70, 0.35)
                                border.color: Qt.rgba(0.35, 0.65, 1.0, 0.70)
                                border.width: 1
                                radius: 3
                                clip: true
                                visible: trackRow.kfData.length > 0

                                Canvas {
                                    id: clipCurve
                                    anchors.fill: parent
                                    onPaint: {
                                        var ctx = getContext("2d");
                                        ctx.clearRect(0, 0, width, height);
                                        if (trackRow.kfData.length < 1) return;

                                        ctx.fillStyle = Qt.rgba(0.25, 0.65, 1.0, 0.18);
                                        ctx.beginPath();
                                        var firstX = root.msToX(trackRow.kfData[0].timeMs) - trackRow.clipX;
                                        ctx.moveTo(firstX, height);
                                        for (var i = 0; i < trackRow.kfData.length; i++) {
                                            var kf = trackRow.kfData[i];
                                            ctx.lineTo(root.msToX(kf.timeMs) - trackRow.clipX,
                                                       root.alphaToY(kf.alpha, height));
                                        }
                                        ctx.lineTo(root.msToX(trackRow.kfData[trackRow.kfData.length - 1].timeMs) - trackRow.clipX, height);
                                        ctx.closePath();
                                        ctx.fill();

                                        ctx.strokeStyle = Qt.rgba(0.35, 0.75, 1.0, 0.90);
                                        ctx.lineWidth = 1.5;
                                        ctx.beginPath();
                                        for (var j = 0; j < trackRow.kfData.length; j++) {
                                            var kf2 = trackRow.kfData[j];
                                            var px2 = root.msToX(kf2.timeMs) - trackRow.clipX;
                                            var py2 = root.alphaToY(kf2.alpha, height);
                                            if (j === 0) ctx.moveTo(px2, py2);
                                            else         ctx.lineTo(px2, py2);
                                        }
                                        ctx.stroke();
                                    }

                                    Connections {
                                        target: trackRow
                                        function onKfDataChanged()  { clipCurve.requestPaint(); }
                                        function onClipXChanged()   { clipCurve.requestPaint(); }
                                    }
                                    Connections {
                                        target: root
                                        function onPxPerMsChanged() { clipCurve.requestPaint(); }
                                    }
                                }
                            }

                            // ---- Double-click on track to add keyframe ----
                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton
                                z: 0
                                onDoubleClicked: function(mouse) {
                                    if (!root.slideModel || !root.slideModel.hasTimeline) return;
                                    var ms    = root.clampMs(root.xToMs(mouse.x));
                                    var alpha = root.yToAlpha(mouse.y, trackRow.height);
                                    // Snap alpha to 0 or 1 when near edges
                                    if (alpha <= 0.08) alpha = 0.0;
                                    else if (alpha >= 0.92) alpha = 1.0;
                                    root.slideModel.addKeyframe(trackRow.layerIdx, ms, alpha,
                                        false, 0, 0, 0,
                                        false, 0, 0, 0);
                                    trackRow.reload();
                                    var kfs = root.slideModel.getKeyframes(trackRow.layerIdx);
                                    for (var i = 0; i < kfs.length; i++) {
                                        if (kfs[i].timeMs === ms) {
                                            root.selectedKf = { layerIdx: trackRow.layerIdx, kfIdx: i };
                                            break;
                                        }
                                    }
                                    root.inspectorRevision++;
                                }
                                onClicked: function(mouse) {
                                    root.selectedKf = null;
                                }
                            }

                            // ---- Keyframe diamond handles ----
                            Repeater {
                                id: kfRepeater
                                model: trackRow.kfData

                                Item {
                                    id: kfItem
                                    readonly property int  kfIndex: index
                                    readonly property var  kf: trackRow.kfData[index]
                                    readonly property bool isSelected: root.selectedKf !== null
                                                                       && root.selectedKf.layerIdx === trackRow.layerIdx
                                                                       && root.selectedKf.kfIdx    === kfIndex

                                    property real dragX: root.msToX(kf.timeMs)
                                    property real dragY: root.alphaToY(kf.alpha, trackRow.height)

                                    x: dragX - root.kfSize / 2
                                    y: dragY - root.kfSize / 2
                                    width:  root.kfSize
                                    height: root.kfSize
                                    z: 3

                                    Canvas {
                                        id: kfDiamond
                                        anchors.fill: parent
                                        onPaint: {
                                            var ctx = getContext("2d");
                                            ctx.clearRect(0, 0, width, height);
                                            var cx = width  / 2;
                                            var cy = height / 2;
                                            var r  = Math.min(cx, cy) - 0.5;
                                            ctx.beginPath();
                                            ctx.moveTo(cx,     cy - r);
                                            ctx.lineTo(cx + r, cy    );
                                            ctx.lineTo(cx,     cy + r);
                                            ctx.lineTo(cx - r, cy    );
                                            ctx.closePath();
                                            ctx.fillStyle   = kfItem.isSelected ? "#ffa020"
                                                            : Qt.rgba(0.35, 0.75, 1.0, 1.0);
                                            ctx.fill();
                                            ctx.strokeStyle = "white";
                                            ctx.lineWidth   = kfItem.isSelected ? 1.5 : 1.0;
                                            ctx.stroke();
                                        }
                                        Connections {
                                            target: kfItem
                                            function onIsSelectedChanged() { kfDiamond.requestPaint(); }
                                        }
                                    }

                                    ToolTip.visible: kfMA.containsMouse && !kfMA.pressed
                                    ToolTip.text:    Math.round(kf.timeMs) + " ms  ?=" + kf.alpha.toFixed(2)
                                    ToolTip.delay:   500

                                    MouseArea {
                                        id: kfMA
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                                        cursorShape: Qt.SizeAllCursor

                                        // Snap threshold for alpha (values within this range of 0 or 1 snap)
                                        readonly property real alphaSnapThreshold: 0.08

                                        property real origTrackX: 0
                                        property real origTrackY: 0
                                        property real origDragX: 0
                                        property real origDragY: 0

                                        onPressed: function(mouse) {
                                            if (mouse.button === Qt.LeftButton) {
                                                root.selectedKf = { layerIdx: trackRow.layerIdx, kfIdx: kfItem.kfIndex };
                                                var globalPos = kfMA.mapToGlobal(mouse.x, mouse.y);
                                                var trackPos  = trackRow.mapFromGlobal(globalPos.x, globalPos.y);
                                                origTrackX = trackPos.x;
                                                origTrackY = trackPos.y;
                                                origDragX  = kfItem.dragX;
                                                origDragY  = kfItem.dragY;
                                                root.draggingKf = true;
                                                root.dragLiveMs = root.clampMs(root.xToMs(kfItem.dragX));
                                                root.dragLiveAlpha = root.yToAlpha(kfItem.dragY, trackRow.height);
                                                mouse.accepted = true;
                                            }
                                        }
                                        onPositionChanged: function(mouse) {
                                            if (pressed && (mouse.buttons & Qt.LeftButton)) {
                                                var globalPos = kfMA.mapToGlobal(mouse.x, mouse.y);
                                                var trackPos  = trackRow.mapFromGlobal(globalPos.x, globalPos.y);
                                                kfItem.dragX = origDragX + (trackPos.x - origTrackX);
                                                kfItem.dragY = origDragY + (trackPos.y - origTrackY);
                                                root.dragLiveMs = root.clampMs(root.xToMs(kfItem.dragX));
                                                var liveAlpha = root.yToAlpha(kfItem.dragY, trackRow.height);
                                                if (liveAlpha <= alphaSnapThreshold) liveAlpha = 0.0;
                                                else if (liveAlpha >= 1.0 - alphaSnapThreshold) liveAlpha = 1.0;
                                                root.dragLiveAlpha = liveAlpha;
                                            }
                                        }
                                        onReleased: function(mouse) {
                                            if (mouse.button === Qt.LeftButton && root.slideModel) {
                                                var newMs    = root.clampMs(root.xToMs(kfItem.dragX));
                                                var newAlpha = root.yToAlpha(kfItem.dragY, trackRow.height);
                                                // Snap alpha to 0 or 1 when near edges
                                                if (newAlpha <= alphaSnapThreshold) newAlpha = 0.0;
                                                else if (newAlpha >= 1.0 - alphaSnapThreshold) newAlpha = 1.0;
                                                var d = root.selectedKfData();
                                                root.slideModel.updateKeyframe(
                                                    trackRow.layerIdx, kfItem.kfIndex,
                                                    newMs, newAlpha,
                                                    d ? d.hasRotate    : false,
                                                    d ? d.rotateX      : 0, d ? d.rotateY    : 0, d ? d.rotateZ    : 0,
                                                    d ? d.hasTranslate : false,
                                                    d ? d.translateX   : 0, d ? d.translateY : 0, d ? d.translateZ : 0);
                                                trackRow.reload();
                                                // Force inspector refresh by nulling first
                                                root.selectedKf = null;
                                                var kfs = root.slideModel.getKeyframes(trackRow.layerIdx);
                                                for (var i = 0; i < kfs.length; i++) {
                                                    if (kfs[i].timeMs === newMs) {
                                                        root.selectedKf = { layerIdx: trackRow.layerIdx, kfIdx: i };
                                                        break;
                                                    }
                                                }
                                                root.inspectorRevision++;
                                                // Stop live drag after model and inspector are updated
                                                root.draggingKf = false;
                                            }
                                        }
                                        onClicked: function(mouse) {
                                            if (mouse.button === Qt.RightButton && root.slideModel) {
                                                root.slideModel.removeKeyframe(trackRow.layerIdx, kfItem.kfIndex);
                                                trackRow.reload();
                                                if (root.selectedKf
                                                        && root.selectedKf.layerIdx === trackRow.layerIdx
                                                        && root.selectedKf.kfIdx    === kfItem.kfIndex) {
                                                    root.selectedKf = null;
                                                }
                                            }
                                        }
                                    }
                                }
                            } // Repeater (keyframes)
                        } // trackRow
                    } // Repeater (layers / keyframesRepeater)
                } // Column

                // =============================================================
                // Outro divider — draggable vertical line marking intro/outro split
                // =============================================================
                Item {
                    id: outroDividerItem
                    visible: root.outroStartMs > 0 && root.outroStartMs < root.duration
                    x: root.msToX(root.outroStartMs) - 4
                    y: 0
                    width: 10
                    height: trackArea.contentHeight
                    z: 19

                    // Amber line spanning full content height
                    Rectangle {
                        x: (parent.width - 2) / 2
                        y: 0
                        width: 2
                        height: parent.height
                        color: "#fa0"
                        opacity: 0.80
                    }

                    // Label tag at top
                    Rectangle {
                        x: (parent.width - 2) / 2 + 2
                        y: root.rulerHeight - 14
                        width: outroLabel.implicitWidth + 4
                        height: 14
                        color: "#fa0"
                        radius: 2

                        Text {
                            id: outroLabel
                            anchors.centerIn: parent
                            text: qsTr("OUT")
                            font.pointSize: 7
                            font.bold: true
                            color: "#000"
                        }
                    }

                    MouseArea {
                        id: outroDividerMA
                        anchors.fill: parent
                        cursorShape: Qt.SplitHCursor

                        property real pressContentX: 0
                        property int  pressMs:       0

                        onPressed: function(mouse) {
                            pressContentX = trackArea.contentX + (outroDividerItem.x + mouse.x);
                            pressMs       = root.outroStartMs;
                        }
                        onPositionChanged: function(mouse) {
                            if (pressed && root.slideModel) {
                                var currentContentX = trackArea.contentX + (outroDividerItem.x + mouse.x);
                                var newMs = root.clampMs(root.xToMs(currentContentX - pressContentX) + pressMs);
                                root.slideModel.timelineOutroStart = newMs;
                            }
                        }
                    }
                }

                // =============================================================
                // Playhead  (inside Flickable, full content height)
                // =============================================================
                Item {
                    id: playheadItem
                    x: root.msToX(root.playheadMs) - 1
                    y: 0
                    width: 14
                    height: trackArea.contentHeight
                    z: 20

                    Canvas {
                        id: playheadTriangle
                        x: 0; y: 0
                        width: parent.width
                        height: root.rulerHeight
                        onPaint: {
                            var ctx = getContext("2d");
                            ctx.clearRect(0, 0, width, height);
                            ctx.fillStyle = "#f44";
                            ctx.beginPath();
                            ctx.moveTo(width / 2 - 0.5, 0);
                            ctx.lineTo(width, height * 0.55);
                            ctx.lineTo(0,     height * 0.55);
                            ctx.closePath();
                            ctx.fill();
                        }
                        Connections {
                            target: root
                            function onPlayheadMsChanged() { playheadTriangle.requestPaint(); }
                        }
                    }

                    Rectangle {
                        x: (parent.width - 2) / 2
                        y: root.rulerHeight
                        width: 2
                        height: parent.height - root.rulerHeight
                        color: "#f44"
                        opacity: 0.85
                    }

                    MouseArea {
                        id: playheadMA
                        anchors.fill: parent
                        cursorShape: Qt.SizeHorCursor

                        property real pressContentX: 0
                        property int  pressMs:       0

                        onPressed: function(mouse) {
                            pressContentX = trackArea.contentX + (playheadItem.x + mouse.x);
                            pressMs       = root.playheadMs;
                        }
                        onPositionChanged: function(mouse) {
                            if (pressed) {
                                var currentContentX = trackArea.contentX + (playheadItem.x + mouse.x);
                                root.playheadMs = root.clampMs(root.xToMs(currentContentX - pressContentX) + pressMs);
                            }
                        }
                    }
                }

            } // Flickable
        } // mainArea
    } // ColumnLayout
}
