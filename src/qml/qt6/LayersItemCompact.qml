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
        if(model.type === "Audio")
            return model.type;
        else
            return model.type + model.page + " - " + model.stereoVideo + " " + model.gridToMapOn;
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
            return Kirigami.Theme.backgroundColor;
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
                title: (layersView.currentIndex === index ? layersNumText() : layersNumText() + model.title)
            }
            TextInput {
                id: slideTitleField

                anchors.left: its.left
                anchors.leftMargin: 12
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
                visible: app.slides.selected.layersCanBeLocked
                hoverEnabled: false
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
            }
            Rectangle {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: 5
                width: 10
                height: 10
                radius: 5
                color: (model.status === 2 ? "lime" : model.status === 1 ? "orange" : model.status === 0 ? "crimson" : "black")
            }
            Item {
                anchors.bottom: parent.bottom
                anchors.right: its.right
                implicitHeight: 25
                implicitWidth: 100
                visible: !visibilitySlider.visible

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

                anchors.bottom: parent.bottom
                anchors.right: its.right
                visible: layersView.currentIndex === index
                implicitWidth: 100
                overlayLabel: qsTr("")

                onValueChanged: {
                    if (!layersView.enabled || visibilitySlider.enabled) {
                        if (value.toFixed(0) !== layerView.layerItem.layerVisibility) {
                            layerView.layerItem.layerVisibility = value.toFixed(0);
                            app.slides.needsSync = true;
                        }
                    }
                }
                Component.onCompleted: {
                    visibilitySlider.value = layerView.layerItem.layerVisibility;
                }
            }
            Item {
                anchors.bottom: parent.bottom
                anchors.right: its.right
                implicitHeight: 20
                implicitWidth: 100
                visible: layersView.currentIndex !== index

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
            anchors.leftMargin: -6
            width: 12
            color: "transparent"

            Kirigami.Icon {
                anchors.bottom: parent.bottom
                source: "drag-surface"
                width: 12
                height: 12
                opacity: 0.6
                visible: !root.dragging
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

                    // create insertion indicator (shared transient visual between rows)
                    if (layersView && layersView.contentItem && !root.insertionIndicator) {
                        // create indicator initially at current center position
                        root.insertionIndicator = insertionLineComp.createObject(layersView.contentItem, { x: 0, y: root.y + root.height/2 - 2, width: layersView.width });
                    }
                }
                                onPositionChanged: {
                    // Boundaries relative to ListView content
                    var minY = -root.y;
                    var maxY = layersView.contentHeight - root.y - root.height;
                    if (drag.y < minY) drag.y = minY;
                    if (drag.y > maxY) drag.y = maxY;

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
        if (layerView.layerItem.layerVisibility === 100 && !visibility_fade_out_animation.running) {
            visibility_fade_out_animation.start();
        } else if (layerView.layerItem.layerVisibility < 100 && !visibility_fade_in_animation.running) {
            visibility_fade_in_animation.start();
        }
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
            if (visibilitySlider.value !== layerView.layerItem.layerVisibility)
                visibilitySlider.value = layerView.layerItem.layerVisibility;
        }
        function onLayerValueChanged() {
            if (visibilitySlider.value !== layerView.layerItem.layerVisibility)
                visibilitySlider.value = layerView.layerItem.layerVisibility;
        }

        target: layerView.layerItem
    }
}
