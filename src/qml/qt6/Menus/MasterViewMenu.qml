/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Controls
import "../Components/PopupHelpers.js" as PopupHelpers

// How the master view is rendered. A submenu of ViewsMenu.qml, and opened by the master view
// button in the header.
Menu {
    id: root

    title: qsTr("Master view")

    icon.name: window.mainViewMode === 0 ? "map-flat" :
              window.mainViewMode === 1 ? "map-globe" :
              window.mainViewMode === 2 ? "draw-halfcircle3" : "draw-circle"
    icon.color: window.mainViewMode === 0 ? "crimson" :
                window.mainViewMode === 1 ? "lime" :
                window.mainViewMode === 2 ? "lightblue" : "orange"

    onOpened: PopupHelpers.handlePopupOpen()
    onClosed: PopupHelpers.handlePopupClose()

    MenuItem {
        checkable: true
        checked: window.mainViewMode === 0
        text: qsTr("Show only the main video")
        ToolTip {
            text: "The standard flat playback view with just the main media."
        }
        onTriggered: window.mainViewMode = 0
    }
    MenuItem {
        checkable: true
        checked: window.mainViewMode === 1
        text: qsTr("Show 3D view with perspective camera")
        ToolTip {
            text: "All layers rendered together in the interactive 3D scene."
        }
        onTriggered: window.mainViewMode = 1
    }
    MenuItem {
        checkable: true
        checked: window.mainViewMode === 2
        text: qsTr("Show 3D view with fisheye camera")
        ToolTip {
            text: "All layers rendered through a virtual 180-degree fisheye lens centered on the zenith."
        }
        onTriggered: window.mainViewMode = 2
    }
    MenuItem {
        checkable: true
        checked: window.mainViewMode === 3
        text: qsTr("Show 3D view mapped on a sphere")
        ToolTip {
            text: "All layers mapped on a sphere seen from outside. Drag to rotate the sphere, scroll to zoom and double-click to reset."
        }
        onTriggered: window.mainViewMode = 3
    }
}
