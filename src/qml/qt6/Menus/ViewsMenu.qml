/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Controls
import "../Components/PopupHelpers.js" as PopupHelpers
import org.ctoolbox.cplay

// The master view, the node windows, and the web views served by the HTTP server (presenter view,
// movie view). In the menu bar.
Menu {
    id: root

    title: qsTr("&Views")

    onOpened: PopupHelpers.handlePopupOpen()
    onClosed: PopupHelpers.handlePopupClose()

    MasterViewMenu {
    }
    Menu {
        title: qsTr("Node windows")
        icon.name: "window"

        MenuItem {
            action: actions.windowOpacityAction
            ToolTip {
                text: ConfigModel.nodeWindowTransparencySupported ?
                    "ON/OFF to have node windows visible." :
                    "Node window transparency/fading is unavailable: all node windows in the cluster configuration are fullscreen. Only windowed (non-fullscreen) node windows support fading."
            }
        }
        MenuItem {
            action: actions.windowOnTopAction
            ToolTip {
                text: ConfigModel.nodeWindowTransparencySupported ?
                    "ON/OFF to show C-Play node windows on top of other applications." :
                    "Node window on-top is unavailable: all node windows in the cluster configuration are fullscreen. Only windowed (non-fullscreen) node windows support the on-top feature."
            }
        }
        MenuSeparator {
            visible: NDI_SUPPORT
            height: visible ? implicitHeight : 0
        }
        MenuItem {
            checkable: true
            checked: window.showNdiOnNodes
            visible: NDI_SUPPORT
            enabled: ndiSender.enabled
            height: visible ? implicitHeight : 0
            text: qsTr("Show the master NDI output on all nodes")
            ToolTip {
                text: "The nodes render the \"C-Play\" NDI stream from the master instead of their own layers. The NDI output has to be ON."
            }
            onTriggered: window.showNdiOnNodes = !window.showNdiOnNodes
        }
    }
    MenuSeparator {
        visible: WEB_VIEWS_SUPPORT
        height: visible ? implicitHeight : 0
    }

    // The web views, only in builds with them (BUILD_CPLAY_WEB_VIEWS).
    Instantiator {
        id: webViews

        model: WEB_VIEWS_SUPPORT ? [
            { view: "presenter", title: qsTr("Presenter view (slides, notes)"), windowLoader: presenterWindowLoader },
            { view: "movie", title: qsTr("Movie view (playlist, sections, cues)"), windowLoader: movieWindowLoader }
        ] : []
        delegate: WEB_VIEWS_SUPPORT ? Qt.createComponent("WebViewMenu.qml") : null

        onObjectAdded: function(index, object) {
            const entry = webViews.model[index];
            object.view = entry.view;
            object.title = entry.title;
            object.windowLoader = entry.windowLoader;
            root.addMenu(object);
        }
        onObjectRemoved: function(index, object) { root.removeMenu(object) }
    }
}
