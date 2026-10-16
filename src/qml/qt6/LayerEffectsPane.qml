/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

// Effects pane: PowerPoint-style entrance (intro) and exit (outro) effects per slide, plus
// post process effects (chroma key, color correction, ...) always applied to a layer's image.
// Entrance and exit effects also play on slides without a timeline, together with the regular slide fade.
Window {
    id: root

    // Slide to animate, -1 for the master slide (whose layers only animate when shown or hidden on their own).
    property int slideIndex: -2
    property var slideTimeline: null
    // Row of the slide's layer to edit only (as from the layer view), -1 for all layers.
    property int layerFilter: -1
    // LayerQtItem showing the layer (set from the layer view): previews post effects and picks colors.
    property var layerView: null

    readonly property bool isMaster: slideIndex === -1
    readonly property var slideModel: isMaster ? app.slides.masterSlide()
                                      : ((slideIndex >= 0 && slideIndex < app.slides.numberOfSlides()) ? app.slides.slide(slideIndex) : null)
    readonly property bool layerMode: layerFilter >= 0
    // The master slide is never triggered, and the layer view previews one layer:
    // both play the layer on its own instead of the slide's intro or outro.
    readonly property bool layerPlayback: isMaster || layerMode
    readonly property bool layerAnimatable: {
        var _r = revision;
        return !layerMode || (slideModel !== null && layerFilter < slideModel.rowCount()
                              && slideModel.layerSupportsAnimation(layerFilter));
    }

    // 0 = entrance, 1 = exit, 2 = post process
    property int tab: 0
    readonly property bool postTab: tab === 2
    // 0 = entrance (intro), 1 = exit (outro)
    readonly property int phase: tab === 1 ? 1 : 0
    // Effects of the phase in slide playback order, and the ones shown (filtered by layer).
    property var allEffects: []
    property var effects: []
    // Selected row in the effect list: { layerIdx, effectIdx } or null
    property var selected: null
    property int revision: 0

    readonly property color introColor: "#3fb950"
    readonly property color outroColor: "#f85149"
    readonly property color postColor: "#58a6ff"
    readonly property color phaseColor: phase === 0 ? introColor : outroColor

    // Post process: the layer edited, its effects (see LayersModel::getPostEffects) and the
    // color being picked from the layer view ("" when not picking).
    readonly property int postLayerIdx: layerMode ? layerFilter : (postLayerCombo.currentIndex >= 0 ? postLayerCombo.currentValue : -1)
    property var fx: ({})
    property string pickKey: ""
    readonly property bool canPick: layerView !== null && layerMode && layerView.layerIdx === layerFilter

    readonly property var postSections: [
        {
            key: "chromaKey", title: qsTr("Chroma key"), colorKey: "keyColor",
            description: qsTr("Removes a background color, such as a green or blue screen. Pick the color from the image for the best result, then raise the similarity until the background is gone."),
            sliders: [
                { key: "keySimilarity", label: qsTr("Similarity"), from: 0, to: 1, step: 0.005, fmt: "pct" },
                { key: "keySmoothness", label: qsTr("Smoothness"), from: 0, to: 1, step: 0.005, fmt: "pct" },
                { key: "keySpill", label: qsTr("Spill reduction"), from: 0, to: 1, step: 0.005, fmt: "pct" }
            ]
        },
        {
            key: "lumaKey", title: qsTr("Luma key"),
            description: qsTr("Removes dark areas, such as graphics on black, or bright areas when inverted."),
            sliders: [
                { key: "lumaThreshold", label: qsTr("Threshold"), from: 0, to: 1, step: 0.005, fmt: "pct" },
                { key: "lumaSmoothness", label: qsTr("Smoothness"), from: 0, to: 0.5, step: 0.005, fmt: "pct" }
            ]
        },
        {
            key: "colorCorrection", title: qsTr("Color correction"),
            description: qsTr("Adjusts brightness, contrast, saturation, gamma and hue. A saturation of 0 % gives black and white."),
            sliders: [
                { key: "brightness", label: qsTr("Brightness"), from: -1, to: 1, step: 0.01, fmt: "signedPct" },
                { key: "contrast", label: qsTr("Contrast"), from: 0, to: 3, step: 0.01, fmt: "x" },
                { key: "saturation", label: qsTr("Saturation"), from: 0, to: 3, step: 0.01, fmt: "pct" },
                { key: "gamma", label: qsTr("Gamma"), from: 0.1, to: 4, step: 0.01, fmt: "x" },
                { key: "hue", label: qsTr("Hue"), from: -180, to: 180, step: 1, fmt: "deg" }
            ]
        },
        {
            key: "invert", title: qsTr("Invert colors"),
            description: qsTr("Turns the image into its negative."),
            sliders: []
        },
        {
            key: "tint", title: qsTr("Tint"), colorKey: "tintColor",
            description: qsTr("Colors the image with one color, such as sepia or a monochrome look."),
            sliders: [
                { key: "tintAmount", label: qsTr("Amount"), from: 0, to: 1, step: 0.01, fmt: "pct" }
            ]
        },
        {
            key: "blur", title: qsTr("Blur"),
            description: qsTr("Softens the image, e.g. to put a background out of focus."),
            sliders: [
                { key: "blurRadius", label: qsTr("Radius"), from: 0, to: 64, step: 0.5, fmt: "px" }
            ]
        },
        {
            key: "sharpen", title: qsTr("Sharpen"),
            description: qsTr("Enhances edges and fine details."),
            sliders: [
                { key: "sharpenAmount", label: qsTr("Amount"), from: 0, to: 3, step: 0.01, fmt: "x" }
            ]
        },
        {
            key: "pixelate", title: qsTr("Pixelate"),
            description: qsTr("Shows the image as large blocks, e.g. to anonymize it."),
            sliders: [
                { key: "pixelSize", label: qsTr("Block size"), from: 1, to: 256, step: 1, fmt: "px" }
            ]
        },
        {
            key: "vignette", title: qsTr("Vignette / edge fade"),
            description: qsTr("Darkens the edges of the layer, or fades them out to blend the layer into what is behind it."),
            sliders: [
                { key: "vignetteRadius", label: qsTr("Radius"), from: 0, to: 1.5, step: 0.01, fmt: "pct" },
                { key: "vignetteSoftness", label: qsTr("Softness"), from: 0, to: 1.5, step: 0.01, fmt: "pct" },
                { key: "vignetteAmount", label: qsTr("Amount"), from: 0, to: 1, step: 0.01, fmt: "pct" }
            ]
        }
    ]

    title: {
        var _r = revision;
        if (!slideModel)
            return qsTr("Effects");
        if (layerMode && isMaster)
            return qsTr("Effects: %1 (master slide)").arg(slideModel.layerTitle(layerFilter));
        if (layerMode)
            return qsTr("Effects: %1 (slide %2. %3)").arg(slideModel.layerTitle(layerFilter)).arg(slideIndex + 1).arg(slideModel.layersName);
        if (isMaster)
            return qsTr("Effects: master slide");
        return qsTr("Effects: %1. %2").arg(slideIndex + 1).arg(slideModel.layersName);
    }
    width: 460
    height: 720
    minimumWidth: 380
    minimumHeight: 480
    color: Kirigami.Theme.backgroundColor
    flags: Qt.Window | Qt.WindowTitleHint | Qt.WindowCloseButtonHint | Qt.WindowMinMaxButtonsHint

    function reload() {
        if (!slideModel) {
            allEffects = [];
            effects = [];
            revision++;
            return;
        }
        allEffects = slideModel.getSlideEffects(phase);
        effects = layerMode ? allEffects.filter(function(e) { return e.layerIdx === layerFilter; }) : allEffects;
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

    // Position of the shown row in the slide's playback order (what moveEffect expects).
    function slidePosition(row) {
        if (row < 0 || row >= effects.length)
            return -1;
        for (var i = 0; i < allEffects.length; i++) {
            if (allEffects[i].layerIdx === effects[row].layerIdx && allEffects[i].effectIdx === effects[row].effectIdx)
                return i;
        }
        return -1;
    }

    // Moves the selected effect to where the shown row targetRow plays.
    function moveSelectedTo(targetRow) {
        var pos = slidePosition(targetRow);
        if (pos >= 0)
            slideModel.moveEffect(selected.layerIdx, selected.effectIdx, pos);
    }

    function targetLayerIdx() {
        return layerMode ? layerFilter : layerCombo.currentValue;
    }

    // Layers with an image, for the layer pickers: { title, layerIdx }.
    function imageLayers() {
        var _r = revision;
        var list = [];
        if (!slideModel)
            return list;
        for (var i = 0; i < slideModel.rowCount(); i++) {
            if (!slideModel.layerSupportsAnimation(i))
                continue;
            var title = (i + 1) + ". " + slideModel.layerTitle(i);
            if (slideModel.isReference(i))
                title += " " + qsTr("(ref. %1)").arg(slideModel.referenceSource(i));
            list.push({ title: title, layerIdx: i });
        }
        return list;
    }

    // Previews one effect: on its own layer in layer playback, otherwise in the slide timeline.
    function previewEffect(layerIdx, effectIdx) {
        if (layerPlayback)
            slideModel.playLayerAnimation(layerIdx, phase === 0);
        else
            slideModel.previewEffect(layerIdx, effectIdx);
    }

    function updateSelected(values) {
        if (!slideModel || !selected)
            return;
        slideModel.updateEffect(selected.layerIdx, selected.effectIdx, values);
    }

    function totalSpan() {
        var span = 1;
        for (var i = 0; i < allEffects.length; i++)
            span = Math.max(span, allEffects[i].endMs);
        return span;
    }

    function triggerText(trigger) {
        if (trigger === 0) return qsTr("On click");
        if (trigger === 1) return qsTr("With previous");
        return qsTr("After previous");
    }

    // ---- Post process ------------------------------------------------------

    function reloadFx() {
        fx = (slideModel && postLayerIdx >= 0) ? slideModel.getPostEffects(postLayerIdx) : ({});
    }

    function updateFx(key, value) {
        if (!slideModel || postLayerIdx < 0)
            return;
        var values = {};
        values[key] = value;
        slideModel.updatePostEffects(postLayerIdx, values);
    }

    function hasPostEffects() {
        for (var i = 0; i < postSections.length; i++) {
            if (fx[postSections[i].key] === true)
                return true;
        }
        return false;
    }

    function formatValue(fmt, v) {
        if (fmt === "pct") return Math.round(v * 100) + " %";
        if (fmt === "signedPct") return (v > 0 ? "+" : "") + Math.round(v * 100) + " %";
        if (fmt === "x") return v.toFixed(2) + "×";
        if (fmt === "deg") return Math.round(v) + "°";
        if (fmt === "px") return (v % 1 === 0 ? v.toFixed(0) : v.toFixed(1)) + " px";
        return v.toFixed(2);
    }

    // Starts (or stops) picking the color named key by clicking in the layer view.
    function togglePick(key) {
        if (!canPick)
            return;
        if (pickKey === key) {
            layerView.colorPickActive = false;
            return;
        }
        pickKey = key;
        layerView.colorPickActive = true;
        layerView.showPostEffects = true;
    }

    function cancelPick() {
        if (layerView && pickKey !== "")
            layerView.colorPickActive = false;
        pickKey = "";
    }

    onPhaseChanged: {
        selected = null;
        reload();
    }
    onVisibleChanged: {
        if (visible) {
            reload();
            reloadFx();
        }
        else {
            cancelPick();
        }
    }
    onSlideModelChanged: {
        selected = null;
        reload();
        reloadFx();
    }
    onLayerFilterChanged: {
        selected = null;
        reload();
        reloadFx();
    }
    onPostLayerIdxChanged: {
        cancelPick();
        reloadFx();
    }
    onTabChanged: if (!postTab) cancelPick()

    Connections {
        target: root.slideModel
        function onTimelineChanged() { root.reload(); }
        function onLayersModelChanged() {
            root.reload();
            root.reloadFx();
        }
        function onPostEffectsChanged(layerIdx) {
            if (layerIdx === root.postLayerIdx)
                root.reloadFx();
        }
    }

    Connections {
        target: root.layerView
        function onColorPicked(color) {
            if (root.pickKey !== "")
                root.updateFx(root.pickKey, color.toString());
        }
        function onColorPickActiveChanged() {
            if (!root.layerView.colorPickActive)
                root.pickKey = "";
        }
    }

    // Shortcuts common in animation tools.
    Shortcut {
        sequence: StandardKey.Delete
        enabled: !root.postTab && root.selected !== null
        onActivated: removeButton.clicked()
    }

    ColorDialog {
        id: colorDialog

        property string colorKey: ""

        function openFor(key) {
            colorKey = key;
            selectedColor = root.fx[key] !== undefined ? root.fx[key] : "white";
            open();
        }

        title: colorKey === "keyColor" ? qsTr("Key color") : qsTr("Tint color")
        onAccepted: root.updateFx(colorKey, selectedColor.toString())
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Kirigami.Units.smallSpacing
        spacing: Kirigami.Units.smallSpacing

        // -------------------------------------------------------------------
        // Entrance / exit / post process switch
        // -------------------------------------------------------------------
        TabBar {
            id: phaseBar
            Layout.fillWidth: true
            currentIndex: root.tab
            onCurrentIndexChanged: root.tab = currentIndex

            TabButton {
                text: qsTr("Entrance")
                icon.name: "go-next"
                icon.color: root.introColor
            }
            TabButton {
                text: qsTr("Exit")
                icon.name: "go-previous"
                icon.color: root.outroColor
            }
            TabButton {
                text: qsTr("Post Process")
                icon.name: "color-management"
                icon.color: root.postColor
            }
        }

        Label {
            Layout.fillWidth: true
            visible: !root.slideModel || !root.layerAnimatable
            wrapMode: Text.WordWrap
            opacity: 0.7
            text: !root.slideModel ? qsTr("Select a slide to add effects to its layers.")
                                   : qsTr("This layer has no image to add effects to.")
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.postTab ? 1 : 0

            // ===============================================================
            // Entrance and exit effects
            // ===============================================================
            ColumnLayout {
                spacing: Kirigami.Units.smallSpacing

                // -----------------------------------------------------------
                // Add effect
                // -----------------------------------------------------------
                RowLayout {
                    Layout.fillWidth: true
                    enabled: root.slideModel !== null && root.layerAnimatable

                    Label {
                        text: qsTr("Layer:")
                        visible: !root.layerMode
                    }
                    ComboBox {
                        id: layerCombo
                        visible: !root.layerMode
                        Layout.fillWidth: true
                        textRole: "title"
                        valueRole: "layerIdx"
                        model: root.imageLayers()
                    }
                    Item {
                        visible: root.layerMode
                        Layout.fillWidth: true
                    }
                    Button {
                        id: addButton
                        text: qsTr("Add effect")
                        icon.name: "list-add"
                        enabled: root.layerMode || layerCombo.count > 0
                        onClicked: gallery.open()
                    }
                }

                // -----------------------------------------------------------
                // Effect list (animation pane)
                // -----------------------------------------------------------
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
                                  ? (root.layerMode ? qsTr("No entrance effects. Press \"Add effect\" to animate how the layer appears.")
                                                    : qsTr("No entrance effects. Pick a layer and press \"Add effect\" to animate how it appears."))
                                  : qsTr("No exit effects. Exit effects play when the slide is left.")
                        }

                        delegate: ItemDelegate {
                            id: row
                            required property var modelData
                            required property int index

                            width: ListView.view.width
                            highlighted: ListView.isCurrentItem
                            onClicked: root.selected = { layerIdx: row.modelData.layerIdx, effectIdx: row.modelData.effectIdx }
                            onDoubleClicked: root.previewEffect(row.modelData.layerIdx, row.modelData.effectIdx)

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
                                              : (row.modelData.trigger === 1 ? "≡" : "↳")
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
                                        text: row.modelData.presetName + " • " + (row.modelData.durationMs / 1000).toFixed(2) + " s"
                                              + (row.modelData.delayMs > 0 ? " • " + qsTr("delay %1 s").arg((row.modelData.delayMs / 1000).toFixed(2)) : "")
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
                        onClicked: root.moveSelectedTo(root.selectedRow() - 1)
                    }
                    ToolButton {
                        icon.name: "go-down"
                        enabled: root.selectedRow() >= 0 && root.selectedRow() < root.effects.length - 1
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Play later")
                        onClicked: root.moveSelectedTo(root.selectedRow() + 1)
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
                        onClicked: root.previewEffect(root.selected.layerIdx, root.selected.effectIdx)
                    }
                    Button {
                        text: root.layerPlayback ? (root.phase === 0 ? qsTr("Play entrance") : qsTr("Play exit"))
                                                 : (root.phase === 0 ? qsTr("Play intro") : qsTr("Play outro"))
                        icon.name: "media-seek-forward"
                        enabled: root.layerPlayback ? root.effects.length > 0 : root.allEffects.length > 0
                        onClicked: {
                            if (root.layerPlayback)
                                root.slideModel.playLayerAnimation(root.layerMode ? root.layerFilter : root.effects[0].layerIdx, root.phase === 0);
                            else if (root.phase === 0)
                                app.slides.startTimelineFrom(root.slideIndex, 0);
                            else
                                app.slides.startTimelineOutro(root.slideIndex);
                        }
                    }
                }

                // -----------------------------------------------------------
                // Effect options
                // -----------------------------------------------------------
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
                        ToolTip.text: value.toFixed(2) + "×"
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
                                { key: "Azimuth", label: qsTr("Azimuth"), unit: "°" },
                                { key: "Elevation", label: qsTr("Elevation"), unit: "°" },
                                { key: "Roll", label: qsTr("Roll"), unit: "°" },
                                { key: "Distance", label: qsTr("Distance"), unit: " cm" },
                                { key: "Horizontal", label: qsTr("Horizontal"), unit: options.e && options.e.planeCircular ? "°" : " cm" },
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
                        text: root.isMaster
                              ? qsTr("Master slide layers play their entrance when they are shown and their exit when they are hidden (double-click in the layer list, or the show/hide buttons of the layer view).")
                              : (root.phase === 0
                                 ? qsTr("\"On click\" steps wait for Next (keyboard, remote or /slide_next) before playing. Moves on a layer kept from an earlier slide continue from where that slide left it.")
                                 : qsTr("Exit effects play automatically when the slide is left."))
                                + (root.slideModel && !root.slideModel.hasTimeline
                                   ? " " + qsTr("The slide needs no timeline: layers without effects keep the regular slide fade.") : "")
                                + " " + qsTr("Showing or hiding the layer on its own (double-click in the layer list) also plays its entrance or exit.")
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

            // ===============================================================
            // Post process effects
            // ===============================================================
            ColumnLayout {
                spacing: Kirigami.Units.smallSpacing
                enabled: root.slideModel !== null && root.layerAnimatable

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        text: qsTr("Layer:")
                        visible: !root.layerMode
                    }
                    ComboBox {
                        id: postLayerCombo
                        visible: !root.layerMode
                        Layout.fillWidth: true
                        textRole: "title"
                        valueRole: "layerIdx"
                        model: root.imageLayers()
                    }
                    CheckBox {
                        visible: root.layerView !== null
                        Layout.fillWidth: true
                        text: qsTr("Preview in layer view")
                        checked: root.layerView !== null && root.layerView.showPostEffects
                        onToggled: root.layerView.showPostEffects = checked
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Shows the layer view with the effects applied, over a checkerboard where they make the image transparent.")
                    }
                    Item {
                        visible: root.layerMode && root.layerView === null
                        Layout.fillWidth: true
                    }
                    Button {
                        text: qsTr("Remove all")
                        icon.name: "edit-clear-all"
                        enabled: root.hasPostEffects()
                        onClicked: {
                            root.cancelPick();
                            root.slideModel.clearPostEffects(root.postLayerIdx);
                        }
                    }
                }

                ScrollView {
                    id: postScroll
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: availableWidth
                    clip: true

                    ColumnLayout {
                        width: postScroll.availableWidth
                        spacing: Kirigami.Units.smallSpacing

                        Repeater {
                            model: root.postSections

                            delegate: Frame {
                                id: section

                                required property var modelData
                                readonly property bool on: root.fx[modelData.key] === true

                                Layout.fillWidth: true
                                padding: Kirigami.Units.smallSpacing

                                ColumnLayout {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    spacing: 2

                                    RowLayout {
                                        Layout.fillWidth: true

                                        CheckBox {
                                            Layout.fillWidth: true
                                            text: section.modelData.title
                                            font.bold: section.on
                                            checked: section.on
                                            onToggled: root.updateFx(section.modelData.key, checked)
                                        }
                                        ToolButton {
                                            visible: section.on && (section.modelData.sliders.length > 0 || section.modelData.colorKey !== undefined)
                                            icon.name: "edit-reset"
                                            ToolTip.visible: hovered
                                            ToolTip.text: qsTr("Reset to defaults")
                                            onClicked: root.slideModel.resetPostEffect(root.postLayerIdx, section.modelData.key)
                                        }
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        Layout.leftMargin: Kirigami.Units.gridUnit * 1.5
                                        visible: section.on
                                        wrapMode: Text.WordWrap
                                        font.pointSize: 8
                                        opacity: 0.7
                                        text: section.modelData.description
                                    }

                                    // Color (key or tint): swatch opening a color dialog, hex value and picking from the image.
                                    RowLayout {
                                        id: colorRow

                                        readonly property string colorKey: section.modelData.colorKey !== undefined ? section.modelData.colorKey : ""
                                        readonly property bool picking: colorKey !== "" && root.pickKey === colorKey

                                        Layout.fillWidth: true
                                        Layout.leftMargin: Kirigami.Units.gridUnit * 1.5
                                        visible: section.on && colorKey !== ""
                                        spacing: Kirigami.Units.smallSpacing

                                        Label {
                                            text: colorRow.colorKey === "keyColor" ? qsTr("Key color:") : qsTr("Color:")
                                            Layout.preferredWidth: Kirigami.Units.gridUnit * 6
                                        }
                                        Rectangle {
                                            Layout.preferredWidth: Kirigami.Units.gridUnit * 2
                                            Layout.preferredHeight: Kirigami.Units.gridUnit * 1.4
                                            radius: 3
                                            color: root.fx[colorRow.colorKey] !== undefined ? root.fx[colorRow.colorKey] : "transparent"
                                            border.color: Kirigami.Theme.textColor
                                            border.width: 1

                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: colorDialog.openFor(colorRow.colorKey)
                                            }
                                        }
                                        TextField {
                                            Layout.preferredWidth: Kirigami.Units.gridUnit * 5
                                            text: root.fx[colorRow.colorKey] !== undefined ? root.fx[colorRow.colorKey] : ""
                                            validator: RegularExpressionValidator { regularExpression: /#?[0-9a-fA-F]{6}/ }
                                            onEditingFinished: root.updateFx(colorRow.colorKey, text.charAt(0) === "#" ? text : "#" + text)
                                        }
                                        Button {
                                            Layout.fillWidth: true
                                            text: colorRow.picking ? qsTr("Click in the layer view…") : qsTr("Pick from image")
                                            icon.name: "color-picker"
                                            checkable: true
                                            checked: colorRow.picking
                                            enabled: root.canPick
                                            onClicked: root.togglePick(colorRow.colorKey)
                                            ToolTip.visible: hovered
                                            ToolTip.text: root.canPick
                                                          ? qsTr("Click or drag in the layer view to sample the color under the mouse (5×5 pixel average, before effects). Esc cancels.")
                                                          : qsTr("Open this pane from the layer view (Effects button) to pick the color from the layer's image.")
                                        }
                                    }

                                    // Common key colors
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.leftMargin: Kirigami.Units.gridUnit * 1.5 + Kirigami.Units.gridUnit * 6 + Kirigami.Units.smallSpacing
                                        visible: section.on && section.modelData.key === "chromaKey"
                                        Button {
                                            text: qsTr("Green")
                                            flat: true
                                            onClicked: root.updateFx("keyColor", "#00ff00")
                                        }
                                        Button {
                                            text: qsTr("Blue")
                                            flat: true
                                            onClicked: root.updateFx("keyColor", "#0000ff")
                                        }
                                        Button {
                                            text: qsTr("Magenta")
                                            flat: true
                                            onClicked: root.updateFx("keyColor", "#ff00ff")
                                        }
                                    }

                                    Repeater {
                                        model: section.modelData.sliders

                                        delegate: RowLayout {
                                            id: sliderRow

                                            required property var modelData

                                            Layout.fillWidth: true
                                            Layout.leftMargin: Kirigami.Units.gridUnit * 1.5
                                            visible: section.on
                                            spacing: Kirigami.Units.smallSpacing

                                            Label {
                                                text: sliderRow.modelData.label + ":"
                                                Layout.preferredWidth: Kirigami.Units.gridUnit * 6
                                            }
                                            Slider {
                                                id: fxSlider
                                                Layout.fillWidth: true
                                                from: sliderRow.modelData.from
                                                to: sliderRow.modelData.to
                                                stepSize: sliderRow.modelData.step
                                                value: root.fx[sliderRow.modelData.key] !== undefined ? root.fx[sliderRow.modelData.key] : from
                                                onMoved: root.updateFx(sliderRow.modelData.key, value)
                                            }
                                            Label {
                                                Layout.preferredWidth: Kirigami.Units.gridUnit * 3
                                                horizontalAlignment: Text.AlignRight
                                                text: root.formatValue(sliderRow.modelData.fmt, fxSlider.value)
                                            }
                                        }
                                    }

                                    CheckBox {
                                        Layout.leftMargin: Kirigami.Units.gridUnit * 1.5
                                        visible: section.on && section.modelData.key === "lumaKey"
                                        text: qsTr("Remove bright areas instead")
                                        checked: root.fx.lumaInvert === true
                                        onToggled: root.updateFx("lumaInvert", checked)
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.leftMargin: Kirigami.Units.gridUnit * 1.5
                                        visible: section.on && section.modelData.key === "vignette"
                                        Label {
                                            text: qsTr("Edges:")
                                            Layout.preferredWidth: Kirigami.Units.gridUnit * 6
                                        }
                                        ComboBox {
                                            Layout.fillWidth: true
                                            model: [qsTr("Darken"), qsTr("Fade to transparent")]
                                            currentIndex: root.fx.vignetteMode !== undefined ? root.fx.vignetteMode : 0
                                            onActivated: root.updateFx("vignetteMode", currentIndex)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Kirigami.Separator { Layout.fillWidth: true }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pointSize: 8
                    opacity: 0.7
                    text: qsTr("Post process effects belong to the layer: they are always applied to its image, in every slide and on every node, before its entrance and exit effects and opacity. Blur, sharpen and pixelate act first, the other effects follow in the order listed.")
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
                                    var layerIdx = root.targetLayerIdx();
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
