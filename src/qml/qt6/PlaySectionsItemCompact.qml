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

ItemDelegate {
    id: root

    property string rowNumber: (index + 1).toString()

    // Drag state for visual feedback
    property bool dragging: false
    property real originalY: 0

    // transient insertion indicator instance (created on drag start)
    property var insertionIndicator: null

    function mainText() {
        const rowNumber = pad(root.rowNumber, sectionsView.count.toString().length) + ". ";
        if (PlaylistSettings.showRowNumber) {
            return rowNumber + (model.title);
        }
        return (model.title);
    }
    function pad(number, length) {
        while (number.length < length)
            number = "0" + number;
        return number;
    }
    function subText() {
        return model.startTime + " - " + model.endTime + " (" + model.duration + ")" + "\nAt end: " + model.eosMode;
    }

    down: model.isPlaying
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
    contentItem: Kirigami.IconTitleSubtitle {
        color: root.hovered || root.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
        icon.color: color
        icon.name: "drive-partition"
        icon.width: model.isPlaying ? root.height * 0.8 : 0
        subtitle: subText()
        title: mainText()
    }

    // Drag handle area: small right-side handle to avoid blocking interactive controls.
    Rectangle {
        id: dragHandleRect
        anchors.right: parent.right
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
                width: parent ? parent.width : sectionsView.width
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
                sectionsView.currentIndex = index;

                // create insertion indicator (shared transient visual between rows)
                if (sectionsView && sectionsView.contentItem && !root.insertionIndicator) {
                    // create indicator initially at current center position
                    root.insertionIndicator = insertionLineComp.createObject(sectionsView.contentItem, { x: 0, y: root.y + root.height/2 - 2, width: sectionsView.width });
                }
            }
            onPositionChanged: {
                // Boundaries relative to ListView content
                var minY = -root.y;
                var maxY = sectionsView.contentHeight - root.y - root.height;
                if (drag.y < minY) drag.y = minY;
                if (drag.y > maxY) drag.y = maxY;

                // Optional: auto-scroll the list when dragging near edges
                var edgeThreshold = 20;
                var localY = root.y;
                if (localY < sectionsView.contentY + edgeThreshold && sectionsView.contentY > 0) {
                    sectionsView.contentY = Math.max(0, sectionsView.contentY - 8);
                }
                var bottomEdge = sectionsView.contentHeight - edgeThreshold - root.height;
                if (localY > bottomEdge && (sectionsView.contentY < sectionsView.contentHeight - sectionsView.height)) {
                    sectionsView.contentY = Math.min(sectionsView.contentHeight - sectionsView.height, sectionsView.contentY + 8);
                }

                // update insertion indicator position to show where the item will land
                if (root.insertionIndicator) {
                    // compute index based on delegate center
                    var centerY = root.y + root.height / 2;
                    var destIndex = Math.floor(centerY / root.height);
                    if (destIndex < 0) destIndex = 0;
                    if (destIndex > sectionsView.count - 1) destIndex = sectionsView.count - 1;

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
                    var maxIndicatorY = sectionsView.contentHeight - root.insertionIndicator.height;
                    if (indicatorY > maxIndicatorY) indicatorY = maxIndicatorY;

                    root.insertionIndicator.y = indicatorY;
                    // ensure width follows view width (in case of resize while dragging)
                    root.insertionIndicator.width = sectionsView.width;
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
                if (destIndex > sectionsView.count - 1) destIndex = sectionsView.count - 1;

                // If dragged down, insert AFTER destIndex; if up, insert BEFORE destIndex.
                var finalDest = (root.y > root.originalY) ? (destIndex) : destIndex;

                // clamp finalDest to valid range (last valid index is count-1)
                if (finalDest < 0) finalDest = 0;
                if (finalDest > sectionsView.count - 1) finalDest = sectionsView.count - 1;

                mpv.playSectionsModel.moveSection(index, finalDest);

                // remove transient indicator after move
                if (root.insertionIndicator) {
                    root.insertionIndicator.destroy();
                    root.insertionIndicator = null;
                }
            }
        }
    }

    onDoubleClicked: {
        mpv.pause = true;
        mpv.loadSection(index);
    }

    Timer {
        id: playItem

        interval: 2000

        onTriggered: {
            mpv.pause = false;
        }
    }
}
