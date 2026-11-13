// Helpers to open the tool windows and dialogs (Kirigami.ApplicationWindow / Window).
//
// Setting "visible = true" alone does nothing when the window is already visible. A window can
// be minimized (owned windows have no taskbar button on Windows, so a minimized one sits as a
// small title bar in a screen corner, often behind the taskbar or the fullscreen main window),
// behind other windows, or on a screen that has since been removed or reconfigured. It then
// stays out of sight however often it is opened from the menu, until C-Play is restarted.

// QWindow::Visibility values (the Window enum is not reachable from a JS resource).
var visibilityHidden = 0;
var visibilityMinimized = 3;

function isOnAScreen(w) {
    var screens = Qt.application.screens;
    // Require a reachable part of the title bar, not just a sliver of the window, to be on a screen.
    var margin = 40;
    for (var i = 0; i < screens.length; ++i) {
        var s = screens[i];
        if (w.x + w.width - margin > s.virtualX && w.x + margin < s.virtualX + s.width
                && w.y + margin > s.virtualY && w.y < s.virtualY + s.height - margin)
            return true;
    }
    return false;
}

function centerOn(w, anchorWindow) {
    if (anchorWindow && anchorWindow !== w && isOnAScreen(anchorWindow)) {
        w.x = anchorWindow.x + (anchorWindow.width - w.width) / 2;
        w.y = Math.max(anchorWindow.y, anchorWindow.y + (anchorWindow.height - w.height) / 2);
        return;
    }
    var s = Qt.application.screens[0];
    if (!s)
        return;
    w.x = s.virtualX + Math.max(0, (s.width - w.width) / 2);
    w.y = s.virtualY + Math.max(0, (s.height - w.height) / 2);
}

// Shows the window, restoring it when minimized, moving it back on screen when it is outside
// every screen, and bringing it to the front.
function showWindow(w) {
    if (!w)
        return;
    // "window" is the main application window (main.qml) in the context of the importing component.
    var anchorWindow = (typeof window !== "undefined") ? window : null;

    if (w.visibility === visibilityMinimized) {
        w.showNormal();
    } else {
        w.show();
        // A window hidden while minimized can come back minimized.
        if (w.visibility === visibilityMinimized)
            w.showNormal();
    }
    if (!isOnAScreen(w))
        centerOn(w, anchorWindow);
    w.raise();
    w.requestActivate();
}

// For a window that is open and in sight: hides it. Otherwise (hidden or minimized): shows it.
function toggleWindow(w) {
    if (!w)
        return;
    if (w.visible && w.visibility !== visibilityMinimized && w.visibility !== visibilityHidden)
        w.visible = false;
    else
        showWindow(w);
}

// Toggles the window from a checkable button whose "checked" follows the window's visibility.
// Clicking a checkable button assigns "checked", which breaks a "checked: w.visible" binding,
// so a window later closed with its title bar button would leave the button checked and the
// next click would do nothing. The binding is restored here.
function toggleWindowFromButton(button, w) {
    toggleWindow(w);
    if (button)
        button.checked = Qt.binding(function () { return w.visible; });
}
