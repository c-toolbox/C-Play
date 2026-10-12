/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

// PowerPoint-style animation pane: entrance (intro) and exit (outro) effects per slide.
Window {
    id: root

    property int slideIndex: -1
    property var slideTimeline: null

    readonly property var slideModel: (slideIndex >= 0 && slideIndex < app.slides.numberOfSlides())
                                      ? app.slides.slide(slideIndex) : null

    // 0 = entrance (intro), 1 = exit (outro)
    property int phase: 0
    property var effects: []
    // Selected row in the effect list: { layerIdx, effectIdx } or null
    property var selected: null
    property int revision: 0

    readonly property color introColor: "#3fb950"
    readonly property color outroColor: "#f85149"
    readonly property color phaseColor: phase === 0 ? introColor : outroColor

    title: slideModel ? qsTr("Animations: %1. %2").arg(slideIndex + 1).arg(slideModel.layersName) : qsTr("Animations")
    width: 460
    height: 720
    minimumWidth: 380
    minimumHeight: 480
    color: Kirigami.Theme.backgroundColor
    flags: Qt.Window | Qt.WindowTitleHint | Qt.WindowCloseButtonHint | Qt.WindowMinMaxButtonsHint

    function reload() {
        if (!slideModel) {
            effects = [];
            return;
        }
        effects = slideModel.getSlideEffects(phase);
        if (selected) {
            var found = false;
            for (var i = 0; i < effects.length; i++) {
                if (effects[i].layerIdx === selected.layerIdx && effects[i].effectIdx === selected.effectIdx) {
                    found = true;
                    break;
                }
            }
            if (!found)
                selected = null;
        }
        revision++;
    }

    function selectedEffect() {
        var _r = revision;
        if (!selected)
            return null;
        for (var i = 0; i < effects.length; i++) {
            if (effects[i].layerIdx === selected.layerIdx && effects[i].effectIdx === selected.effectIdx)
                return effects[i];
        }
        return null;
    }

    function selectedRow() {
        var _r = revision;
        if (!selected)
            return -1;
        for (var i = 0; i < effects.length; i++) {
            if (effects[i].layerIdx === selected.layerIdx && effects[i].effectIdx === selected.effectIdx)
                return i;
        }
        return -1;
    }

    function updateSelected(values) {
        if (!slideModel || !selected)
            return;
        slideModel.updateEffect(selected.layerIdx, selected.effectIdx, values);
    }

    function totalSpan() {
        var span = 1;
        for (var i = 0; i < effects.length; i++)
            span = Math.max(span, effects[i].endMs);
        return span;
    }

    function triggerText(trigger) {
        if (trigger === 0) return qsTr("On click");
        if (trigger === 1) return qsTr("With previous");
        return qsTr("After previous");
    }

    onPhaseChanged: {
        selected = null;
        reload();
    }
    onVisibleChanged: if (visible) reload()
    onSlideModelChanged: reload()

    Connections {
        target: root.slideModel
        function onTimelineChanged() { root.reload(); }
        function onLayersModelChanged() { root.reload(); }
    }

    // Shortcuts common in animation tools.
    Shortcut {
        sequence: StandardKey.Delete
        enabled: root.selected !== null
        onActivated: removeButton.clicked()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Kirigami.Units.smallSpacing
        spacing: Kirigami.Units.smallSpacing

        // -------------------------------------------------------------------
        // Entrance / exit switch
        // -------------------------------------------------------------------
        TabBar {
            id: phaseBar
            Layout.fillWidth: true
            currentIndex: root.phase
            onCurrentIndexChanged: root.phase = currentIndex

            TabButton {
                text: qsTr("Entrance (intro)")
                icon.name: "go-next"
                icon.color: root.introColor
            }
            TabButton {
                text: qsTr("Exit (outro)")
                icon.name: "go-previous"
                icon.color: root.outroColor
            }
        }

        // -------------------------------------------------------------------
        // Add effect
        // -------------------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true

            Label { text: qsTr("Layer:") }
            ComboBox {
                id: layerCombo
                Layout.fillWidth: true
                textRole: "title"
                valueRole: "layerIdx"
                model: {
                    var _r = root.revision;
                    var list = [];
                    if (!root.slideModel)
                        return list;
                    for (var i = 0; i < root.slideModel.rowCount(); i++) {
                        if (!root.slideModel.layerSupportsAnimation(i))
                            continue;
                        var title = (i + 1) + ". " + root.slideModel.layerTitle(i);
                        if (root.slideModel.isReference(i))
                            title += " " + qsTr("(ref. %1)").arg(root.slideModel.referenceSource(i));
                        list.push({ title: title, layerIdx: i });
                    }
                    return list;
                }
            }
            Button {
                id: addButton
                text: qsTr("Add effect")
                icon.name: "list-add"
                enabled: layerCombo.count > 0
                onClicked: gallery.open()
            }
        }

        // -------------------------------------------------------------------
        // Effect list (animation pane)
        // -------------------------------------------------------------------
        Frame {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 160
            padding: 0

            ListView {
                id: effectList
                anchors.fill: parent
                clip: true
                model: root.effects
                currentIndex: root.selectedRow()
                ScrollBar.vertical: ScrollBar {}

                Label {
                    anchors.centerIn: parent
                    width: parent.width - 2 * Kirigami.Units.largeSpacing
                    visible: effectList.count === 0
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    opacity: 0.7
                    text: root.phase === 0
                          ? qsTr("No entrance effects. Pick a layer and press \"Add effect\" to animate how it appears.")
                          : qsTr("No exit effects. Exit effects play when the slide is left.")
                }

                delegate: ItemDelegate {
                    id: row
                    required property var modelData
                    required property int index

                    width: ListView.view.width
                    highlighted: ListView.isCurrentItem
                    onClicked: root.selected = { layerIdx: row.modelData.layerIdx, effectIdx: row.modelData.effectIdx }
                    onDoubleClicked: root.slideModel.previewEffect(row.modelData.layerIdx, row.modelData.effectIdx)

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.smallSpacing

                        // Click step badge (PowerPoint numbering), or a link icon for chained effects.
                        Rectangle {
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 1.3
                            Layout.preferredHeight: Kirigami.Units.gridUnit * 1.3
                            radius: width / 2
                            color: row.modelData.trigger === 0 && root.phase === 0 ? Kirigami.Theme.highlightColor : "transparent"
                            border.color: Kirigami.Theme.disabledTextColor
                            border.width: row.modelData.trigger === 0 && root.phase === 0 ? 0 : 1
                            Label {
                                anchors.centerIn: parent
                                font.pointSize: 8
                                color: row.modelData.trigger === 0 && root.phase === 0 ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                                text: row.modelData.trigger === 0 && root.phase === 0 ? row.modelData.step
                                      : (row.modelData.trigger === 1 ? "\u2261" : "\u21b3")
                            }
                            ToolTip.visible: badgeMA.containsMouse
                            ToolTip.text: root.triggerText(row.modelData.trigger)
                            MouseArea { id: badgeMA; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
                        }

                        Kirigami.Icon {
                            source: "rating"
                            color: root.phaseColor
                            isMask: true
                            Layout.preferredWidth: Kirigami.Units.iconSizes.small
                            Layout.preferredHeight: Kirigami.Units.iconSizes.small
                        }

                        ColumnLayout {
                            spacing: 0
                            Layout.fillWidth: true
                            Label {
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                text: row.modelData.layerTitle
                            }
                            Label {
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                font.pointSize: 8
                                opacity: 0.7
                                text: row.modelData.presetName + " \u2022 " + (row.modelData.durationMs / 1000).toFixed(2) + " s"
                                      + (row.modelData.delayMs > 0 ? " \u2022 " + qsTr("delay %1 s").arg((row.modelData.delayMs / 1000).toFixed(2)) : "")
                            }
                        }

                        // Timing bar (Gantt view of the phase)
                        Item {
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 7
                            Layout.preferredHeight: Kirigami.Units.gridUnit * 0.7
                            Rectangle {
                                anchors.fill: parent
                                radius: 2
                                color: Qt.rgba(0.5, 0.5, 0.5, 0.15)
                            }
                            Rectangle {
                                id: timingBar
                                readonly property real span: root.totalSpan()
                                x: parent.width * row.modelData.startMs / timingBar.span
                                width: Math.max(2, parent.width * (row.modelData.endMs - row.modelData.startMs) / timingBar.span)
                                height: parent.height
                                radius: 2
                                color: root.phaseColor
                                opacity: 0.85
                            }
                        }
                    }
                }
            }
        }

        // List actions
        RowLayout {
            Layout.fillWidth: true
            ToolButton {
                icon.name: "go-up"
                enabled: root.selectedRow() > 0
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Play earlier")
                onClicked: root.slideModel.moveEffect(root.selected.layerIdx, root.selected.effectIdx, root.selectedRow() - 1)
            }
            ToolButton {
                icon.name: "go-down"
                enabled: root.selectedRow() >= 0 && root.selectedRow() < root.effects.length - 1
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Play later")
                onClicked: root.slideModel.moveEffect(root.selected.layerIdx, root.selected.effectIdx, root.selectedRow() + 1)
            }
            ToolButton {
                id: removeButton
                icon.name: "edit-delete"
                enabled: root.selected !== null
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Remove effect (Del)")
                onClicked: {
                    root.slideModel.removeEffect(root.selected.layerIdx, root.selected.effectIdx);
                    root.selected = null;
                }
            }
            ToolButton {
                icon.name: "document-export"
                enabled: {
                    var e = root.selectedEffect();
                    return e !== null && (e.preset === 1 || e.preset === 4);
                }
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Convert to keyframes (Fade and Zoom)")
                onClicked: {
                    root.slideModel.convertEffectToKeyframes(root.selected.layerIdx, root.selected.effectIdx);
                    root.selected = null;
                }
            }
            Item { Layout.fillWidth: true }
            Button {
                text: qsTr("Play")
                icon.name: "media-playback-start"
                enabled: root.selected !== null
                onClicked: root.slideModel.previewEffect(root.selected.layerIdx, root.selected.effectIdx)
            }
            Button {
                text: root.phase === 0 ? qsTr("Play intro") : qsTr("Play outro")
                icon.name: "media-seek-forward"
                enabled: root.effects.length > 0
                onClicked: {
                    if (root.phase === 0)
                        app.slides.startTimelineFrom(root.slideIndex, 0);
                    else
                        app.slides.startTimelineOutro(root.slideIndex);
                }
            }
        }

        // -------------------------------------------------------------------
        // Effect options
        // -------------------------------------------------------------------
        Kirigami.Separator { Layout.fillWidth: true }

        GridLayout {
            id: options
            Layout.fillWidth: true
            columns: 2
            columnSpacing: Kirigami.Units.largeSpacing
            enabled: root.selected !== null
            opacity: enabled ? 1 : 0.5

            readonly property var e: root.selectedEffect()
            readonly property bool isMove: e !== null && root.slideModel !== null && root.slideModel.effectPresetIsMove(e.preset)
            readonly property var startPose: isMove ? root.slideModel.poseBeforeEffect(e.layerIdx, e.effectIdx) : null

            Label { text: qsTr("Effect:") }
            ComboBox {
                Layout.fillWidth: true
                model: root.slideModel ? root.slideModel.effectPresetNames() : []
                currentIndex: options.e ? options.e.preset : -1
                onActivated: root.updateSelected({ preset: currentIndex })
            }

            Label { text: qsTr("Start:") }
            ComboBox {
                Layout.fillWidth: true
                model: root.phase === 0 ? [qsTr("On click"), qsTr("With previous"), qsTr("After previous")]
                                        : [qsTr("After previous (auto)"), qsTr("With previous"), qsTr("After previous")]
                currentIndex: options.e ? options.e.trigger : -1
                onActivated: root.updateSelected({ trigger: currentIndex })
            }

            Label { text: qsTr("Duration:") }
            SpinBox {
                Layout.fillWidth: true
                from: 1; to: 600000; stepSize: 50
                editable: true
                value: options.e ? options.e.durationMs : 500
                textFromValue: function(v) { return (v / 1000).toFixed(2) + " s"; }
                valueFromText: function(t) { var p = parseFloat(t); return isNaN(p) ? value : Math.round(p * 1000); }
                onValueModified: root.updateSelected({ durationMs: value })
            }

            Label { text: qsTr("Delay:") }
            SpinBox {
                Layout.fillWidth: true
                from: 0; to: 600000; stepSize: 50
                editable: true
                value: options.e ? options.e.delayMs : 0
                textFromValue: function(v) { return (v / 1000).toFixed(2) + " s"; }
                valueFromText: function(t) { var p = parseFloat(t); return isNaN(p) ? value : Math.round(p * 1000); }
                onValueModified: root.updateSelected({ delayMs: value })
            }

            Label { text: qsTr("Easing:") }
            RowLayout {
                Layout.fillWidth: true
                ComboBox {
                    id: easingCombo
                    Layout.fillWidth: true
                    model: root.slideModel ? root.slideModel.easingNames() : []
                    currentIndex: options.e ? options.e.easing : -1
                    onActivated: root.updateSelected({ easing: currentIndex })
                }
                EasingCurveThumb {
                    slideModel: root.slideModel
                    easing: easingCombo.currentIndex
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 2.5
                    Layout.preferredHeight: Kirigami.Units.gridUnit * 2
                    strokeColor: root.phaseColor
                }
            }

            Label {
                text: qsTr("Direction:")
                visible: options.e !== null && root.slideModel && root.slideModel.effectPresetUsesDirection(options.e.preset)
            }
            ComboBox {
                Layout.fillWidth: true
                visible: options.e !== null && root.slideModel && root.slideModel.effectPresetUsesDirection(options.e.preset)
                model: root.slideModel ? root.slideModel.effectDirectionNames() : []
                currentIndex: options.e ? options.e.direction : -1
                onActivated: root.updateSelected({ direction: currentIndex })
            }

            Label {
                text: qsTr("Softness:")
                visible: options.e !== null && root.slideModel && root.slideModel.effectPresetUsesFeather(options.e.preset)
            }
            Slider {
                Layout.fillWidth: true
                visible: options.e !== null && root.slideModel && root.slideModel.effectPresetUsesFeather(options.e.preset)
                from: 0; to: 1; stepSize: 0.01
                value: options.e ? options.e.feather : 0.1
                onMoved: root.updateSelected({ feather: value })
                ToolTip.visible: hovered || pressed
                ToolTip.text: Math.round(value * 100) + " %"
            }

            Label {
                text: qsTr("Amount:")
                visible: options.e !== null && [2, 3, 4, 5].indexOf(options.e.preset) >= 0
            }
            Slider {
                Layout.fillWidth: true
                visible: options.e !== null && [2, 3, 4, 5].indexOf(options.e.preset) >= 0
                from: 0.1; to: 3; stepSize: 0.05
                value: options.e ? options.e.amount : 1
                onMoved: root.updateSelected({ amount: value })
                ToolTip.visible: hovered || pressed
                ToolTip.text: value.toFixed(2) + "\u00d7"
            }

            // Move: plane pose at the start and at the end of the effect.
            Label {
                text: qsTr("Move:")
                visible: options.isMove
                Layout.alignment: Qt.AlignTop
            }
            GridLayout {
                Layout.fillWidth: true
                visible: options.isMove
                columns: 3
                columnSpacing: Kirigami.Units.smallSpacing
                rowSpacing: 2

                RowLayout {
                    Layout.columnSpan: 3
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing
                    Item { Layout.preferredWidth: Kirigami.Units.gridUnit * 4 }
                    Label { text: qsTr("Start"); font.bold: true; Layout.fillWidth: true; Layout.preferredWidth: 1 }
                    Label { text: qsTr("End"); font.bold: true; Layout.fillWidth: true; Layout.preferredWidth: 1 }
                }

                Repeater {
                    model: [
                        { key: "Azimuth", label: qsTr("Azimuth"), unit: "\u00b0" },
                        { key: "Elevation", label: qsTr("Elevation"), unit: "\u00b0" },
                        { key: "Roll", label: qsTr("Roll"), unit: "\u00b0" },
                        { key: "Distance", label: qsTr("Distance"), unit: " cm" },
                        { key: "Horizontal", label: qsTr("Horizontal"), unit: " cm" },
                        { key: "Vertical", label: qsTr("Vertical"), unit: " cm" }
                    ]

                    delegate: RowLayout {
                        id: poseRow
                        required property var modelData
                        Layout.columnSpan: 3
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        function pinStartWith(values) {
                            if (!options.e.hasStart) {
                                // Editing an automatic start pins all of it.
                                var p = options.startPose;
                                values.hasStart = true;
                                values.startAzimuth = p.azimuth; values.startElevation = p.elevation; values.startRoll = p.roll;
                                values.startDistance = p.distance; values.startHorizontal = p.horizontal; values.startVertical = p.vertical;
                            }
                            return values;
                        }

                        Label {
                            text: poseRow.modelData.label + ":"
                            font.pointSize: 8
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 4
                        }
                        SpinBox {
                            id: startSpin
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            from: -1000000; to: 1000000; stepSize: 10
                            editable: true
                            value: options.startPose ? Math.round(options.startPose[poseRow.modelData.key.toLowerCase()] * 10) : 0
                            textFromValue: function(v) { return (v / 10.0).toFixed(1) + poseRow.modelData.unit; }
                            valueFromText: function(t) { var p = parseFloat(t); return isNaN(p) ? value : Math.round(p * 10); }
                            onValueModified: {
                                var values = poseRow.pinStartWith({});
                                values["start" + poseRow.modelData.key] = value / 10.0;
                                root.updateSelected(values);
                            }
                        }
                        SpinBox {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            from: -1000000; to: 1000000; stepSize: 10
                            editable: true
                            value: options.e && options.isMove ? Math.round(options.e["target" + poseRow.modelData.key] * 10) : 0
                            textFromValue: function(v) { return (v / 10.0).toFixed(1) + poseRow.modelData.unit; }
                            valueFromText: function(t) { var p = parseFloat(t); return isNaN(p) ? value : Math.round(p * 10); }
                            onValueModified: {
                                var values = {};
                                values["target" + poseRow.modelData.key] = value / 10.0;
                                root.updateSelected(values);
                            }
                        }
                    }
                }

                CheckBox {
                    Layout.columnSpan: 3
                    text: qsTr("Automatic start (where earlier moves left the layer)")
                    checked: options.e !== null && !options.e.hasStart
                    onToggled: {
                        if (checked) {
                            root.updateSelected({ hasStart: false });
                            return;
                        }
                        var p = options.startPose;
                        root.updateSelected({ hasStart: true, startAzimuth: p.azimuth, startElevation: p.elevation, startRoll: p.roll,
                                              startDistance: p.distance, startHorizontal: p.horizontal, startVertical: p.vertical });
                    }
                }

                RowLayout {
                    Layout.columnSpan: 3
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing
                    Item { Layout.preferredWidth: Kirigami.Units.gridUnit * 4 }
                    Button {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        text: qsTr("Set from layer")
                        icon.name: "document-save"
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Use where the layer is shown now as the start position")
                        onClicked: root.slideModel.captureMovePose(root.selected.layerIdx, root.selected.effectIdx, false)
                    }
                    Button {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        text: qsTr("Set from layer")
                        icon.name: "document-save"
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Use where the layer is shown now as the end position")
                        onClicked: root.slideModel.captureMovePose(root.selected.layerIdx, root.selected.effectIdx, true)
                    }
                }

                Label {
                    Layout.columnSpan: 3
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pointSize: 8
                    opacity: 0.7
                    text: qsTr("Select the layer in the Layers panel and drag it in the 3D view with the Ctrl, Alt or Shift drag operation, then set the start or end. Dragging a layer shown at an animated position only changes that position, not the layer's own settings.")
                }
            }
        }

        Kirigami.Separator { Layout.fillWidth: true }

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pointSize: 8
                opacity: 0.7
                text: root.phase === 0
                      ? qsTr("\"On click\" steps wait for Next (keyboard, remote or /slide_next) before playing. Moves on a layer kept from an earlier slide continue from where that slide left it.")
                      : qsTr("Exit effects play automatically when the slide is left.")
            }
            Button {
                text: qsTr("Timeline")
                icon.name: "chronometer"
                visible: root.slideTimeline !== null
                onClicked: {
                    root.slideTimeline.show();
                    root.slideTimeline.raise();
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // Effect gallery
    // -----------------------------------------------------------------------
    Popup {
        id: gallery
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(root.width - 2 * Kirigami.Units.largeSpacing, Kirigami.Units.gridUnit * 24)
        modal: true
        focus: true
        padding: Kirigami.Units.largeSpacing

        readonly property var categories: [qsTr("Basic"), qsTr("Wipe & mask"), qsTr("Motion"), qsTr("Zoom & spin"), qsTr("Move (keeps visibility, carries to next slides)")]

        contentItem: ColumnLayout {
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Heading {
                level: 3
                text: root.phase === 0 ? qsTr("Entrance effects") : qsTr("Exit effects")
                color: root.phaseColor
            }

            Repeater {
                model: gallery.categories

                ColumnLayout {
                    id: categoryColumn
                    required property int index
                    required property string modelData
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        text: categoryColumn.modelData
                        font.bold: true
                        opacity: 0.8
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        Repeater {
                            model: {
                                var names = root.slideModel ? root.slideModel.effectPresetNames() : [];
                                var list = [];
                                for (var i = 0; i < names.length; i++) {
                                    if (root.slideModel.effectPresetCategory(i) === categoryColumn.index)
                                        list.push({ preset: i, name: names[i] });
                                }
                                return list;
                            }

                            EffectPreviewTile {
                                required property var modelData
                                preset: modelData.preset
                                name: modelData.name
                                intro: root.phase === 0
                                accentColor: root.phaseColor
                                onChosen: {
                                    var layerIdx = layerCombo.currentValue;
                                    var idx = root.slideModel.addEffect(layerIdx, root.phase, preset);
                                    gallery.close();
                                    if (idx >= 0)
                                        root.selected = { layerIdx: layerIdx, effectIdx: idx };
                                    root.reload();
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
