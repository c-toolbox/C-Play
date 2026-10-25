/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Controls
import "../Components/PopupHelpers.js" as PopupHelpers
import org.ctoolbox.cplay

Menu {
    id: root

    title: qsTr("&Settings")

    onOpened: PopupHelpers.handlePopupOpen()
    onClosed: PopupHelpers.handlePopupClose()

    Menu {
        title: "States"
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
        text: qsTr("C-Lux Editor...")
        icon.name: "preferences-desktop-color"
        // C-Lux support is a build option (BUILD_CPLAY_WITH_CLUX); the client does not exist on app without it.
        visible: CLUX_SUPPORT
        height: visible ? implicitHeight : 0
        onTriggered: {
            cluxEditorLoader.item.visible = true;
        }
    }
    MenuItem {
        text: qsTr("MediaMTX Streams...")
        icon.name: "network-server"
        // MediaMTX support is a build option (BUILD_CPLAY_WITH_MEDIA_MTX); the models do not exist on app without it.
        visible: !!app.mediaMtxServersModel && !!app.mediaMtxModel
        height: visible ? implicitHeight : 0
        onTriggered: {
            mediaMtxEditor.visible = true;
        }
    }
    MenuItem {
        text: qsTr("VDO.Ninja Arena Rooms...")
        icon.name: "im-user-online"
        // VDO.Ninja rooms feed WebRTC layers, a build option (BUILD_CPLAY_WITH_WEBRTC); the models do not exist on app without it.
        visible: !!app.vdoNinjaRoomsModel && !!app.vdoNinjaSlotsModel
        height: visible ? implicitHeight : 0
        onTriggered: {
            vdoNinjaRoomsEditor.visible = true;
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
