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

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

ItemDelegate {
    id: root

    property string iconName: "kt-set-max-upload-speed"
    property int iconWidth: 0
    property string rowNumber: (index + 1).toString()

    // Drag state for visual feedback
    property bool dragging: false
    property real originalY: 0

    // transient insertion indicator instance (created on drag start)
    property var insertionIndicator: null

    function mainText() {
        const rowNumber = pad(root.rowNumber, playlistView.count.toString().length) + ". ";
        if (PlaylistSettings.showRowNumber) {
            return rowNumber + (PlaylistSettings.showMediaTitle ? model.title : model.name);
        }
        return (PlaylistSettings.showMediaTitle ? model.title : model.name);
    }
    function pad(number, length) {
        while (number.length < length)
            number = "0" + number;
        return number;
    }
    function subText() {
        if (model.hasDescriptionFile) {
            return model.duration + " : " + model.stereoVideo + " " + model.gridToMapOn + " : (" + model.eofMode + ")";
        }
        return model.duration + " : (" + model.eofMode + ")";
    }

    down: model.isPlaying
    font.pointSize: 9
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
        anchors.fill: parent
        color: root.hovered || root.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
        icon.color: color
        icon.name: iconName
        icon.width: iconWidth
        selected: root.down
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
                width: parent ? parent.width : playlistView.width
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
                playlistView.currentIndex = index;

                // create insertion indicator (shared transient visual between rows)
                if (playlistView && playlistView.contentItem && !root.insertionIndicator) {
                    // create indicator initially at current center position
                    root.insertionIndicator = insertionLineComp.createObject(playlistView.contentItem, { x: 0, y: root.y + root.height/2 - 2, width: playlistView.width });
                }
            }
            onPositionChanged: {
                // Boundaries relative to ListView content
                var minY = -root.y;
                var maxY = playlistView.contentHeight - root.y - root.height;
                if (drag.y < minY) drag.y = minY;
                if (drag.y > maxY) drag.y = maxY;

                // Optional: auto-scroll the list when dragging near edges
                var edgeThreshold = 20;
                var localY = root.y;
                if (localY < playlistView.contentY + edgeThreshold && playlistView.contentY > 0) {
                    playlistView.contentY = Math.max(0, playlistView.contentY - 8);
                }
                var bottomEdge = playlistView.contentHeight - edgeThreshold - root.height;
                if (localY > bottomEdge && (playlistView.contentY < playlistView.contentHeight - playlistView.height)) {
                    playlistView.contentY = Math.min(playlistView.contentHeight - playlistView.height, playlistView.contentY + 8);
                }

                // update insertion indicator position to show where the item will land
                if (root.insertionIndicator) {
                    // compute index based on delegate center
                    var centerY = root.y + root.height / 2;
                    var destIndex = Math.floor(centerY / root.height);
                    if (destIndex < 0) destIndex = 0;
                    if (destIndex > playlistView.count - 1) destIndex = playlistView.count - 1;

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
                    var maxIndicatorY = playlistView.contentHeight - root.insertionIndicator.height;
                    if (indicatorY > maxIndicatorY) indicatorY = maxIndicatorY;

                    root.insertionIndicator.y = indicatorY;
                    // ensure width follows view width (in case of resize while dragging)
                    root.insertionIndicator.width = playlistView.width;
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
                if (destIndex > playlistView.count - 1) destIndex = playlistView.count - 1;

                // If dragged down, insert AFTER destIndex; if up, insert BEFORE destIndex.
                var finalDest = (root.y > root.originalY) ? (destIndex) : destIndex;

                // clamp finalDest to valid range (last valid index is count-1)
                if (finalDest < 0) finalDest = 0;
                if (finalDest > playlistView.count - 1) finalDest = playlistView.count - 1;

                mpv.playlistModel.moveItem(index, finalDest);

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
        iconWidth = Kirigami.Units.gridUnit * 1.8;
        mpv.position = 0;
        mpv.loadItem(index);
        mpv.playlistModel.setPlayingVideo(index);
        if (mpv.autoPlay)
            playItem.start();
    }

    Connections {
        function onPauseChanged() {
            if (model.isPlaying) {
                if (mpv.pause) {
                    iconName = "media-playback-pause";
                } else {
                    iconName = "media-playback-start";
                }
                iconWidth = root.height * 0.8;
            } else {
                iconName = "kt-set-max-upload-speed";
                iconWidth = 0;
            }
        }

        target: mpv
    }
    Timer {
        id: playItem

        interval: PlaylistSettings.autoPlayAfterTime * 1000

        onTriggered: {
            mpv.pause = false;
        }
    }
}
