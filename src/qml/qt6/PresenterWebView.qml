/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtWebView

// Loaded by PresenterWindow.qml, only in builds with Qt WebView (WEBVIEW_SUPPORT).
WebView {
    id: view

    // The page asked to close: its Close button sets the title to "cplay:close", as
    // WebView has no signal for window.close().
    signal closeRequested()

    onTitleChanged: {
        if (view.title === "cplay:close")
            view.closeRequested();
    }
}
