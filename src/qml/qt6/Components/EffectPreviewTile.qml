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

// Effect gallery tile with a looping preview of the effect while shown.
AbstractButton {
    id: root

    property int preset: 0
    property string name: ""
    property bool intro: true
    property color accentColor: Kirigami.Theme.highlightColor
    // Effect progress 0..1 used by the preview drawing.
    property real progress: 1.0

    signal chosen()

    implicitWidth: Kirigami.Units.gridUnit * 5
    implicitHeight: Kirigami.Units.gridUnit * 5
    hoverEnabled: true
    onClicked: chosen()

    ToolTip.visible: hovered
    ToolTip.delay: 600
    ToolTip.text: name

    onProgressChanged: preview.requestPaint()

    SequentialAnimation {
        id: previewAnim
        running: root.visible
        loops: Animation.Infinite
        onStopped: root.progress = 1.0
        NumberAnimation { target: root; property: "progress"; from: 0; to: 1; duration: 900; easing.type: Easing.OutCubic }
        PauseAnimation { duration: 500 }
    }

    background: Rectangle {
        radius: Kirigami.Units.smallSpacing
        color: root.hovered ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.15)
                            : Kirigami.Theme.alternateBackgroundColor
        border.color: root.hovered ? root.accentColor : Qt.rgba(0.5, 0.5, 0.5, 0.3)
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: 2

        Canvas {
            id: preview
            Layout.fillWidth: true
            Layout.fillHeight: true

            function hash(i, j) {
                var s = Math.sin(i * 12.9898 + j * 78.233) * 43758.5453;
                return s - Math.floor(s);
            }

            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();
                var w = width, h = height;
                var bw = w * 0.6, bh = h * 0.6;
                var bx = (w - bw) / 2, by = (h - bh) / 2;
                // Visible amount: entrances reveal, exits hide.
                var v = root.intro ? root.progress : 1.0 - root.progress;
                var alpha = 1.0;
                var dx = 0, dy = 0, s = 1.0, rot = 0;

                ctx.save();
                switch (root.preset) {
                case 0: alpha = v > 0.5 ? 1 : 0; break;
                case 1: alpha = v; break;
                case 2: dx = -(1 - v) * w; break;
                case 3: dy = (1 - v) * h * 0.3; alpha = v; break;
                case 4: s = Math.max(0.01, v); alpha = v; break;
                case 5: rot = (1 - v) * 2 * Math.PI; alpha = v; break;
                case 6:
                    ctx.beginPath(); ctx.rect(bx, by, bw * v, bh); ctx.clip(); break;
                case 7:
                    ctx.beginPath(); ctx.arc(w / 2, h / 2, v * Math.sqrt(bw * bw + bh * bh) / 2, 0, 2 * Math.PI); ctx.clip(); break;
                case 8:
                    ctx.beginPath(); ctx.moveTo(w / 2, h / 2);
                    ctx.arc(w / 2, h / 2, Math.max(w, h), -Math.PI / 2, -Math.PI / 2 + v * 2 * Math.PI); ctx.closePath(); ctx.clip(); break;
                case 10:
                    s = 0.6; dx = (root.progress - 0.5) * w * 0.5; dy = -(root.progress - 0.5) * h * 0.35; break;
                default: break;
                }

                ctx.globalAlpha = Math.max(0, Math.min(1, alpha));
                ctx.translate(w / 2 + dx, h / 2 + dy);
                ctx.rotate(rot);
                ctx.scale(s, s);

                if (root.preset === 9) {
                    var cols = 8, rows = 6;
                    for (var i = 0; i < cols; i++) {
                        for (var j = 0; j < rows; j++) {
                            if (hash(i, j) < v) {
                                ctx.fillStyle = root.accentColor;
                                ctx.fillRect(-bw / 2 + i * bw / cols, -bh / 2 + j * bh / rows, bw / cols + 0.5, bh / rows + 0.5);
                            }
                        }
                    }
                } else {
                    var grad = ctx.createLinearGradient(-bw / 2, -bh / 2, bw / 2, bh / 2);
                    grad.addColorStop(0, root.accentColor);
                    grad.addColorStop(1, Qt.darker(root.accentColor, 1.8));
                    ctx.fillStyle = grad;
                    ctx.fillRect(-bw / 2, -bh / 2, bw, bh);
                }
                ctx.restore();
            }
        }

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            font.pointSize: 8
            text: root.name
        }
    }
}
