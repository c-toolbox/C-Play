/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.12
import org.ctoolbox.cplay 1.0

Image {
    id: root

    width: parent.width
    height: window.isFullScreen() ? parent.height : parent.height - footer.height
    anchors.left: PlaylistSettings.position === "left" ? playSections.right : parent.left
    anchors.right: PlaylistSettings.position === "right" ? playList.left : parent.right
    anchors.top: parent.top

    source: playerController.backgroundImageFileUrl()
    fillMode: Image.PreserveAspectFit
    opacity: playerController.backgroundVisibility()

    Connections {
        target: playerController

        function onBackgroundImageChanged(){
            root.source = playerController.backgroundImageFileUrl()
        }
        function onBackgroundVisibilityChanged(){
            root.opacity = playerController.backgroundVisibility()
        }
    }
}
