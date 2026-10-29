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

// Confirms in the master view that a file opened by the user (Open File, drag and drop or the
// playlist) was loaded. It can be turned off with UserInterfaceSettings.showLoadNotice. Without it a load can look like nothing happened: the first frame is often
// black, and nothing is shown at all while the media visibility is 0%. In that case the
// notice stays up with a warning and a button to fade the visibility up, until the
// visibility is raised or the notice is closed.
//
// When the file is synced to cluster nodes it also follows the nodes loading it (from the
// main video load tracking of NodeLoaderVerifier): it stays up while nodes are still loading,
// and until it is closed when a node failed to load the file or did not answer in time.
Pane {
    id: root

    // Set by expect() and cleared once the load succeeded or failed, so loads that the
    // user did not start (end of file continue, sections, ...) show no notice.
    property string expectedFileName: ""
    property bool opened: false
    property bool failed: false
    property string fileName: ""
    readonly property bool visibilityIsZero: mpv.visibility === 0

    // Node state of the current main video load. nodesLoadCount is 0 when no node is
    // connected or the file is not synced to the nodes; the node rows are hidden then.
    readonly property bool hasNodes: mpv.nodesLoadCount > 0
    readonly property bool nodesWaiting: hasNodes && mpv.nodesLoadWaiting.length > 0 && !mpv.nodesLoadTimedOut
    readonly property bool nodesProblem: hasNodes && (mpv.nodesLoadFailed.length > 0 || mpv.nodesLoadTimedOut)
    readonly property bool keepOpen: (!failed && visibilityIsZero) || nodesWaiting || nodesProblem

    function expect(path) {
        if (UserInterfaceSettings.showLoadNotice)
            expectedFileName = playerController.returnFileName(path);
    }

    function expectPlaylistItem(index) {
        if (UserInterfaceSettings.showLoadNotice)
            expectedFileName = mpv.playlistModel.mediaTitle(index);
    }

    function show(loadFailed) {
        fileName = (!loadFailed && mpv.mediaTitle !== "") ? mpv.mediaTitle : expectedFileName;
        expectedFileName = "";
        failed = loadFailed;
        opened = true;
        hideTimer.restart();
    }

    visible: opened && UserInterfaceSettings.showLoadNotice && !window.isFullScreenMode
    z: 100   // above the video / 3D view
    padding: Kirigami.Units.largeSpacing

    background: Rectangle {
        color: Kirigami.Theme.backgroundColor
        border.color: (root.failed || root.nodesProblem) ? Kirigami.Theme.negativeTextColor
                    : (root.visibilityIsZero || root.nodesWaiting) ? Kirigami.Theme.neutralTextColor
                    : Kirigami.Theme.positiveTextColor
        border.width: 2
        radius: Kirigami.Units.smallSpacing
        opacity: 0.95
    }

    Connections {
        function onFileLoaded() {
            if (root.expectedFileName !== "")
                root.show(false);
        }
        function onEndFile(reason) {
            if (reason === "error" && root.expectedFileName !== "")
                root.show(true);
        }

        target: mpv
    }

    // Hides the notice after a while, but not while there is still something to act on.
    Timer {
        id: hideTimer

        interval: 5000

        onTriggered: {
            if (!root.keepOpen)
                root.opened = false;
        }
    }
    // Once the visibility has been raised and the nodes are ready, let the notice go after a
    // short while.
    onKeepOpenChanged: {
        if (opened && !keepOpen && !hideTimer.running)
            hideTimer.restart();
    }

    contentItem: RowLayout {
        spacing: Kirigami.Units.largeSpacing

        Kirigami.Icon {
            Layout.alignment: Qt.AlignTop
            implicitWidth: Kirigami.Units.iconSizes.medium
            implicitHeight: Kirigami.Units.iconSizes.medium
            source: (root.failed || root.nodesProblem) ? "dialog-error"
                  : (root.visibilityIsZero || root.nodesWaiting) ? "dialog-warning"
                  : "dialog-ok"
        }
        ColumnLayout {
            spacing: Kirigami.Units.smallSpacing

            Label {
                Layout.maximumWidth: mpv.width * 0.6
                elide: Text.ElideMiddle
                font.bold: true
                text: root.failed ? qsTr("Could not load: %1").arg(root.fileName)
                                  : qsTr("Loaded: %1").arg(root.fileName)
            }
            Label {
                visible: !root.failed
                text: {
                    // A playlist item can start playing on its own (auto play).
                    const state = mpv.pause ? qsTr("Paused, the first frame may be black.") : qsTr("Playing.");
                    return mpv.duration > 0 ? qsTr("Duration %1. %2").arg(app.formatTime(mpv.duration)).arg(state)
                                            : state;
                }
            }
            Label {
                visible: !root.failed && root.visibilityIsZero
                color: Kirigami.Theme.neutralTextColor
                font.bold: true
                text: qsTr("Media visibility is 0%, so nothing is shown.")
            }
            RowLayout {
                visible: root.hasNodes
                spacing: Kirigami.Units.smallSpacing

                BusyIndicator {
                    visible: root.nodesWaiting
                    running: visible
                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                    Layout.preferredHeight: Kirigami.Units.iconSizes.small
                }
                Label {
                    Layout.maximumWidth: mpv.width * 0.6
                    elide: Text.ElideRight
                    color: root.nodesProblem ? Kirigami.Theme.negativeTextColor
                         : root.nodesWaiting ? Kirigami.Theme.neutralTextColor
                         : Kirigami.Theme.positiveTextColor
                    text: {
                        const total = mpv.nodesLoadCount;
                        const failedNodes = mpv.nodesLoadFailed;
                        const waitingNodes = mpv.nodesLoadWaiting;
                        if (failedNodes.length > 0)
                            return qsTr("Failed to load on %1 of %2 nodes: %3")
                                .arg(failedNodes.length).arg(total).arg(failedNodes.join(", "));
                        if (mpv.nodesLoadTimedOut)
                            return qsTr("No answer from %1 of %2 nodes: %3")
                                .arg(waitingNodes.length).arg(total).arg(waitingNodes.join(", "));
                        if (waitingNodes.length > 0)
                            return qsTr("Loading on nodes: %1 of %2 ready (waiting for %3)")
                                .arg(total - waitingNodes.length).arg(total).arg(waitingNodes.join(", "));
                        return total === 1 ? qsTr("Loaded on the node")
                                           : qsTr("Loaded on all %1 nodes").arg(total);
                    }
                }
            }
        }
        Button {
            Layout.alignment: Qt.AlignVCenter
            visible: !root.failed && root.visibilityIsZero
            icon.name: "visibility"
            text: qsTr("Fade up")

            onClicked: actions.visibilityFadeUpAction.trigger()
        }
        ToolButton {
            Layout.alignment: Qt.AlignTop
            icon.name: "window-close"

            onClicked: root.opened = false

            ToolTip.text: qsTr("Close")
            ToolTip.visible: hovered
        }
    }
}
