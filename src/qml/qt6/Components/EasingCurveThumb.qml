/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick

import org.kde.kirigami as Kirigami

// Small plot of an easing curve, sampled from the C++ easing functions.
Canvas {
    id: root

    property var slideModel: null
    property int easing: 0
    property color strokeColor: Kirigami.Theme.highlightColor

    onEasingChanged: requestPaint()
    onSlideModelChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    onPaint: {
        var ctx = getContext("2d");
        ctx.clearRect(0, 0, width, height);
        ctx.strokeStyle = Qt.rgba(0.5, 0.5, 0.5, 0.4);
        ctx.lineWidth = 1;
        ctx.strokeRect(0.5, 0.5, width - 1, height - 1);
        if (!slideModel || easing < 0)
            return;
        // Leave headroom for overshooting curves (back/elastic).
        var top = height * 0.2;
        var span = height * 0.6;
        ctx.strokeStyle = strokeColor;
        ctx.lineWidth = 1.5;
        ctx.beginPath();
        var steps = Math.max(16, Math.round(width));
        for (var i = 0; i <= steps; i++) {
            var t = i / steps;
            var v = slideModel.easingValue(easing, t);
            var x = t * (width - 2) + 1;
            var y = top + span - v * span;
            if (i === 0)
                ctx.moveTo(x, y);
            else
                ctx.lineTo(x, y);
        }
        ctx.stroke();
    }
}
