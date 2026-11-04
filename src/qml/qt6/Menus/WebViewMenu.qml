/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Controls

// Opens a web view served by the HTTP server: the presenter view (web/presentation_mode) or the
// movie view (web/movie_mode), shows the screen it is open on, and closes it. Used by
// ViewsMenu.qml, only in builds with the web views.
Menu {
    id: root

    // The path the view is served at: "presenter" or "movie".
    property string view: "presenter"
    // Loader of the PresenterWindow that shows the view (WEBVIEW_SUPPORT).
    property var windowLoader
    readonly property bool serverRunning: playerController.presenter.serverRunning
    // The screen the view is open on, in the PresenterWindow or a browser app window; -1 when
    // it is not open there (views opened in the default browser are not tracked).
    readonly property int openScreen: {
        const window = windowLoader ? windowLoader.item : null;
        if (window && window.screenIndex >= 0)
            return window.screenIndex;
        const browserScreen = playerController.presenter.browserViews[view];
        return browserScreen === undefined ? -1 : browserScreen;
    }
    readonly property bool isOpen: openScreen >= 0

    function closeView() {
        if (windowLoader && windowLoader.item)
            windowLoader.item.close();
        playerController.presenter.closeInBrowser(view);
    }

    Instantiator {
        model: Qt.application.screens

        delegate: MenuItem {
            enabled: root.serverRunning
            checkable: true
            checked: root.openScreen === index
            text: qsTr("Open on %1 (%2x%3)").arg(modelData.name).arg(modelData.width).arg(modelData.height)

            onTriggered: {
                if (WEBVIEW_SUPPORT)
                    root.windowLoader.item.openOn(index);
                else
                    playerController.presenter.openInBrowser(index, root.view);
                // Clicking toggles checked, which would break the binding.
                checked = Qt.binding(() => root.openScreen === index);
            }
        }

        onObjectAdded: function(index, object) { root.insertItem(index, object) }
        onObjectRemoved: function(index, object) { root.removeItem(object) }
    }
    MenuItem {
        enabled: root.isOpen
        text: qsTr("Close")

        onTriggered: root.closeView()
    }
    MenuSeparator {}
    MenuItem {
        enabled: root.serverRunning
        text: qsTr("Open in default browser")

        onTriggered: Qt.openUrlExternally(playerController.presenter.localUrlOf(root.view))
    }
    Menu {
        id: addressMenu

        enabled: root.serverRunning
        title: qsTr("Copy address for other devices")

        Instantiator {
            model: root.visible ? playerController.presenter.networkUrls(root.view) : []

            delegate: MenuItem {
                text: modelData

                onTriggered: app.copyToClipboard(modelData)
            }

            onObjectAdded: function(index, object) { addressMenu.insertItem(index, object) }
            onObjectRemoved: function(index, object) { addressMenu.removeItem(object) }
        }
    }
    MenuItem {
        enabled: false
        visible: !root.serverRunning
        height: visible ? implicitHeight : 0
        text: qsTr("The HTTP server is off (data/http-server-conf.json)")
    }
}
