/*
 * SPDX-FileCopyrightText:
 * 2021-2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import org.ctoolbox.cplay

Image {
    id: root

    anchors.left: (window.hideUI ? parent.left : PlaylistSettings.position === "left" ? (playSections.visible ? playSections.right : playList.right) : (layers.visible ? layers.right : slides.right))
    anchors.right: (window.hideUI ? parent.right : PlaylistSettings.position === "right" ? (playList.visible ? playList.left : playSections.left) : (slides.visible ? slides.left : layers.left))
    anchors.top: parent.top
    fillMode: Image.PreserveAspectFit
    height: footer.visible ? parent.height - footer.height : parent.height
    opacity: playerController.backgroundVisibility()
    source: playerController.backgroundMode() === 0 ? playerController.backgroundImageFileUrl() : ""
    width: parent.width

    // Shown instead of the image in background color mode
    Rectangle {
        id: backgroundColorRect

        anchors.fill: parent
        color: playerController.backgroundColor()
        visible: playerController.backgroundMode() === 1
    }

    Connections {
        function onBackgroundImageChanged() {
            root.source = playerController.backgroundMode() === 0 ? playerController.backgroundImageFileUrl() : "";
        }
        function onBackgroundModeChanged() {
            root.source = playerController.backgroundMode() === 0 ? playerController.backgroundImageFileUrl() : "";
            backgroundColorRect.visible = playerController.backgroundMode() === 1;
        }
        function onBackgroundColorChanged() {
            backgroundColorRect.color = playerController.backgroundColor();
        }
        function onBackgroundVisibilityChanged() {
            root.opacity = playerController.backgroundVisibility();
        }

        target: playerController
    }
}
