/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Controls
import "../Components/PopupHelpers.js" as PopupHelpers

Menu {
    id: root

    title: qsTr("&Settings")

    onOpened: PopupHelpers.handlePopupOpen()
    onClosed: PopupHelpers.handlePopupClose()

    Menu {
        title: "States"
        Menu {
            title: qsTr("Master view")

            icon.name: window.mainViewMode === 0 ? "map-flat" :
                      window.mainViewMode === 1 ? "map-globe" : "draw-halfcircle3"
            icon.color: window.mainViewMode === 0 ? "crimson" :
                        window.mainViewMode === 1 ? "lime" : "lightblue"

            MenuItem {
                checkable: true
                checked: window.mainViewMode === 0
                text: qsTr("Render only the main video")
                onTriggered: window.mainViewMode = 0
            }
            MenuItem {
                checkable: true
                checked: window.mainViewMode === 1
                text: qsTr("Render 3D view with perspective camera")
                onTriggered: window.mainViewMode = 1
            }
            MenuItem {
                checkable: true
                checked: window.mainViewMode === 2
                text: qsTr("Render 3D view with a fisheye camera")
                onTriggered: window.mainViewMode = 2
            }
            MenuSeparator {
                visible: NDI_SUPPORT
                height: visible ? implicitHeight : 0
            }
            MenuItem {
                checkable: true
                checked: window.showNdiOnNodes
                visible: NDI_SUPPORT
                height: visible ? implicitHeight : 0
                text: qsTr("Show the master NDI output on all nodes")
                onTriggered: window.showNdiOnNodes = !window.showNdiOnNodes
                ToolTip {
                    text: "The nodes render the NDI stream from the master instead of their own layers. The NDI output has to be ON."
                }
            }
        }
        MenuItem {
            action: actions.windowOpacityAction
            ToolTip {
                text: "ON/OFF to have node windows visible."
            }
        }
        MenuItem {
            action: actions.windowOnTopAction
            ToolTip {
                text: "ON/OFF to sync state from master to clients."
            }
        }
        MenuItem {
            action: actions.syncAction
            ToolTip {
                text: "ON/OFF to sync state from master to clients."
            }
        }
        MenuItem {
            action: actions.ndiSenderAction
            visible: ndiSender.available
            height: visible ? implicitHeight : 0
            ToolTip {
                text: "ON/OFF to send the player as an NDI source."
            }
        }
    }
    MenuSeparator {
    }
    MenuItem {
        text: qsTr("REST Commands Editor...")
        icon.name: "document-send"
        onTriggered: {
            restCommandsEditor.visible = true;
        }
    }
    MenuItem {
        text: qsTr("Logging...")
        icon.name: "console"
        onTriggered: {
            loggingWindow.visible = true;
        }
    }
    MenuSeparator {
    }
    MenuItem {
        action: actions["configureShortcutsAction"]
    }
    MenuSeparator {
    }
    MenuItem {
        action: actions["configureAction"]
    }
}
