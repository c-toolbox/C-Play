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
        recentMediaFilesMenuInstantiator.model = mpv.recentMediaFiles;
        recentPlaylistsMenuInstantiator.model = mpv.recentPlaylists;
    }

    MenuItem {
        action: actions["openAction"]
    }
    Menu {
        id: recentMediaFilesMenu

        title: qsTr("Open Recent Media Files")
        icon.name: "document-open-recent"

        Instantiator {
            id: recentMediaFilesMenuInstantiator

            model: 0

            delegate: MenuItem {
                text: modelData

                onTriggered: mpv.loadFile(modelData)
            }

            onObjectAdded: function(index, object) {
                recentMediaFilesMenu.insertItem(index, object)
            }
            onObjectRemoved: function(index, object) {
                recentMediaFilesMenu.removeItem(object)
            }
        }
        Connections {
            function onRecentMediaFilesChanged() {
                recentMediaFilesMenuInstantiator.model = mpv.recentMediaFiles;
            }

            target: mpv
        }
        MenuSeparator {
        }
        MenuItem {
            action: actions["clearRecentMediaFilesAction"]
        }
    }
    Menu {
        id: recentPlaylistsMenu

        title: qsTr("Open Recent Playlists")
        icon.name: "format-list-unordered"

        Instantiator {
            id: recentPlaylistsMenuInstantiator

            model: 0

            delegate: MenuItem {
                text: modelData

                onTriggered: mpv.loadFile(modelData)
            }

            onObjectAdded: function(index, object) {
                recentPlaylistsMenu.insertItem(index, object)
            }
            onObjectRemoved: function(index, object) {
                recentPlaylistsMenu.removeItem(object)
            }
        }
        Connections {
            function onRecentPlaylistsChanged() {
                recentPlaylistsMenuInstantiator.model = mpv.recentPlaylists;
            }

            target: mpv
        }
        MenuSeparator {
        }
        MenuItem {
            action: actions["clearRecentPlaylistsAction"]
        }
    }
    MenuSeparator {
    }
    MenuItem {
        action: actions["saveAsCPlayFileAction"]
    }
    //MenuItem { action: actions["openUrlAction"] }

    MenuSeparator {
    }
    MenuItem {
        action: actions["quitApplicationAction"]
    }
}
