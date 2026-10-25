/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts

import org.kde.kirigami as Kirigami

// A web view served by the HTTP server fullscreen on a screen: the presenter view
// (web/presentation_mode) or the movie view (web/movie_mode, view: "movie").
// Needs Qt WebView; without it C-Play opens the view in a browser app window instead.
Window {
    id: root

    // The path the view is served at: "presenter" or "movie".
    property string view: "presenter"

    title: view === "movie" ? qsTr("C-Play Movie View") : qsTr("C-Play Presenter View")
    color: "black"
    width: 1280
    height: 800

    function openOn(screenIndex) {
        const screens = Qt.application.screens;
        const target = screens[Math.max(0, Math.min(screenIndex, screens.length - 1))];
        // Leave fullscreen first, so the window can move to the other screen.
        root.visibility = Window.Windowed;
        root.screen = target;
        root.x = target.virtualX + (target.width - root.width) / 2;
        root.y = target.virtualY + (target.height - root.height) / 2;
        root.visibility = Window.FullScreen;
        root.raise();
        root.requestActivate();
        if (webViewLoader.item)
            webViewLoader.item.reload();
    }

    onClosing: webViewLoader.active = false
    onVisibleChanged: if (visible) webViewLoader.active = true

    Shortcut {
        sequence: "F11"
        onActivated: root.visibility = root.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen
    }
    Shortcut {
        sequences: ["Ctrl+W", "Ctrl+Q"]
        onActivated: root.close()
    }

    Loader {
        id: webViewLoader

        anchors.fill: parent
        active: false
        visible: WEBVIEW_SUPPORT && playerController.presenter.serverRunning
        source: WEBVIEW_SUPPORT ? Qt.resolvedUrl("PresenterWebView.qml") : ""

        onLoaded: {
            item.url = playerController.presenter.localUrlOf(root.view) + "?host=cplay";
            item.closeRequested.connect(root.close);
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        visible: !webViewLoader.visible
        spacing: Kirigami.Units.largeSpacing

        Label {
            Layout.alignment: Qt.AlignHCenter
            color: "white"
            text: !playerController.presenter.serverRunning
                  ? qsTr("This view needs the HTTP server. Set \"run\" to \"yes\" in data/http-server-conf.json and restart C-Play.")
                  : qsTr("This C-Play is built without Qt WebView.")
        }
        Button {
            Layout.alignment: Qt.AlignHCenter
            visible: playerController.presenter.serverRunning
            text: qsTr("Open in browser window")
            onClicked: {
                playerController.presenter.openInBrowser(Qt.application.screens.indexOf(root.screen), root.view);
                root.close();
            }
        }
        Button {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Close")
            onClicked: root.close()
        }
    }
}
