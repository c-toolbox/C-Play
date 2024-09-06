/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.15
import org.ctoolbox.cplay 1.0

Image {
    id: root

    anchors.left: PlaylistSettings.position === "left" ? (playSections.visible ? playSections.right : playList.right) : (layers.visible ? layers.right : slides.right)
    anchors.right: PlaylistSettings.position === "right" ? (playList.visible ? playList.left : playSections.left) : (slides.visible ? slides.left : layers.left)
    anchors.top: parent.top
    fillMode: Image.PreserveAspectFit
    height: window.isFullScreen() ? parent.height : parent.height - footer.height
    opacity: playerController.backgroundVisibility()
    source: playerController.backgroundImageFileUrl()
    width: parent.width

    Connections {
        function onBackgroundImageChanged() {
            root.source = playerController.backgroundImageFileUrl();
        }
        function onBackgroundVisibilityChanged() {
            root.opacity = playerController.backgroundVisibility();
        }

        target: playerController
    }
}
