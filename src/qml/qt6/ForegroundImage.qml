/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import org.ctoolbox.cplay

Image {
    id: root

    width: parent.width
    height: window.isFullScreen() ? parent.height : parent.height - footer.height
    anchors.left: PlaylistSettings.position === "left" ? (playSections.visible ? playSections.right : playList.right) : (layers.visible ? layers.right : slides.right)
    anchors.right: PlaylistSettings.position === "right" ? (playList.visible ? playList.left : playSections.left) : (slides.visible ? slides.left : layers.left)
    anchors.top: parent.top

    source: playerController.foregroundImageFileUrl()
    fillMode: Image.PreserveAspectFit
    opacity: playerController.foregroundVisibility()

    Connections {
        target: playerController

        function onForegroundImageChanged(){
            root.source = playerController.foregroundImageFileUrl()
        }
        function onForegroundVisibilityChanged(){
            root.opacity = playerController.foregroundVisibility()
        }
    }
}
