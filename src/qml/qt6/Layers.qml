/*
 * SPDX-FileCopyrightText:
 * 2021-2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

import "Components/PopupHelpers.js" as PopupHelpers

Rectangle {
    id: layersRoot

    property int bigFont: PlaylistSettings.bigFontFullscreen
    property alias layersScrollView: layersScrollView
    property alias layersView: layersView
    property string position: PlaylistSettings.position
    property int rowHeight: PlaylistSettings.rowHeight
    property alias scrollPositionTimer: scrollPositionTimer
    property var markedIndices: []
    // Row a Shift + click range starts from: the last row clicked without Shift.
    property int markAnchor: -1
    // Rows captured when the remove is requested, removed once the remove timer fires.
    property var layersToRemove: []
    property bool busyIndicator: false
    property bool shouldBeVisible: true

    // Tips shown at the bottom of the layers list. The rotation order is shuffled once on
    // startup (this component is created once per app start); the user advances to the next
    // tip with a button or hides the tips for this session, and no more tips are shown after
    // all of them have been displayed until next launch.
    // Display names for the modifier+drag layer operations (values 0-7), matching the options in
    // the Presentation settings page and presentationsettings.kcfg.
    readonly property var dragLayerActionNames: [
        qsTr("Elevation & Azimuth"),
        qsTr("Elevation"),
        qsTr("Azimuth"),
        qsTr("Horizontal & Vertical Move"),
        qsTr("Horizontal Move"),
        qsTr("Vertical Move"),
        qsTr("Resize plane size"),
        qsTr("Move plane distance")
    ]

    function dragLayerActionName(action) {
        return (action >= 0 && action < dragLayerActionNames.length) ? dragLayerActionNames[action] : "";
    }

    // The first tip reflects the operations currently configured for Ctrl/Alt/Shift+left-drag in
    // the Presentation settings, so it always tells the user what each modifier key does. The
    // binding re-evaluates whenever any of those settings changes.
    readonly property var allTips: [
        qsTr("In the 3D view, hold a modifier key and left-drag to manipulate the selected layer (Ctrl = %1, Alt = %2, Shift = %3).")
            .arg(dragLayerActionName(PresentationSettings.ctrlDragLayerAction))
            .arg(dragLayerActionName(PresentationSettings.altDragLayerAction))
            .arg(dragLayerActionName(PresentationSettings.shiftDragLayerAction)),
        qsTr("Right-click or use Ctrl+C to copy a layer, and Ctrl+V to paste a copy."),
        qsTr("You can drag-and-drop files directly from the file explorer.")
    ]
    property var tipOrder: []
    property int tipIndex: 0
    // Set when all tips have been shown or the user hides the tips - no more tips until next launch.
    property bool tipsFinished: false
    // Whether the tips row is currently displayed. It stays hidden while the layers panel is
    // opened and only becomes true after a short delay, so the tips pop up instead of being
    // visible right away.
    property bool tipsVisible: false
    // True while any loading is in progress (either for the slides or this layers panel).
    property bool isBusy: slides.busyIndicator || busyIndicator

    // (Re)start the countdown that reveals the tips. Hides any currently shown tips and waits a
    // fresh 5 seconds before showing them again, so that loading always delays the tips by a full
    // interval from when it finishes.
    function armTipTimer() {
        if (tipsFinished || !PresentationSettings.showTipsAtStartup)
            return;
        tipShowTimer.stop();
        tipsVisible = false;
        tipShowTimer.start();
    }

    // Start the countdown when the layers panel is opened (only if not already loading - in that
    // case the onIsBusyChanged handler below arms it once the loading has finished).
    onShouldBeVisibleChanged: {
        if (shouldBeVisible && !isBusy)
            armTipTimer();
    }

    // Whenever loading starts, halt the showing of the tips and restart the trigger; whenever it
    // ends, start a fresh countdown so that the tips appear 5 seconds after things settle down.
    onIsBusyChanged: {
        if (isBusy) {
            tipShowTimer.stop();
            tipsVisible = false;
        } else if (shouldBeVisible && !tipsFinished && PresentationSettings.showTipsAtStartup) {
            armTipTimer();
        }
    }

    function showNextTip() {
        if (tipIndex + 1 >= allTips.length)
            tipsFinished = true;   // last tip has been shown - no more tips until next launch
        else
            tipIndex += 1;
    }

    function shuffleTips() {
        if(PresentationSettings.showTipsAtStartup === false)
            return;
        var order = [];
        for (var i = 0; i < allTips.length; ++i)
            order.push(i);
        // Fisher-Yates shuffle - new random rotation order on every startup
        for (var j = order.length - 1; j > 0; --j) {
            var k = Math.floor(Math.random() * (j + 1));
            var tmp = order[j];
            order[j] = order[k];
            order[k] = tmp;
        }
        tipOrder = order;
    }

    Component.onCompleted: shuffleTips()

    visible: shouldBeVisible && !window.hideUI
    color: Kirigami.Theme.backgroundColor
    height: mpv.height
    state: "hidden"
    width: {
        const w = Kirigami.Units.gridUnit * 13.25;
        return w;
    }
    x: position !== "left" ? parent.width : -width
    y: 0
    z: position === "left" ? 41 : 40

    states: [
        State {
            name: "hidden"

            PropertyChanges {
                target: layersRoot
                x: position === "left" ? parent.width : -width
            }
            PropertyChanges {
                target: layersRoot
                shouldBeVisible: false
            }
        },
        State {
            name: "visible-without-partner"

            PropertyChanges {
                target: layersRoot
                x: position === "left" ? parent.width - layersRoot.width : 0
            }
            PropertyChanges {
                target: layersRoot
                shouldBeVisible: true
            }
        },
        State {
            name: "visible-with-partner"

            PropertyChanges {
                target: layersRoot
                x: position === "left" ? parent.width - layersRoot.width : layersRoot.width
            }
            PropertyChanges {
                target: layersRoot
                shouldBeVisible: true
            }
        }
    ]
    transitions: [
        Transition {
            from: "visible-without-partner"
            to: "hidden"

            SequentialAnimation {
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.InQuad
                    property: "x"
                    target: layersRoot
                }
                PropertyAction {
                    property: "shouldBeVisible"
                    target: layersRoot
                    value: false
                }
            }
        },
        Transition {
            from: "hidden"
            to: "visible-without-partner"

            SequentialAnimation {
                PropertyAction {
                    property: "shouldBeVisible"
                    target: layersRoot
                    value: true
                }
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.OutQuad
                    property: "x"
                    target: layersRoot
                }
            }
        },
        Transition {
            from: "visible-with-partner"
            to: "hidden"

            SequentialAnimation {
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.InQuad
                    property: "x"
                    target: layersRoot
                }
                PropertyAction {
                    property: "shouldBeVisible"
                    target: layersRoot
                    value: false
                }
            }
        },
        Transition {
            from: "hidden"
            to: "visible-with-partner"

            SequentialAnimation {
                PropertyAction {
                    property: "shouldBeVisible"
                    target: layersRoot
                    value: true
                }
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.OutQuad
                    property: "x"
                    target: layersRoot
                }
            }
        },
        Transition {
            from: "visible-without-partner"
            to: "visible-with-partner"

            SequentialAnimation {
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.OutQuad
                    property: "x"
                    target: layersRoot
                }
            }
        },
        Transition {
            from: "visible-with-partner"
            to: "visible-without-partner"

            SequentialAnimation {
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.OutQuad
                    property: "x"
                    target: layersRoot
                }
            }
        }
    ]

    function clearAllLayers() {
        clearAllLayersTimer.start();
    }

    function removeLayer() {
        removeLayerTimer.start();
    }

    // Keyboard shortcut path (removeSelected action): removes the marked layer under the same
    // conditions as the remove button, and only while the layers panel is shown.
    // Returns false when nothing could be removed.
    function removeSelectedLayer() {
        if (!shouldBeVisible || isBusy || !layerDeleteButton.enabled)
            return false;
        if (window.rowsToRemove(markedIndices, layersView.currentIndex, layersView.count).length === 0)
            return false;
        layerDeleteButton.clicked();
        return true;
    }

    // Rows marked with Ctrl + left click (one row) or Shift + left click (a range); remove acts on
    // these when any are marked. Returns true when the click marked rows instead of selecting one.
    function handleMarkClick(idx) {
        const type = window.markClickType();
        if (type === "toggle") {
            markedIndices = window.toggleMarkedIndex(markedIndices, idx, layersView.currentIndex);
            markAnchor = idx;
            return true;
        }
        if (type === "range") {
            markedIndices = window.markedRange(markAnchor >= 0 ? markAnchor : layersView.currentIndex, idx);
            return true;
        }
        clearMarked();
        markAnchor = idx;
        return false;
    }
    function clearMarked() {
        markAnchor = -1;
        if (markedIndices.length > 0)
            markedIndices = [];
    }
    function isMarked(idx) {
        return markedIndices.indexOf(idx) >= 0;
    }

    // Marks are plain row indices, so any change to the rows drops them.
    Connections {
        function onRowsInserted() { clearMarked(); }
        function onRowsRemoved() { clearMarked(); }
        function onRowsMoved() { clearMarked(); }
        function onModelReset() { clearMarked(); }

        target: layersView.model
    }

    Menu {
        id: pasteLayerMenu

        onOpened: PopupHelpers.handlePopupOpen()
        onClosed: PopupHelpers.handlePopupClose()

        MenuItem { 
            action: actions.layerPasteAction 
            visible: actions.layerPasteAction.enabled
        }
    }

    ColumnLayout {
        id: layersHeader
        enabled: !slides.busyIndicator && !busyIndicator

        spacing: 5

        ColumnLayout {
            id: slidesMenu

            spacing: 1

            RowLayout {
                Layout.preferredWidth: parent.width
                anchors.rightMargin: Kirigami.Units.largeSpacing
                spacing: 1

                Button {
                    icon.name: "document-open"
                    enabled: !layerView.visible

                    onClicked: {
                        slides.openPresentationButton.clicked();
                    }

                    ToolTip {
                        text: qsTr("Open presentation")
                    }
                }
                Button {
                    icon.color: app.slides.slidesNeedsSave ? "orange" : "lime"
                    icon.name: "system-save-session"

                    onClicked: {
                        slides.savePresentationButton.clicked();
                    }

                    ToolTip {
                        text: qsTr("Save presentation")
                    }
                }
                Button {
                    icon.name: "layer-top"
                    enabled: !layerView.visible

                    onClicked: {
                        if(app.slides.selected.layersEnabled){
                            app.slides.selected.moveLayerTop(layersView.currentIndex);
                        }
                    }

                    ToolTip {
                        text: qsTr("Move selected layer to top")
                    }
                }
                Button {
                    icon.name: "layer-bottom"
                    enabled: !layerView.visible

                    onClicked: {
                        if(app.slides.selected.layersEnabled){
                            app.slides.selected.moveLayerBottom(layersView.currentIndex);
                            layersView.currentIndex = layersView.count - 1;
                        }
                    }

                    ToolTip {
                        text: qsTr("Move selected layer to bottom")
                    }
                }
                Button {
                    icon.color: "crimson"
                    icon.name: "trash-empty"
                    enabled: !layerView.visible

                    onClicked: {
                        clearLayersDialog.open();
                    }

                    ToolTip {
                        text: qsTr("Clear layers list")
                    }
                    Dialog {
                        id: clearLayersDialog
                        standardButtons: Dialog.Ok | Dialog.Cancel

                        onOpened: PopupHelpers.handlePopupOpen()
                        onClosed: PopupHelpers.handlePopupClose()

                        Label {
                            text: "Confirm clearing of all items in layers list."
                        }

                        onAccepted: {
                            busyIndicator = true;
                            app.slides.pauseLayerUpdate = true;
                            Qt.callLater(clearAllLayers);
                        }
                    }
                }
                Button {
                    id: configureGridParameters

                    checkable: true
                    checked: layerViewGridParams.visible
                    enabled: layerView.layerItem.layerTypeName !== "Audio"
                    focusPolicy: Qt.NoFocus
                    icon.name: "configure"

                    onClicked: {
                        layerViewGridParams.visible = checked;
                    }

                    ToolTip {
                        text: qsTr("Configure Grid Parameters")
                    }
                }
                Item {
                    // spacer item
                    Layout.fillWidth: true
                }
            }
            RowLayout {
                Layout.preferredWidth: parent.width
                anchors.rightMargin: Kirigami.Units.largeSpacing
                spacing: 1

                Button {
                    id: layerAddButton
                    icon.name: "layer-new"

                    onClicked: {
                        layersAddNew.visible = true;
                    }

                    ToolTip {
                        text: qsTr("Add layer below the slide's own layers")
                    }
                }
                Button {
                    id: layerDeleteButton
                    icon.name: "layer-delete"
                    enabled: !layerView.visible && (markedIndices.length > 0 || !app.slides.selected.isLocked(layersView.currentIndex) || !app.slides.selected.layersCanBeLocked)

                    onClicked: {
                        layersToRemove = window.rowsToRemove(markedIndices, layersView.currentIndex, layersView.count);
                        busyIndicator = true;
                        app.slides.pauseLayerUpdate = true;
                        Qt.callLater(removeLayer);
                    }

                    ToolTip {
                        text: markedIndices.length > 0
                              ? qsTr("Remove the %1 marked layers (locked layers are kept)").arg(markedIndices.length)
                              : app.slides.selected.isReference(layersView.currentIndex)
                              ? qsTr("Remove the selected reference: the original layer stops being visible from this slide on")
                              : qsTr("Remove selected layer")
                    }
                }
                Button {
                    id: layerRaiseButton
                    icon.name: "layer-raise"
                    enabled: !layerView.visible

                    onClicked: {
                        if(app.slides.selected.layersEnabled){
                            app.slides.selected.moveLayerUp(layersView.currentIndex);
                        }
                    }

                    ToolTip {
                        text: qsTr("Move selected layer upwards")
                    }
                }
                Button {
                    icon.name: "layer-lower"
                    enabled: !layerView.visible

                    onClicked: {
                        if(app.slides.selected.layersEnabled){
                            app.slides.selected.moveLayerDown(layersView.currentIndex);
                        }
                    }

                    ToolTip {
                        text: qsTr("Move selected layer downwards")
                    }
                }
                Button {
                    checkable: true
                    checked: layerView.visible
                    icon.name: "document-edit-decrypt-verify"
                    text: qsTr("Layer View")

                    onClicked: {
                        layerView.visible = checked;
                    }

                    ToolTip {
                        text: qsTr("Layer View (showing the selected layer)")
                    }
                }
                Item {
                    // spacer item
                    Layout.fillWidth: true
                }
            }
        }
        Item {
            Layout.fillHeight: true
            // spacer item
            Layout.fillWidth: true
        }
        RowLayout {
            Layout.preferredWidth: layersRoot.width

            Rectangle {
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
                width: Kirigami.Units.gridUnit * 0.5
            }
            Switch {
                id: slideEnabledSwitch

                enabled: app.slides.selected.layersEnabled
                checked: !app.slides.selected.slideSkipped
                padding: 0
                scale: 0.8

                onToggled: {
                    app.slides.selected.slideSkipped = !checked;
                    checked = Qt.binding(() => !app.slides.selected.slideSkipped);
                    if (checked && app.slides.selectedSlideIdx === -1)
                        app.slides.checkMasterLayersRunBasedOnMediaVisibility(mpv.visibility);
                    app.slides.updateSelectedSlide();
                }

                ToolTip {
                    text: app.slides.selectedSlideIdx === -1
                          ? qsTr("Enable/disable the master slide. A disabled master slide is not rendered and all its layers get zero opacity.")
                          : qsTr("Enable/disable the slide. A disabled slide is skipped when going to the next or previous slide.")
                }
            }
            Label {
                id: layersTitle

                font.pointSize: 9
                font.italic: app.slides.selected.slideSkipped
                text: app.slides.selected.getLayersNameShort(25) + qsTr(" Layers")
                wrapMode: Text.Wrap
            }
            Rectangle {
                Layout.fillWidth: true
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
                width: Kirigami.Units.gridUnit + 10
            }
            ButtonGroup {
                id: masterHierarchyGroup
                exclusive: true
            }
            Repeater {
                // Master slide only: marks the selected layer as rendered behind (BACK)
                // or above (FRONT) the layers of all other slides.
                model: [
                    { label: qsTr("Front"), value: 1, tip: qsTr("Front: the selected master layer is rendered above the layers of all slides (below the foreground image)") },
                    { label: qsTr("Back"), value: 0, tip: qsTr("Back: the selected master layer is rendered behind the main video and the layers of all slides") }
                ]
                delegate: Button {
                    required property var modelData
                    readonly property int selectedHierarchy: layersView.currentItem ? layersView.currentItem.layerHierarchy : -1

                    visible: app.slides.selectedSlideIdx === -1
                    enabled: selectedHierarchy >= 0 && app.slides.selected.layersEnabled
                             && !layersView.currentItem.layerIsReference && !layersView.currentItem.layerIsLocked
                    checkable: true
                    checked: selectedHierarchy === modelData.value
                    ButtonGroup.group: masterHierarchyGroup
                    font.pointSize: 8
                    text: modelData.label
                    leftPadding: 15
                    rightPadding: 15
                    Layout.preferredWidth: hierarchyTextMetrics.advanceWidth + leftPadding + rightPadding + 4

                    TextMetrics {
                        id: hierarchyTextMetrics
                        font: parent.font
                        text: parent.text
                    }
                    Layout.preferredHeight: Kirigami.Units.gridUnit * 1.2

                    onClicked: {
                        app.slides.selected.setLayerHierarchy(layersView.currentIndex, modelData.value);
                        checked = Qt.binding(() => selectedHierarchy === modelData.value);
                    }

                    ToolTip {
                        text: modelData.tip
                    }
                }
            }
            Item {
                visible: app.slides.selectedSlideIdx === -1
                Layout.fillHeight: true
                Layout.preferredWidth: 3
            }
        }
    }

    WorkingIndicator {
        id: layerLoadingIndicator
        running: slides.busyIndicator || busyIndicator
        visible: slides.busyIndicator || busyIndicator
        Layout.preferredWidth: parent.width
        Layout.preferredHeight: parent.height
        anchors.fill: parent
        z: 30
    }

    RowLayout {
        id: moveLayerTip

        visible: tipsVisible && !tipsFinished && PresentationSettings.showTipsAtStartup
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 5
        anchors.rightMargin: 5
        anchors.bottomMargin: 5
        spacing: 5

        Kirigami.Icon {
            id: moveLayerTipIcon

            source: "system-help"   // generic tip icon, since the tips rotate between topics
            color: Kirigami.Theme.disabledTextColor
            Layout.preferredWidth: Kirigami.Units.iconSizes.small
            Layout.preferredHeight: Kirigami.Units.iconSizes.small
            Layout.alignment: Qt.AlignTop

            ToolTip {
                text: qsTr("Tips")
            }
        }
        Label {
            id: moveLayerTipLabel

            color: Kirigami.Theme.disabledTextColor
            font.pointSize: 9
            wrapMode: Text.Wrap
            Layout.fillWidth: true
            text: qsTr("Tip: ") + allTips[tipOrder.length > 0 ? tipOrder[tipIndex] : tipIndex]
        }
        ToolButton {
            id: moveLayerTipNextButton

            focusPolicy: Qt.NoFocus
            icon.name: "arrow-right"
            icon.height: 16
            text: ""
            Layout.alignment: Qt.AlignTop
            ToolTip.text: qsTr("Show next tip")

            onClicked: showNextTip()
        }
        ToolButton {
            id: moveLayerTipHideButton

            focusPolicy: Qt.NoFocus
            icon.name: "window-close"
            icon.height: 16
            text: ""
            Layout.alignment: Qt.AlignTop
            ToolTip.text: qsTr("Hide tips")

            onClicked: tipsFinished = true;   // no more tips until next launch
        }
    }

    ScrollView {
        id: layersScrollView

        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        anchors.fill: parent
        anchors.topMargin: layersHeader.height + 5
        anchors.bottomMargin: (moveLayerTip.visible ? moveLayerTip.height : 0) + 5
        clip: true
        z: 20

        ListView {
            id: layersView

            delegate: layersItemCompact
            model: app.slides.selected
            spacing: 1
            enabled: !slides.busyIndicator && !busyIndicator

            MouseArea {
                id: layers_MA
                anchors.bottom: layersView.bottom
                implicitHeight: layersView.height - (layersView.count*50)
                implicitWidth: parent.width
                propagateComposedEvents: true
                acceptedButtons: Qt.RightButton
                onClicked: (mouse)=> {
                    if(app.slides.copyIsAvailable()){
                        pasteLayerMenu.popup();
                    }
                    else {
                        mouse.accepted = false
                    }
                }
            }

            onCurrentIndexChanged: {
                if (layerView.layerItem.layerIdx !== layersView.currentIndex) {
                    layerView.layerItem.layerIdx = layersView.currentIndex;
                }
            }
            onModelChanged: layersRoot.clearMarked()
        }

        Connections {
            function onLayerChanged() {
                if (layerView.layerItem.layerIdx !== layersView.currentIndex) {
                    layersView.currentIndex = layerView.layerItem.layerIdx;
                }
            }
            function onLayerValueChanged() {
                if(app.slides.selected.layersEnabled){
                    app.slides.selected.updateLayer(layerView.layerItem.layerIdx);
                }
            }
            function onLayerNeedsSave() {
                if(app.slides.selected.layersEnabled){
                    app.slides.selected.setLayersNeedsSave(true);
                }
            }

            target: layerView.layerItem
        }
    }

    // Drop files anywhere on the layer list. The insertion line shows where the new
    // layers will land: between two rows, above the first or below the last.
    DropArea {
        id: dropAreaLayers

        property int insertRow: -1
        property int scrollDirection: 0

        // Row pitch of the list (delegate height + spacing).
        function rowPitch() {
            var first = layersView.itemAtIndex(0);
            return (first ? first.height : 50) + layersView.spacing;
        }
        function updateInsertRow(dragY) {
            var contentY = dropAreaLayers.mapToItem(layersView.contentItem, 0, dragY).y;
            var row = Math.round(contentY / rowPitch());
            insertRow = Math.max(0, Math.min(layersView.count, row));

            // Auto-scroll when hovering near the top or bottom edge, so a full
            // list can be scrolled to reach any insertion point.
            var edge = 25;
            if (dragY < edge)
                scrollDirection = -1;
            else if (dragY > height - edge)
                scrollDirection = 1;
            else
                scrollDirection = 0;
        }
        function resetDrag() {
            insertRow = -1;
            scrollDirection = 0;
        }

        anchors.fill: layersScrollView
        keys: ["text/uri-list"]
        z: 21

        onEntered: function(drag) {
            if (!app.slides.selected.layersEnabled) {
                drag.accepted = false;
                return;
            }
            updateInsertRow(drag.y);
        }
        onPositionChanged: function(drag) {
            updateInsertRow(drag.y);
        }
        onExited: resetDrag()

        onDropped: function(drop) {
            var insertAt = insertRow >= 0 ? insertRow : layersView.count;
            resetDrag();
            if(app.slides.selected.layersEnabled){
                // Pause the render thread's layer updates while the batch is
                // inserted, exactly like the clear/remove operations do. Without
                // this the render loop starts loading every layer as it appears,
                // competing with the GUI thread for the model and stalling the UI.
                busyIndicator = true;
                app.slides.pauseLayerUpdate = true;

                // Add every file first and only point the layer view at the
                // last one afterwards. Rebinding layerIdx per file made the
                // preview tear down and re-attach its layer (and repaint the
                // window) once per dropped file, which stalled the UI when a
                // handful of images came in at the same time.
                // New layers are appended, then moved to the insertion point
                // so that multiple files keep their dropped order.
                var lastAdded = -1;
                for(var i in drop.urls){
                    var idx = app.slides.selected.addLayerBasedOnMime(drop.urls[i]);
                    if(idx >= 0) {
                        if (idx !== insertAt) {
                            app.slides.selected.moveLayer(idx, insertAt);
                            idx = insertAt;
                        }
                        lastAdded = idx;
                        insertAt++;
                    }
                }
                if(lastAdded >= 0) {
                    layerView.layerItem.layerIdx = lastAdded;
                    layersView.positionViewAtIndex(lastAdded, ListView.Contain);
                }

                app.slides.updateSelectedSlide();
                mpv.focus = true;
                addDroppedLayersTimer.start();
            }
        }

        Rectangle {
            id: dropInsertionLine

            visible: dropAreaLayers.containsDrag && dropAreaLayers.insertRow >= 0
            x: 0
            width: parent.width
            height: 4
            radius: 2
            color: Kirigami.Theme.highlightColor
            opacity: 0.95
            y: {
                if (dropAreaLayers.insertRow < 0)
                    return 0;
                // Keep a dependency on contentY so the line follows auto-scroll.
                var contentY = layersView.contentY;
                var lineY = dropAreaLayers.insertRow * dropAreaLayers.rowPitch() - layersView.spacing / 2;
                var localY = layersView.contentItem.mapToItem(dropAreaLayers, 0, lineY).y - height / 2;
                return Math.max(0, Math.min(dropAreaLayers.height - height, localY));
            }
        }

        Timer {
            interval: 30
            repeat: true
            running: dropAreaLayers.containsDrag && dropAreaLayers.scrollDirection !== 0

            onTriggered: {
                var maxY = Math.max(layersView.originY, layersView.originY + layersView.contentHeight - layersView.height);
                layersView.contentY = Math.max(layersView.originY, Math.min(maxY, layersView.contentY + dropAreaLayers.scrollDirection * 10));
                dropAreaLayers.updateInsertRow(dropAreaLayers.drag.y);
            }
        }
    }

    Component {
        id: layersItemCompact

        LayersItemCompact {
            highlighted: layersRoot.markedIndices.length > 0 ? layersRoot.isMarked(index) : layersView.currentIndex === index
        }
    }
    Timer {
        id: scrollPositionTimer

        interval: 50
        repeat: true
        running: true

        onTriggered: {
            scrollPositionTimer.stop();
        }
    }

    Timer {
        id: clearAllLayersTimer

        interval: 500
        repeat: false

        onTriggered: {
            if(app.slides.selected.layersEnabled){
                app.slides.selected.clearLayers();
            }
            app.slides.updateSelectedSlide();
            app.slides.pauseLayerUpdate = false;
            busyIndicator = false;
        }
    }
    Timer {
        id: addDroppedLayersTimer

        interval: 500
        repeat: false

        onTriggered: {
            app.slides.updateSelectedSlide();
            app.slides.pauseLayerUpdate = false;
            busyIndicator = false;
        }
    }
    Timer {
        id: removeLayerTimer

        interval: 500
        repeat: false

        onTriggered: {
            if(app.slides.selected.layersEnabled){
                if (app.slides.removeLayers(app.slides.selected, layersToRemove))
                    window.recordRemoval("slides");
            }
            layersToRemove = [];
            clearMarked();
            app.slides.updateSelectedSlide();
            app.slides.pauseLayerUpdate = false;
            busyIndicator = false;
        }
    }
    Timer {
        id: tipShowTimer

        interval: 5000
        repeat: false

        onTriggered: {
            if (isBusy) {
                // Still loading - keep waiting and restart the countdown from scratch.
                tipShowTimer.start();
            } else {
                tipsVisible = true;
            }
        }
    }
}
