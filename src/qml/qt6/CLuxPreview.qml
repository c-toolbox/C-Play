/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Controls
import org.kde.kirigami as Kirigami

// Dome ring visualizer for the C-Lux editor, inspired by the C-Lux web frontend's pattern
// visualizer: one wedge per light around a thin ring, colored with the current frame.
Item {
    id: root

    // nLights*3 ints (r, g, b per light), from CLuxClient.frame().
    property var frame: []
    property int nLights: 0

    Canvas {
        id: ringCanvas
        anchors.centerIn: parent
        width: Math.max(0, Math.min(root.width, root.height) - 20)
        height: width

        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();

            if (root.nLights <= 0 || root.frame.length < root.nLights * 3)
                return;

            // Same geometry as the web visualizer: a thin ring, one wedge per light.
            const margin = 10;
            const outerR = Math.min(width, height) / 2 - margin;
            const innerR = outerR * 0.9;
            const cx = width / 2;
            const cy = height / 2;

            for (let i = 0; i < root.nLights; ++i) {
                const r = root.frame[i * 3] ?? 0;
                const g = root.frame[i * 3 + 1] ?? 0;
                const b = root.frame[i * 3 + 2] ?? 0;

                ctx.beginPath();
                ctx.arc(cx, cy, outerR, i / root.nLights * Math.PI * 2 - Math.PI / 2,
                        (i + 1) / root.nLights * Math.PI * 2 - Math.PI / 2);
                ctx.arc(cx, cy, innerR, (i + 1) / root.nLights * Math.PI * 2 - Math.PI / 2,
                        i / root.nLights * Math.PI * 2 - Math.PI / 2, true);
                ctx.closePath();
                ctx.fillStyle = "rgb(" + r + "," + g + "," + b + ")";
                ctx.fill();
            }
        }
    }

    Label {
        anchors.centerIn: parent
        text: qsTr("Dome preview")
        visible: root.nLights === 0
        color: Kirigami.Theme.disabledTextColor
    }

    // Redraw the ring whenever a new frame arrives.
    onFrameChanged: ringCanvas.requestPaint()
}
