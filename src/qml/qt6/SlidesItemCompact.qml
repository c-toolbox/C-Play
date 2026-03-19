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

    function pad(number, length) {
        while (number.length < length)
            number = "0" + number;
        return number;
    }
    function slideNumText() {
        const rowNumber = pad(root.rowNumber, slidesView.count.toString().length) + ". ";
        return rowNumber;
    }
    function subText() {
        return model.layers + (model.layers === 1 ? " layer" : " layers");
    }

    down: app.slides.triggeredSlideIdx === index
    font.pointSize: 9
    highlighted: slidesView.currentIndex === index
    implicitWidth: ListView.view.width
    padding: 0

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
    
    Menu {
        id: pasteLayerMenu
        MenuItem { 
            action: actions.layerPasteAction 
            visible: actions.layerPasteAction.enabled
        }
    }
    
    contentItem: Rectangle {
        color: "transparent"
        implicitHeight: 50
        implicitWidth: root.width

        MouseArea {
            id: slideIC_MA
            implicitHeight: parent.height
            implicitWidth: parent.width
            acceptedButtons: Qt.RightButton
            onClicked: {
                if(app.slides.copyIsAvailable()){
                    pasteLayerMenu.popup();
                    app.slides.slideToPaste = index;
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
                title: (slidesView.currentIndex === index ? slideNumText() : slideNumText() + model.name)
            }
            TextInput {
                id: slideNameField

                anchors.left: its.left
                anchors.leftMargin: 12
                anchors.top: parent.top
                anchors.topMargin: 3
                color: Kirigami.Theme.textColor
                font.pointSize: 9
                maximumLength: 24
                text: model.name
                visible: slidesView.currentIndex === index

                onEditingFinished: {
                    if(app.slides.selected.layersEnabled){
                        app.slides.selected.layersName = slideNameField.text;
                    }
                    app.slides.updateSelectedSlide();
                }
            }
            Label {
                anchors.leftMargin: 93
                anchors.bottomMargin: 5
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                font.pointSize: 9
                visible: model.locked && PresentationSettings.customSlidesCanHaveLockableLayers
                text: model.countlocked
                color: "gray"
            }
            Button  {
                flat: true
                hoverEnabled: false
                visible: PresentationSettings.customSlidesCanHaveLockableLayers
                anchors.leftMargin: 93
                anchors.bottomMargin: -3
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                icon.name: model.locked ? "object-locked" : "object-unlocked"
                icon.color: model.locked ? "darkred" : "gray"
                icon.width: 20
                icon.height: 20

                onClicked: {
                    app.slides.slide(index).layersCanBeLocked = !app.slides.slide(index).layersCanBeLocked;
                    app.slides.updateSlide(index);
                }
            }
            Rectangle {
                id: layerMinStatus
                anchors.top: parent.top
                anchors.right: layerMaxStatus.left
                anchors.topMargin: 5
                width: 10
                height: 10
                radius: 5
                visible: model.layerminstatus !== model.layermaxstatus
                color: (model.layerminstatus === 1 ? "orange" : "crimson")          
            }
            Rectangle {
                id: layerMaxStatus
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: 5
                width: 10
                height: 10
                radius: 5
                visible: model.layerminstatus !== model.layermaxstatus
                color: (model.layermaxstatus === 2 ? "lime" : "orange")       
            }
            Rectangle {
                id: layerStatus
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: 5
                width: 10
                height: 10
                radius: 5
                visible: model.layerminstatus === model.layermaxstatus
                color: (model.layerminstatus === 2 ? "lime" : model.layerminstatus === 1 ? "orange" : model.layerminstatus === 0 ? "crimson" : "black")         
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
                enabled: false
                implicitWidth: 100
                overlayLabel: qsTr("")
                visible: false

                onValueChanged: {
                    if (!slidesView.enabled) {
                        if (value.toFixed(0) !== app.slides.triggeredSlideVisibility) {
                            app.slides.triggeredSlideVisibility = value.toFixed(0);
                            app.slides.updateSelectedSlide();
                        }
                        if (value.toFixed(0) !== layerView.layerItem.layerVisibility) {
                            layerView.layerItem.layerVisibility = value.toFixed(0);
                            app.slides.updateSelectedSlide();
                        }
                    }
                }
                Component.onCompleted: {
                    visibilitySlider.value = layerView.layerItem.layerVisibility;
                }
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
                width: parent ? parent.width : slidesView.width
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
                slidesView.currentIndex = index;

                // create insertion indicator (shared transient visual between rows)
                if (slidesView && slidesView.contentItem && !root.insertionIndicator) {
                    // create indicator initially at current center position
                    root.insertionIndicator = insertionLineComp.createObject(slidesView.contentItem, { x: 0, y: root.y + root.height/2 - 2, width: slidesView.width });
                }
            }
            onPositionChanged: {
                // Boundaries relative to ListView content
                var minY = -root.y;
                var maxY = slidesView.contentHeight - root.y - root.height;
                if (drag.y < minY) drag.y = minY;
                if (drag.y > maxY) drag.y = maxY;

                // Optional: auto-scroll the list when dragging near edges
                var edgeThreshold = 20;
                var localY = root.y;
                if (localY < slidesView.contentY + edgeThreshold && slidesView.contentY > 0) {
                    slidesView.contentY = Math.max(0, slidesView.contentY - 8);
                }
                var bottomEdge = slidesView.contentHeight - edgeThreshold - root.height;
                if (localY > bottomEdge && (slidesView.contentY < slidesView.contentHeight - slidesView.height)) {
                    slidesView.contentY = Math.min(slidesView.contentHeight - slidesView.height, slidesView.contentY + 8);
                }

                // update insertion indicator position to show where the item will land
                if (root.insertionIndicator) {
                    // compute index based on delegate center
                    var centerY = root.y + root.height / 2;
                    var destIndex = Math.floor(centerY / root.height);
                    if (destIndex < 0) destIndex = 0;
                    if (destIndex > slidesView.count - 1) destIndex = slidesView.count - 1;

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
                    var maxIndicatorY = slidesView.contentHeight - root.insertionIndicator.height;
                    if (indicatorY > maxIndicatorY) indicatorY = maxIndicatorY;

                    root.insertionIndicator.y = indicatorY;
                    // ensure width follows view width (in case of resize while dragging)
                    root.insertionIndicator.width = slidesView.width;
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
                if (destIndex > slidesView.count - 1) destIndex = slidesView.count - 1;

                // If dragged down, insert AFTER destIndex; if up, insert BEFORE destIndex.
                var finalDest = (root.y > root.originalY) ? (destIndex) : destIndex;

                // clamp finalDest to valid range (last valid index is count-1)
                if (finalDest < 0) finalDest = 0;
                if (finalDest > slidesView.count - 1) finalDest = slidesView.count - 1;

                app.slides.moveSlide(index, finalDest);

                // remove transient indicator after move
                if (root.insertionIndicator) {
                    root.insertionIndicator.destroy();
                    root.insertionIndicator = null;
                }
            }
        }
    }

    onClicked: {
        slidesView.currentIndex = index;
        app.slides.slideToPaste = index;
    }
    onDoubleClicked: {
        slidesView.currentIndex = index;
        app.slides.slideFadeTime = PresentationSettings.fadeDurationToNextSlide;
        app.slides.triggeredSlideIdx = index;
    }

    PropertyAnimation {
        id: visibility_fade_out_animation

        duration: PlaybackSettings.fadeDuration
        property: "value"
        target: visibilitySlider
        to: 0

        onFinished: {
            slidesView.enabled = true;
            app.slides.pauseLayerUpdate = false;
            app.action("slidePrevious").enabled = true;
            app.action("slideNext").enabled = true;
        }
        onStarted: {
            slidesView.enabled = false;
            app.slides.pauseLayerUpdate = true;
            app.action("slidePrevious").enabled = false;
            app.action("slideNext").enabled = false;
        }
    }
    PropertyAnimation {
        id: visibility_fade_in_animation

        duration: PlaybackSettings.fadeDuration
        property: "value"
        target: visibilitySlider
        to: 100

        onFinished: {
            slidesView.enabled = true;
            app.slides.pauseLayerUpdate = false;
            app.action("slidePrevious").enabled = true;
            app.action("slideNext").enabled = true;
        }
        onStarted: {
            slidesView.enabled = false;
            app.slides.pauseLayerUpdate = true;
            app.action("slidePrevious").enabled = false;
            app.action("slideNext").enabled = false;
        }
    }
    Connections {
        function onSelectedSlideChanged() {
            if(app.slides.selected.layersEnabled) {
                visibilitySlider.value = app.slides.selected.layersVisibility;
            }
        }

        function onTriggeredSlideChanged() {
            if(app.slides.selected.layersEnabled) {
                if (app.slides.selected.layersVisibility === 100 && !visibility_fade_out_animation.running) {
                    visibility_fade_out_animation.duration = app.slides.slideFadeTime / 2;
                    visibility_fade_out_animation.start();
                }
                if (app.slides.selected.layersVisibility === 0 && !visibility_fade_in_animation.running) {
                    visibility_fade_in_animation.duration = app.slides.slideFadeTime / 2;
                    visibility_fade_in_animation.start();
                }
            }
        }

        target: app.slides
    }
}
