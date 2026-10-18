/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQml.Models
import QtQuick
import QtQuick.Controls
import "../Components/PopupHelpers.js" as PopupHelpers

Menu {
    id: root

    title: qsTr("&File")

    onOpened: {
        PopupHelpers.handlePopupOpen();
        recentMediaFilesMenuInstantiator.model = mpv.recentMediaFiles;
        recentPlaylistsMenuInstantiator.model = mpv.recentPlaylists;
    }

    onClosed: {
        PopupHelpers.handlePopupClose();
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

            model: mpv.recentMediaFiles;

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

            model: mpv.recentPlaylists;

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

    Menu {
        id: recentPresentationsMenu

        title: qsTr("Open Recent Presentations")
        icon.name: "document-open"

        Instantiator {
            id: recentPresentationsMenuInstantiator

            model: app.slides.recentPresentations;

            delegate: MenuItem {
                text: modelData

                onTriggered: {
                    slides.presentationToLoad = modelData;
                    slides.openCPlayPresentation();
                }
            }

            onObjectAdded: function(index, object) {
                recentPresentationsMenu.insertItem(index, object)
            }
            onObjectRemoved: function(index, object) {
                recentPresentationsMenu.removeItem(object)
            }
        }
        Connections {
            function onRecentPresentationsChanged() {
                recentPresentationsMenuInstantiator.model = app.slides.recentPresentations;
            }

            target: app.slides
        }
        MenuSeparator {
        }
        MenuItem {
            action: actions["clearRecentPresentationsAction"]
        }
    }

    // Autosaves of edited presentations, playlists and C-Play files, kept next to the
    // application settings so the loaded files are never overwritten.
    Menu {
        id: autosavedFilesMenu

        title: qsTr("Open Autosaved")
        icon.name: "document-save"

        // app.autosavedFiles() grouped by type, each group under a heading.
        function autosavedFilesModel() {
            const files = app.autosavedFiles();
            if (files.length === 0)
                return [{ heading: true, text: qsTr("No autosaved files") }];
            const headings = { "cplaypres": qsTr("Presentations"), "cplaylist": qsTr("Playlists"), "cplayfile": qsTr("C-Play Files") };
            const result = [];
            let type = "";
            for (const file of files) {
                if (file.type !== type) {
                    type = file.type;
                    result.push({ heading: true, text: headings[type] });
                }
                result.push({ heading: false, text: file.text, type: file.type, path: file.path });
            }
            return result;
        }

        onAboutToShow: autosavedFilesMenuInstantiator.model = autosavedFilesModel()

        Instantiator {
            id: autosavedFilesMenuInstantiator

            model: []

            delegate: MenuItem {
                text: modelData.text
                enabled: !modelData.heading
                font.bold: modelData.heading

                onTriggered: {
                    if (modelData.type === "cplaypres") {
                        slides.presentationToLoad = modelData.path;
                        slides.openCPlayPresentation();
                    }
                    else {
                        mpv.loadFile(modelData.path);
                    }
                }
            }

            onObjectAdded: function(index, object) {
                autosavedFilesMenu.insertItem(index, object)
            }
            onObjectRemoved: function(index, object) {
                autosavedFilesMenu.removeItem(object)
            }
        }
        MenuSeparator {
        }
        MenuItem {
            text: qsTr("Clear Autosaved Files")
            icon.name: "edit-clear-history"

            onTriggered: {
                app.clearAutosavedFiles();
                autosavedFilesMenuInstantiator.model = [];
            }
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
