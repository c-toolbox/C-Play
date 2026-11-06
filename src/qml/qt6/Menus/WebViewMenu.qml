/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Controls
import org.ctoolbox.cplay

// Opens a web view served by the HTTP server: the presenter view (web/presentation_mode), the
// movie view (web/movie_mode) or the layer view (web/layer_mode), shows the screen it is open on,
// and closes it. Used by ViewsMenu.qml, only in builds with the web views.
Menu {
    id: root

    // The path the view is served at: "presenter", "movie" or "layerview".
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
    // The layer view: what the flat layers are moved on (PresentationSettings.layerViewCanvas).
    readonly property var canvases: [
        { text: qsTr("Fisheye (dome)"), tip: qsTr("Azimuth and elevation on the fisheye image of the dome, as the master view fisheye camera shows it.") },
        { text: qsTr("Flat"), tip: qsTr("Horizontal and vertical move in cm, within the plane move ranges of the Grid settings.") },
        { text: qsTr("Sphere"), tip: qsTr("Azimuth and elevation on an equirectangular map of the whole sphere.") },
        { text: qsTr("Circular screen"), tip: qsTr("Degrees around and height on the circular screen of the circular surface mapping (Grid settings).") }
    ]
    // The chosen canvas, shown in the title of its menu; empty for the other views.
    readonly property string canvasName: view === "layerview" ? (canvases[PresentationSettings.layerViewCanvas] || canvases[0]).text : ""
    // Items put above the screens (the canvas menu and its separator).
    property int headerItems: 0

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

        onObjectAdded: function(index, object) { root.insertItem(index + root.headerItems, object) }
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

    // The layer view: what the flat layers are moved on, first in the menu with the choice in its title.
    Component {
        id: separatorComponent

        MenuSeparator {}
    }
    Instantiator {
        model: root.view === "layerview" ? 1 : 0

        delegate: Menu {
            id: canvasMenu

            title: qsTr("Move layers on: %1").arg(root.canvasName)

            Instantiator {
                model: root.canvases

                delegate: MenuItem {
                    checkable: true
                    checked: PresentationSettings.layerViewCanvas === index
                    text: modelData.text
                    ToolTip.text: modelData.tip
                    ToolTip.visible: hovered
                    ToolTip.delay: 500

                    onTriggered: {
                        PresentationSettings.layerViewCanvas = index;
                        PresentationSettings.save();
                        // Clicking toggles checked, which would break the binding.
                        checked = Qt.binding(() => PresentationSettings.layerViewCanvas === index);
                    }
                }

                onObjectAdded: function(index, object) { canvasMenu.insertItem(index, object) }
                onObjectRemoved: function(index, object) { canvasMenu.removeItem(object) }
            }
        }

        onObjectAdded: function(index, object) {
            root.insertMenu(0, object);
            root.insertItem(1, separatorComponent.createObject(null));
            root.headerItems = 2;
        }
        onObjectRemoved: function(index, object) {
            root.removeItem(root.itemAt(1));
            root.removeMenu(object);
            root.headerItems = 0;
        }
    }
}
