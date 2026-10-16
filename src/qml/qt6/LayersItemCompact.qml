/*
 * SPDX-FileCopyrightText:
 * 2021-2026 Erik Sunden <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

ItemDelegate {
    id: root

    property string iconName: "edit-select-all-layers"
    property int iconWidth: 0
    property string rowNumber: (index + 1).toString()

    // Read by the master Front/Back buttons in Layers.qml through layersView.currentItem.
    readonly property int layerHierarchy: model.hierarchy
    readonly property bool layerIsReference: model.isReference
    readonly property bool layerIsLocked: model.locked && app.slides.selected.layersCanBeLocked

    // Drag state for visual feedback
    property bool dragging: false
    property real originalY: 0

    // transient insertion indicator instance (created on drag start)
    property var insertionIndicator: null

    function layersNumText() {
        const rowNumber = pad(root.rowNumber, layersView.count.toString().length) + ". ";
        return rowNumber;
    }
    function pad(number, length) {
        while (number.length < length)
            number = "0" + number;
        return number;
    }
    function subText() {
        return baseSubText();
    }
    function baseSubText() {
        if(model.type === "Audio")
            return model.type;
        else if(model.type === "Control")
            return model.type + " - " + model.filepath;
        else if(model.type === "TCP") {
            var commands = app.tcpControlManager.commands;
            var servers = app.tcpControlManager.servers;
            return model.type + " - " + app.tcpControlManager.commandDescription(model.filepath);
        }
        else if(model.type === "REST")
            return model.type + " - " + model.filepath;
        else if(model.type === "MultiVideo")
            return model.type + " - " + model.filepath.split(/[\\/]/).pop();
        else
            return model.type + model.page + " - " + model.stereoVideo + " " + model.gridToMapOn;
    }
    function statusToolTipText(status) {
        if (model.type === "TCP") {
            if (status === 2) return qsTr("TCP command accepted by transport; device execution is not acknowledged.");
            if (status === 1) return qsTr("TCP command is being dispatched.");
            if (status === 0) return qsTr("TCP command rejected: check the command and server connection.");
            return qsTr("TCP command has not been triggered.");
        }
        if (status === 2)
            return qsTr("Layer status: Loaded and visible.");
        if (status === 1)
            return qsTr("Layer status: Loaded but not visible.");
        if (status === 0)
            return qsTr("Layer status: Not Loaded.");
        return qsTr("Layer status: Not available.");
    }

    font.pointSize: 9
    highlighted: layersView.currentIndex === index
    implicitWidth: ListView.view.width
    padding: 0

    Menu {
        id: copyLayerMenu
        MenuItem { 
            action: actions.layerCopyAction
            visible: actions.layerCopyAction.enabled
        }
        MenuItem { 
            action: actions.layerPastePropertiesAction
            visible: actions.layerPastePropertiesAction.enabled
        }
    }

    background: Rectangle {
        anchors.fill: parent
        color: {
            if (highlighted) {
                return Qt.alpha(Kirigami.Theme.highlightColor, 0.6);
            }
            if (hovered) {
                return Qt.alpha(Kirigami.Theme.hoverColor, 0.4);
            }
            if (down) {
                return Qt.alpha(Kirigami.Theme.positiveBackgroundColor, 0.8);
            }
            if (model.isReference) {
                return Kirigami.Theme.alternateBackgroundColor;
            }
            return Kirigami.Theme.backgroundColor;
        }

        // Marks rows that reference a layer owned by an earlier slide.
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 3
            color: Kirigami.Theme.linkColor
            visible: model.isReference
        }
    }
    contentItem: Rectangle {
        color: "transparent"
        implicitHeight: 50
        implicitWidth: root.width

        MouseArea {
            id: layerIC_MA
            implicitHeight: parent.height
            implicitWidth: parent.width
            acceptedButtons: Qt.RightButton
            onClicked: {
                copyLayerMenu.popup();
                if(app.slides.selected.layersEnabled){
                    app.slides.selected.layerToCopy = index;
                }
            }

            Kirigami.IconTitleSubtitle {
                id: its

                anchors.bottomMargin: 15
                anchors.fill: parent
                color: root.hovered || root.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                icon.color: color
                icon.name: iconName
                icon.width: iconWidth
                implicitHeight: 50
                selected: root.down
                subtitle: subText()
                opacity: model.isReference ? 0.85 : 1.0
                title: (layersView.currentIndex === index ? layersNumText() : layersNumText() + model.title)
            }
            TextInput {
                id: slideTitleField

                anchors.left: parent.left
                anchors.leftMargin: layersView.count  > 9 ? 19 : 12
                anchors.top: parent.top
                anchors.topMargin: 3
                color: Kirigami.Theme.textColor
                font.pointSize: 9
                maximumLength: 30
                text: model.title
                visible: layersView.currentIndex === index

                onAccepted: {
                    layerView.layerItem.layerTitle = slideTitleField.text;
                }
            }
            Button  {
                flat: true
                visible: app.slides.selected.layersCanBeLocked && !model.isReference
                hoverEnabled: true
                anchors.leftMargin: 93
                anchors.bottomMargin: -3
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                icon.name: model.locked ? "object-locked" : "object-unlocked"
                icon.color: model.locked ? "darkred" : "gray"
                icon.width: 20
                icon.height: 20

                onClicked: {
                    if(app.slides.selected.layersEnabled){
                        if(model.locked) {
                            app.slides.selected.unlockLayer(index);
                        }
                        else {
                            app.slides.selected.lockLayer(index)
                        }
                    }
                    if(layersView.currentIndex === index) {
                        layersView.currentIndex = -1;
                    }
                    layersView.currentIndex = index;
                    app.slides.updateSelectedSlide();
                }

                ToolTip {
                    text: model.locked ? qsTr("Layer is locked. Click to unlock it for editing.") : qsTr("Layer is unlocked. Click to lock it against editing.")
                }
            }
            Label {
                // Master slide only: toggles whether the layer is rendered behind or above all slides.
                visible: app.slides.selectedSlideIdx === -1 && !model.isReference
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: 2
                anchors.rightMargin: 16
                font.pointSize: 7
                color: model.hierarchy === 1 ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                opacity: hierarchyMarkMA.containsMouse ? 1.0 : 0.7
                text: model.hierarchy === 1 ? qsTr("Front") : qsTr("Back")

                MouseArea {
                    id: hierarchyMarkMA
                    anchors.fill: parent
                    anchors.margins: -2
                    hoverEnabled: true
                    enabled: !root.layerIsLocked
                    cursorShape: Qt.PointingHandCursor

                    onClicked: {
                        if (app.slides.selected.layersEnabled) {
                            app.slides.selected.setLayerHierarchy(index, model.hierarchy === 1 ? 0 : 1);
                        }
                    }
                }

                ToolTip.visible: hierarchyMarkMA.containsMouse
                ToolTip.text: model.hierarchy === 1
                              ? qsTr("Front: rendered above the layers of all slides (below the foreground image). Click to make it a back layer.")
                              : qsTr("Back: rendered behind the main video and the layers of all slides. Click to make it a front layer.")
            }
            Rectangle {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: 5
                width: 10
                height: 10
                radius: 5
                visible: model.type !== "Control"
                color: (model.status === 2 ? "lime" : model.status === 1 ? "orange" : model.status === 0 ? "crimson" : "black")

                MouseArea {
                    id: layerStatusMA
                    anchors.fill: parent
                    hoverEnabled: true
                }

                ToolTip.visible: layerStatusMA.containsMouse
                ToolTip.text: statusToolTipText(model.status)
            }
            Kirigami.Icon {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: 2
                width: 14
                height: 14
                source: "media-playback-start"
                visible: model.type === "Control"
                color: Kirigami.Theme.textColor
            }
            Item {
                anchors.bottom: parent.bottom
                anchors.right: its.right
                implicitHeight: 25
                implicitWidth: 100
                visible: !visibilitySlider.visible && model.type !== "Control" && model.type !== "REST" && model.type !== "TCP"

                ToolTip.visible: visibilityPreviewMA.containsMouse
                ToolTip.text: qsTr("Layer visibility: %1%. Select the layer to adjust its opacity.").arg(model.visibility)

                MouseArea {
                    id: visibilityPreviewMA
                    anchors.fill: parent
                    hoverEnabled: true
                }

                Rectangle {
                    color: Kirigami.Theme.highlightColor
                    height: parent.height
                    radius: 0
                    width: model.visibility * 0.01 * parent.width
                }

                Label {
                    anchors.centerIn: parent
                    anchors.fill: parent
                    topPadding: 5
                    horizontalAlignment: Text.AlignHCenter
                    text: model.visibility + "%"
                    color:"#fff"
                    font.pointSize: 9
                    layer.enabled: true
                    layer.effect: DropShadow {
                        color: "#111"
                        radius: 5
                        samples: 17
                        spread: 0.3
                        verticalOffset: 1
                    }
                }
            }
            VisibilitySlider {
                id: visibilitySlider

                // Model refreshes must not write back while the selected layer changes.
                property bool syncingValue: false
                // Follows changes made elsewhere, e.g. fades driven by the media visibility.
                property int modelVisibility: model.visibility
                onModelVisibilityChanged: {
                    if (visible && !pressed && Math.round(value) !== modelVisibility)
                        syncValue(modelVisibility);
                }
                function syncValue(newValue) {
                    syncingValue = true;
                    value = newValue;
                    syncingValue = false;
                }

                anchors.bottom: parent.bottom
                anchors.right: its.right
                visible: layersView.currentIndex === index && model.type !== "Control" && model.type !== "REST" && model.type !== "TCP"
                implicitWidth: 100
                overlayLabel: qsTr("")

                ToolTip {
                    text: qsTr("Layer visibility: drag to adjust this layer's opacity from 0% to 100%.")
                }

                onVisibleChanged: {
                    if (visible) {
                        visibilitySlider.syncValue(app.slides.selected.layerVisibility(index));
                    }
                }
                onValueChanged: {
                    if (!syncingValue && layerView.layerItem.layerIdx === index
                            && (!layersView.enabled || visibilitySlider.enabled)) {
                        if (visibilitySlider.visible && (value.toFixed(0) !== layerView.layerItem.layerVisibility)) {
                            // A layer with entrance/exit effects scrubs through them instead of fading.
                            if (layerView.layerItem.layerSelectedSubLayer > 0
                                    || !app.slides.selected.scrubLayerAnimation(index, Math.round(value)))
                                layerView.layerItem.layerVisibility = value.toFixed(0);
                            app.slides.needsSync = true;
                        }
                    }
                }
                Component.onCompleted: {
                    visibilitySlider.syncValue(app.slides.selected.layerVisibility(index));
                }
            }
            Item {
                anchors.bottom: parent.bottom
                anchors.right: its.right
                implicitHeight: 20
                implicitWidth: 100
                visible: layersView.currentIndex !== index && model.type !== "Control" && model.type !== "REST" && model.type !== "TCP"

                ToolTip.visible: visibilityTextMA.containsMouse
                ToolTip.text: qsTr("Layer visibility: %1%. Select the layer to adjust its opacity.").arg(model.visibility)

                MouseArea {
                    id: visibilityTextMA
                    anchors.fill: parent
                    hoverEnabled: true
                }

                Label {
                    anchors.fill: parent
                    horizontalAlignment: Text.AlignHCenter
                    text: model.visibility + "%"
                }
            }
        }

        // Drag handle area: small left-side handle to avoid blocking interactive controls.
        Rectangle {
            id: dragHandleRect
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 12
            color: "transparent"

            Row {
                anchors.left: parent.left
                anchors.bottom: parent.bottom

                Kirigami.Icon {
                    source: "drag-surface"
                    width: 12
                    height: 12
                    opacity: 0.6
                    visible: !root.dragging
                }

                // Reference indicator: the layer is owned by an earlier slide
                Kirigami.Icon {
                    source: "link"
                    width: 12
                    height: 12
                    color: Kirigami.Theme.linkColor
                    visible: model.isReference

                    MouseArea {
                        id: referenceIndicatorMA
                        anchors.fill: parent
                        hoverEnabled: true
                    }

                    ToolTip.visible: referenceIndicatorMA.containsMouse
                    ToolTip.text: qsTr("Reference to a layer of %1, kept visible in this slide by its slide visibility. Edits change the original layer; the position in this list and the animations are set per slide.").arg(model.referenceSource)
                }

                // Effects indicator: yellow entrance/exit, blue post process, purple both
                Kirigami.Icon {
                    id: effectsIndicatorIcon

                    // Bumped when the slide's effects change, so the bindings below re-evaluate.
                    property int revision: 0
                    readonly property bool hasEntranceExit: {
                        var _r = revision;
                        return app.slides.selected.layerHasEffects(index);
                    }
                    readonly property bool hasPostEffects: {
                        var _r = revision;
                        return app.slides.selected.layerHasPostEffects(index);
                    }

                    source: "rating"
                    width: 12
                    height: 12
                    color: hasEntranceExit && hasPostEffects ? "mediumpurple"
                         : hasEntranceExit ? "yellow" : "dodgerblue"
                    visible: hasEntranceExit || hasPostEffects

                    Connections {
                        target: app.slides.selected
                        function onTimelineChanged() { effectsIndicatorIcon.revision++; }
                        function onLayersModelChanged() { effectsIndicatorIcon.revision++; }
                        function onPostEffectsChanged() { effectsIndicatorIcon.revision++; }
                    }

                    MouseArea {
                        id: effectsIndicatorMA
                        anchors.fill: parent
                        hoverEnabled: true
                    }

                    ToolTip.visible: effectsIndicatorMA.containsMouse
                    ToolTip.text: hasEntranceExit && hasPostEffects
                        ? qsTr("The layer has entrance/exit effects and post process effects.")
                        : hasEntranceExit
                          ? qsTr("The layer has entrance/exit effects.")
                          : qsTr("The layer has post process effects.")
                }

                // NDI output indicator, shown when not in default state (NDI enabled)
                Kirigami.Icon {
                    id: ndiIndicatorIcon
                    source: "cloud-upload"
                    width: 12
                    height: 12
                    color: "lime"
                    visible: model.ndiOutput

                    MouseArea {
                        id: ndiIndicatorMA
                        anchors.fill: parent
                        hoverEnabled: true
                    }

                    ToolTip.visible: ndiIndicatorMA.containsMouse
                    ToolTip.text: model.ndiOutput && model.nodeStreamOutput
                        ? qsTr("The layer is streamed to the nodes as an NDI source on the master.")
                        : qsTr("NDI output is enabled for this layer on the master.")
                }

                // Node stream indicator, shown when the layer texture is streamed to the nodes
                Kirigami.Icon {
                    source: "network-wired"
                    width: 12
                    height: 12
                    color: "lime"
                    visible: model.nodeStreamOutput

                    MouseArea {
                        id: nodeStreamIndicatorMA
                        anchors.fill: parent
                        hoverEnabled: true
                    }

                    ToolTip.visible: nodeStreamIndicatorMA.containsMouse
                    ToolTip.text: qsTr("The layer texture is streamed from the master to the nodes.")
                }

                // Sync indicator, shown when not in default state (not synced to the nodes)
                Kirigami.Icon {
                    id: syncIndicatorIcon
                    source: model.existOnMasterOnly ? "network-disconnect" : "network-connect"
                    width: 12
                    height: 12
                    color: "orange"
                    visible: model.existOnMasterOnly

                    MouseArea {
                        id: syncIndicatorMA
                        anchors.fill: parent
                        hoverEnabled: true
                    }

                    ToolTip.visible: syncIndicatorMA.containsMouse
                    ToolTip.text: qsTr("Layer exists on the master only, it is not synced to the nodes.")
                }
            }

            // Template for transient insertion indicator (created at drag start)
            Component {
                id: insertionLineComp
                Rectangle {
                    color: Kirigami.Theme.highlightColor
                    height: 4
                    width: parent ? parent.width : layersView.width
                    radius: 2
                    opacity: 0.95
                    z: 10000
                }
            }

            MouseArea {
                id: dragHandleMA
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton
                drag.target: root
                drag.axis: Drag.YAxis
                cursorShape: Qt.DragCopyCursor

                // Keep the item inside the view content while dragging
                onPressed: {
                    root.dragging = true;
                    root.originalY = root.y;
                    root.z = 9999; // bring to front while dragging
                    root.opacity = 0.85;
                    // ensure current index follows selection while dragging
                    layersView.currentIndex = index;

                    // Constrain the drag to the visible content area
                    drag.minimumY = 0;
                    drag.maximumY = Math.max(0, layersView.contentHeight);

                    // create insertion indicator (shared transient visual between rows)
                    if (layersView && layersView.contentItem && !root.insertionIndicator) {
                        // create indicator initially at current center position
                        root.insertionIndicator = insertionLineComp.createObject(layersView.contentItem, { x: 0, y: root.y + root.height/2 - 2, width: layersView.width });
                    }
                }
                onPositionChanged: {
                    // Optional: auto-scroll the list when dragging near edges
                    var edgeThreshold = 20;
                    var localY = root.y;
                    if (localY < layersView.contentY + edgeThreshold && layersView.contentY > 0) {
                        layersView.contentY = Math.max(0, layersView.contentY - 8);
                    }
                    var bottomEdge = layersView.contentHeight - edgeThreshold - root.height;
                    if (localY > bottomEdge && (layersView.contentY < layersView.contentHeight - layersView.height)) {
                        layersView.contentY = Math.min(layersView.contentHeight - layersView.height, layersView.contentY + 8);
                    }

                    // update insertion indicator position to show where the item will land
                    if (root.insertionIndicator) {
                        // compute index based on delegate center
                        var centerY = root.y + root.height / 2;
                        var destIndex = Math.floor(centerY / root.height);
                        if (destIndex < 0) destIndex = 0;
                        if (destIndex > layersView.count - 1) destIndex = layersView.count - 1;

                        var halfIndicator = root.insertionIndicator.height / 2;
                        var indicatorY;

                        // When dragging down, show the indicator at the BOTTOM of the target item
                        // (i.e. after that item). When dragging up, show it at the TOP of the target item.
                        if (root.y > root.originalY) {
                            // place after destIndex
                            indicatorY = (destIndex + 1) * root.height - halfIndicator;
                        } else {
                            // place before destIndex
                            indicatorY = destIndex * root.height - halfIndicator;
                        }

                        // clamp indicator to content area
                        if (indicatorY < 0) indicatorY = 0;
                        var maxIndicatorY = layersView.contentHeight - root.insertionIndicator.height;
                        if (indicatorY > maxIndicatorY) indicatorY = maxIndicatorY;

                        root.insertionIndicator.y = indicatorY;
                        // ensure width follows view width (in case of resize while dragging)
                        root.insertionIndicator.width = layersView.width;
                    }
                }
                onCanceled: {
                    // cleanup transient indicator
                    if (root.insertionIndicator) {
                        root.insertionIndicator.destroy();
                        root.insertionIndicator = null;
                    }
                    root.dragging = false;
                    root.z = 0;
                    root.opacity = 1.0;
                    // restore original position
                    root.y = root.originalY;
                }
                onReleased: {
                    root.dragging = false;
                    root.z = 0;
                    root.opacity = 1.0;

                    // Compute destination index based on the delegate center and drag direction.
                    var centerY = root.y + root.height / 2;
                    var destIndex = Math.floor(centerY / root.height);
                    if (destIndex < 0) destIndex = 0;
                    if (destIndex > layersView.count - 1) destIndex = layersView.count - 1;

                    // If dragged down, insert AFTER destIndex; if up, insert BEFORE destIndex.
                    var finalDest = (root.y > root.originalY) ? (destIndex) : destIndex;

                    // clamp finalDest to valid range (last valid index is count-1)
                    if (finalDest < 0) finalDest = 0;
                    if (finalDest > layersView.count - 1) finalDest = layersView.count - 1;

                    if(app.slides.selected.layersEnabled){
                        app.slides.selected.moveLayer(index, finalDest);
                        layersView.currentIndex = finalDest;
                    }

                    // remove transient indicator after move
                    if (root.insertionIndicator) {
                        root.insertionIndicator.destroy();
                        root.insertionIndicator = null;
                    }
                }
            }
        }
    }

    onHighlightedChanged: {
        if (highlighted) {
            visibilitySlider.syncValue(model.visibility);
        }
    }

    onClicked: {
        layersView.currentIndex = index;
        slideTitleField.text = model.title;
        layerView.layerItem.layerIdx = index;
        if(app.slides.selected.layersEnabled){
            app.slides.selected.layerToCopy = index;
        }
    }
    onDoubleClicked: {
        layerView.layerItem.layerIdx = index;
        if (model.type === "Control" || model.type === "REST" || model.type === "TCP") {
            layerView.layerItem.start();
        } else if (layerView.layerItem.layerVisibility === 100 && !visibility_fade_out_animation.running) {
            // The layer's exit effects replace the plain fade when it has any.
            if (!playLayerAnimation(false))
                visibility_fade_out_animation.start();
        } else if (layerView.layerItem.layerVisibility < 100 && !visibility_fade_in_animation.running) {
            if (!playLayerAnimation(true))
                visibility_fade_in_animation.start();
        }
    }

    // Plays the layer's entrance (show) or exit effects instead of fading it, when it has them.
    function playLayerAnimation(show) {
        if (layerView.layerItem.layerSelectedSubLayer > 0)
            return false;
        if (!app.slides.selected.playLayerAnimation(index, show))
            return false;
        app.slides.needsSync = true;
        return true;
    }

    PropertyAnimation {
        id: visibility_fade_out_animation

        duration: PlaybackSettings.fadeDuration
        property: "value"
        target: visibilitySlider
        to: 0

        onFinished: {
            layersView.enabled = true;
        }
        onStarted: {
            layersView.enabled = false;
        }
    }
    PropertyAnimation {
        id: visibility_fade_in_animation

        duration: PlaybackSettings.fadeDuration * ((100 - visibilitySlider.value) / 100)
        property: "value"
        target: visibilitySlider
        to: 100

        onFinished: {
            layersView.enabled = true;
        }
        onStarted: {
            layersView.enabled = false;
        }
    }
    Connections {
        function onLayerChanged() {
            if (visibilitySlider.value !== app.slides.selected.layerVisibility(index))
                visibilitySlider.syncValue(app.slides.selected.layerVisibility(index));
        }
        function onLayerValueChanged() {
            if (visibilitySlider.value !== app.slides.selected.layerVisibility(index))
                visibilitySlider.syncValue(app.slides.selected.layerVisibility(index));
        }

        target: layerView.layerItem
    }
    Connections {
        function onLayerAnimationFinished(layerIdx) {
            if (layerIdx === index)
                visibilitySlider.syncValue(app.slides.selected.layerVisibility(index));
        }
        // While the layer animates, the slider follows the animation instead of the opacity.
        function onLayerAnimationProgressChanged(layerIdx, progress) {
            if (layerIdx === index)
                visibilitySlider.syncValue(progress);
        }

        target: app.slides.selected
    }
}
