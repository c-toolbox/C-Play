/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQml.Models
import QtQuick
import QtQuick.Controls

Menu {
    id: root

    title: qsTr("&File")
    onOpened: {
        recentMediaFilesMenuInstantiator.model = mpv.recentMediaFiles
        recentPlaylistsMenuInstantiator.model = mpv.recentPlaylists
    }

    MenuItem { action: actions["openAction"] }

    Menu {
        id: recentMediaFilesMenu

        title: qsTr("Open Recent Media Files")

        Instantiator {
            id: recentMediaFilesMenuInstantiator
            model: 0
            onObjectAdded: recentMediaFilesMenu.insertItem( index, object )
            onObjectRemoved: recentMediaFilesMenu.removeItem( object )
            delegate: MenuItem {
                text: modelData
                onTriggered: mpv.loadFile(modelData)
            }
        }

        Connections {
            target: mpv
            function onRecentMediaFilesChanged() {
                recentMediaFilesMenuInstantiator.model = mpv.recentMediaFiles
            }
        }

        MenuSeparator {}
        MenuItem { action: actions["clearRecentMediaFilesAction"] }
    }
    Menu {
        id: recentPlaylistsMenu

        title: qsTr("Open Recent Playlists")

        Instantiator {
            id: recentPlaylistsMenuInstantiator
            model: 0
            onObjectAdded: recentPlaylistsMenu.insertItem( index, object )
            onObjectRemoved: recentPlaylistsMenu.removeItem( object )
            delegate: MenuItem {
                text: modelData
                onTriggered: mpv.loadFile(modelData)
            }
        }

        Connections {
            target: mpv
            function onRecentPlaylistsChanged() {
                recentPlaylistsMenuInstantiator.model = mpv.recentPlaylists
            }
        }


        MenuSeparator {}
        MenuItem { action: actions["clearRecentPlaylistsAction"] }
    }

    MenuSeparator {}

    MenuItem { action: actions["saveAsCPlayFileAction"] }
    //MenuItem { action: actions["openUrlAction"] }

    MenuSeparator {}

    MenuItem { action: actions["quitApplicationAction"] }
}
