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

// Shows in the master view how the cluster nodes close when C-Play quits: the master waits
// for every node connected over DataTransfer to report that it closed (or drop its
// connection) and quits slightly after the last one. Outside a quit it reports nodes that
// closed on their own (or whose connection was lost) until the notice is closed.
Pane {
    id: root

    readonly property bool shuttingDown: NodeShutdown.active
    readonly property var closedNodes: NodeShutdown.closedNodes
    readonly property bool hasClosedNodes: closedNodes.length > 0
    readonly property bool problem: shuttingDown ? NodeShutdown.timedOut : hasClosedNodes

    function namesWithState(state) {
        const names = [];
        for (const n of NodeShutdown.nodes) {
            if (n.state === state)
                names.push(n.name);
        }
        return names;
    }

    // The shutdown progress is shown also in full screen mode: the window is held open for it.
    visible: shuttingDown || (hasClosedNodes && !window.isFullScreenMode)
    z: 100   // above the video / 3D view
    padding: Kirigami.Units.largeSpacing

    background: Rectangle {
        color: Kirigami.Theme.backgroundColor
        border.color: root.problem ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.neutralTextColor
        border.width: 2
        radius: Kirigami.Units.smallSpacing
        opacity: 0.95
    }

    contentItem: RowLayout {
        spacing: Kirigami.Units.largeSpacing

        BusyIndicator {
            Layout.alignment: Qt.AlignTop
            visible: root.shuttingDown && NodeShutdown.closedCount < NodeShutdown.nodeCount
            running: visible
            Layout.preferredWidth: Kirigami.Units.iconSizes.medium
            Layout.preferredHeight: Kirigami.Units.iconSizes.medium
        }
        Kirigami.Icon {
            Layout.alignment: Qt.AlignTop
            visible: !root.shuttingDown || NodeShutdown.closedCount >= NodeShutdown.nodeCount
            implicitWidth: Kirigami.Units.iconSizes.medium
            implicitHeight: Kirigami.Units.iconSizes.medium
            source: root.problem ? "dialog-error" : root.shuttingDown ? "dialog-ok" : "dialog-warning"
        }
        ColumnLayout {
            spacing: Kirigami.Units.smallSpacing

            Label {
                font.bold: true
                text: root.shuttingDown ? qsTr("Closing C-Play: shutting down nodes")
                                        : (root.closedNodes.length === 1 ? qsTr("A node has shut down")
                                                                         : qsTr("%1 nodes have shut down").arg(root.closedNodes.length))
            }
            Label {
                visible: root.shuttingDown
                Layout.maximumWidth: mpv.width * 0.6
                elide: Text.ElideRight
                color: root.problem ? Kirigami.Theme.negativeTextColor
                     : NodeShutdown.closedCount >= NodeShutdown.nodeCount ? Kirigami.Theme.positiveTextColor
                     : Kirigami.Theme.neutralTextColor
                text: {
                    const total = NodeShutdown.nodeCount;
                    const closed = NodeShutdown.closedCount;
                    if (NodeShutdown.timedOut)
                        return qsTr("No answer from %1 of %2 nodes: %3, closing anyway")
                            .arg(total - closed).arg(total)
                            .arg(root.namesWithState("running").concat(root.namesWithState("closing")).join(", "));
                    if (closed >= total)
                        return total === 1 ? qsTr("The node has closed, closing the master")
                                           : qsTr("All %1 nodes have closed, closing the master").arg(total);
                    const closing = root.namesWithState("closing");
                    const running = root.namesWithState("running");
                    let waiting = qsTr("%1 of %2 closed").arg(closed).arg(total);
                    if (closing.length > 0)
                        waiting += qsTr(", closing: %1").arg(closing.join(", "));
                    if (running.length > 0)
                        waiting += qsTr(", waiting for: %1").arg(running.join(", "));
                    return waiting;
                }
            }
            Repeater {
                model: root.shuttingDown ? [] : root.closedNodes

                Label {
                    required property var modelData

                    Layout.maximumWidth: mpv.width * 0.6
                    elide: Text.ElideRight
                    color: Kirigami.Theme.negativeTextColor
                    text: modelData.reason === "closed" ? qsTr("%1 closed").arg(modelData.name)
                                                        : qsTr("%1: connection lost").arg(modelData.name)
                }
            }
        }
        ToolButton {
            Layout.alignment: Qt.AlignTop
            visible: !root.shuttingDown
            icon.name: "window-close"

            onClicked: NodeShutdown.clearClosedNodes()

            ToolTip.text: qsTr("Close")
            ToolTip.visible: hovered
        }
    }
}
