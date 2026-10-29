/*
 * SPDX-FileCopyrightText:
 * 2021-2026 Erik Sunden <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick.Window
import QtQuick.Layouts
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick3D
import QtQuick3D.Helpers

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

import "Menus"
import "Settings"
import "Components/PopupHelpers.js" as PopupHelpers

Kirigami.ApplicationWindow {
    id: window

    // Let Qt position the content below both the menu bar and header. Newer
    // Kirigami versions anchor it to the window with only the header's height
    // as a top margin, which puts the list toolbars behind the header.
    contentItem.anchors.fill: undefined
    contentItem.anchors.left: contentItem.parent.left
    contentItem.anchors.right: contentItem.parent.right
    contentItem.anchors.topMargin: 0
    contentItem.anchors.bottomMargin: 0

    property var appActions: actions.list
    property var configure: app.action("configure")
    property bool isFullScreenMode: false
    property bool isIdleMode: false
    property bool hideUI: (isFullScreenMode || isIdleMode)
    property url newMediaFileToOpen: ""
    property bool forceClose: false
    // Which list the removeSelected shortcut (Delete) acts on: the one clicked last
    // ("layer", "slide", "playlist" or "section").
    property string removeShortcutTarget: "layer"
    // Lists with removals that the undoRemove shortcut (Ctrl+Z) can put back, latest last:
    // "slides" (slides and layers), "playlist" or "sections". Each entry is one removal step
    // of that list's model.
    property var removalUndoOrder: []

    // Called after a removal that the list's model recorded as an undo step.
    function recordRemoval(list) {
        removalUndoOrder.push(list);
        // The models keep at most 50 steps each.
        if (removalUndoOrder.length > 150)
            removalUndoOrder.shift();
    }
    // Puts back the latest removal in any list. Steps a model no longer has (another
    // presentation, playlist or media file was loaded since) are skipped.
    function undoLastRemoval() {
        while (removalUndoOrder.length > 0) {
            const list = removalUndoOrder.pop();
            let restored = false;
            if (list === "slides")
                restored = app.slides.undoRemove();
            else if (list === "playlist")
                restored = mpv.playlistModel.undoRemove();
            else if (list === "sections")
                restored = mpv.playSectionsModel.undoRemove();
            if (restored)
                return true;
        }
        osd.message(qsTr("Nothing to undo"));
        return false;
    }

    // How a left click on a list item marks rows: "toggle" (default Ctrl) adds or removes one row,
    // "range" (default Shift) marks every row between the anchor and the clicked one, "" is a plain
    // click. The modifiers are set in the shortcut editor; when both are held the more specific wins.
    function markClickType() {
        const toggle = app.heldActionModifierCount("listMarkToggle");
        const range = app.heldActionModifierCount("listMarkRange");
        if (range > 0 && range >= toggle)
            return "range";
        if (toggle > 0)
            return "toggle";
        return "";
    }
    // Returns a copy of marked with idx toggled. The first mark also marks the current row,
    // so selecting a row and then modifier-clicking another marks both.
    function toggleMarkedIndex(marked, idx, current) {
        const result = marked.slice();
        if (result.length === 0 && current >= 0 && current !== idx)
            result.push(current);
        const pos = result.indexOf(idx);
        if (pos >= 0)
            result.splice(pos, 1);
        else
            result.push(idx);
        return result;
    }
    // All rows from anchor to idx, either direction. Without an anchor only idx is marked.
    function markedRange(anchor, idx) {
        if (anchor < 0)
            return [idx];
        const result = [];
        for (let r = Math.min(anchor, idx); r <= Math.max(anchor, idx); r++)
            result.push(r);
        return result;
    }
    // Rows a remove acts on: the marked ones, or else the current one. Highest first, so
    // removing one row does not shift the rows still to be removed.
    function rowsToRemove(marked, current, count) {
        const rows = marked.length > 0 ? marked : [current];
        return rows.filter(r => r >= 0 && r < count).sort((a, b) => b - a);
    }
    // Moves the marked rows (or else the current row) one step up (dir -1) or down (dir 1) by
    // calling moveFn(row), keeping their order. A row stops at the list end or behind a row that
    // could not move. Returns the new positions of the marked rows ([] when none are marked).
    function moveMarkedRows(marked, current, count, dir, moveFn) {
        const rows = (marked.length > 0 ? marked : [current])
            .filter(r => r >= 0 && r < count)
            .sort((a, b) => dir < 0 ? a - b : b - a);
        let limit = dir < 0 ? 0 : count - 1;
        const moved = [];
        for (const r of rows) {
            const to = r === limit ? r : r + dir;
            if (to !== r)
                moveFn(r);
            moved.push(to);
            limit = to - dir;
        }
        return marked.length > 0 ? moved : [];
    }
    property bool uiPopupOpen: false  // True when any popup (Menu, ComboBox dropdown or Dialog) is open in this ApplicationWindow

    // How the central viewport renders its content:
    //   0 = render only the main video (standard flat playback view)
    //   1 = render all layers with a perspective camera (3D view)
    //   2 = render all layers as a 180-degree fisheye (fulldome)
    //   3 = render all layers mapped on a sphere seen from outside (dragging rotates the sphere)
    // The UserInterfaceSettings settings decide which mode is active at startup; afterwards the
    // globe menu in the header taskbar (and Settings > States) changes it during playback.
    property int mainViewMode: 0

    // When on, the nodes render the "C-Play" NDI stream from the master instead of their own
    // layers, following whatever the master view currently shows.
    property bool showNdiOnNodes: false

    // Keep the NDI output in sync with the master view state, so it publishes the 3D view
    // instead of the main video whenever the 3D view is the one being shown.
    onMainViewModeChanged: {
        ndiSender.mainViewMode = window.mainViewMode;
        updateNdiOnNodes();
    }

    onShowNdiOnNodesChanged: updateNdiOnNodes()

    function updateNdiOnNodes() {
        playerController.masterNdiOnNodes = window.showNdiOnNodes ? window.mainViewMode + 1 : 0;
    }

    Timer {
        id: closeTimer
        interval: 1000
        running: false
        repeat: false

        onTriggered: {
            window.close() // Closes the target window
        }
    }

    onClosing: function(close) {
        if (!forceClose && (LoggingSettings.generalLoggingEnabled || LoggingSettings.performanceMetricsEnabled || LoggingSettings.nodeTelemetryEnabled)) {
            // Don't close yet; ask whether to turn logging off for next start. The nodes keep
            // running until the user has answered (see Application::quitApp).
            close.accepted = false;
            forceClose = true;
            loggingCloseDialog.open();
            return;
        }
        app.sendQuitToNodes();
    }

    function openMediaFile(path, startPlayback, loadSiblings) {
        mpv.pause = true;
        mpv.position = 0;
        if (loadSiblings) {
            // get video files from same folder as the opened file
            mpv.playlistModel.getVideos(path);
        }
        mpv.loadFile(path);
    }

    function saveCPlayFile(path) {
        if (mpv.playSectionsModel.isEmpty() || !mpv.playSectionsModel.currentEditItem)
            return;
        mpv.playSectionsModel.currentEditItem.saveAsJSONPlayFile(path);
        mpv.playSectionsModel.setCurrentEditItemIsEdited(false);
    }

    function saveCPlayPlaylist(path) {
        mpv.playlistModel.saveAsJSONPlaylist(path);
        mpv.playlistModelChanged();
    }

    // Builds a camera rotation quaternion from Euler angles (degrees around the X, Y and Z
    // axes). Qt 6.10's QML "Qt" object only provides Qt.quaternion(scalar, x, y, z) - there is
    // no Qt.quaternionFromEulerAngles() - so this mirrors QQuaternion::fromEulerAngles() from
    // qtbase/src/gui/math3d/qquaternion.cpp exactly. It is the inverse of toEulerAngles(),
    // which is what the "Get current position & rotation" button saves, so a captured pose
    // round-trips through save/restore unchanged.
    function cameraRotationFromEuler(xDeg, yDeg, zDeg) {
        const r = Math.PI / 180;
        const p = xDeg * r * 0.5;   // half angle around X (pitch)
        const y = yDeg * r * 0.5;   // half angle around Y (yaw)
        const z = zDeg * r * 0.5;   // half angle around Z (roll)

        const c1 = Math.cos(y), s1 = Math.sin(y);
        const c2 = Math.cos(z), s2 = Math.sin(z);
        const c3 = Math.cos(p), s3 = Math.sin(p);

        return Qt.quaternion(
            c1 * c2 * c3 + s1 * s2 * s3,   // scalar (w)
            c1 * c2 * s3 + s1 * s2 * c3,   // x
            s1 * c2 * c3 - c1 * s2 * s3,   // y
            c1 * s2 * c3 - s1 * c2 * s3    // z
        );
    }

    function openFile(path) {
        var openFileExt = playerController.returnFileExtension(path);
        if(openFileExt == "cplaypres"){
            slides.presentationToLoad = path.toString();
            slides.openCPlayPresentation();
        }
        else if(UserInterfaceSettings.mappingModeOnOpenFile 
        && openFileExt != "cplayfile" && openFileExt != "cplaylist" 
        && openFileExt != "cplay_file" && openFileExt != "cplay_list" 
        && openFileExt != "fdv" && openFileExt != "playlist"){
            openFileValuesDialogLabel.text = playerController.returnFileName(path);
            newMediaFileToOpen = path;
            openFileValuesDialog.open();
        }
        else{
            loadNotice.expect(path);
            openMediaFile(path.toString(), true, PlaylistSettings.loadSiblings);
        }
        // the timer scrolls the playlist to the playing file
        // once the table view rows are loaded
        playList.scrollPositionTimer.start();
        mpv.focus = true;
        LocationSettings.fileDialogLastLocation = app.parentUrl(path);
        LocationSettings.save();
    }

    Connections {
        function onActionsUpdated() {
            actions.updateShortcuts();
            actionsAlternate.updateShortcuts();
        }
        function onApplicationInteraction() {
            if(window.isIdleMode) {
                window.isIdleMode = false;
            }
            if(UserInterfaceSettings.idleModeOn){
                idleModeTimer.restart();
            }
        }
        target: app
    }

    Connections {
        function onQuitCPlay() {
            actions.quitApplicationAction.trigger();
        }

        target: playerController
    }

    // Cluster node load failures: the detailed rows are shown only in the Logging window
    // (LoggingWindow.qml). On the master we latch a flag the first time a failure appears so a
    // small alert button can pop up in the top-left corner of the main view; pressing it opens
    // the Logging window and clears the flag, hiding the button again. New failures re-arm it.
    property bool nodeLoadFailureSeen: false

    Connections {
        target: playerController
        function onNodeLoadFailuresChanged() {
            if (ConfigModel.isMaster && playerController.nodeLoadFailures.length > 0)
                window.nodeLoadFailureSeen = true;
        }
    }

    title: mpv.mediaTitle || qsTr("C-Play")
    visible: true
    visibility: window.isFullScreenMode ? Window.FullScreen : Window.Windowed
    color: window.hideUI ? "black" : Kirigami.Theme.alternateBackgroundColor
    minimumHeight: 621 > Screen.height / 2 ? Screen.height / 2 : 621
    minimumWidth: 1120 > Screen.width / 2 ? Screen.width / 2 : 1120
    height: 909 * Screen.devicePixelRatio > Screen.height ? Screen.height - 60 : 909
    width: 1680 * Screen.devicePixelRatio > Screen.width ? Screen.width - 20 : 1720

    header: Header {
        id: header

    }
    menuBar: MenuBar {
        id: menuBar
        property bool hide: false

        visible: !window.hideUI && !menuBar.hide

        Kirigami.Theme.colorSet: Kirigami.Theme.Header

        background: Rectangle {
            color: Kirigami.Theme.backgroundColor
        }

        FileMenu {
            onOpened: PopupHelpers.handlePopupOpen()
            onClosed: PopupHelpers.handlePopupClose()
        }
        PlaybackMenu {
            onOpened: PopupHelpers.handlePopupOpen()
            onClosed: PopupHelpers.handlePopupClose()
        }
        AudioMenu {
            onOpened: PopupHelpers.handlePopupOpen()
            onClosed: PopupHelpers.handlePopupClose()
        }
        SubtitleMenu {
            onOpened: PopupHelpers.handlePopupOpen()
            onClosed: PopupHelpers.handlePopupClose()
        }
        ViewsMenu {
        }
        SettingsMenu {
            onOpened: PopupHelpers.handlePopupOpen()
            onClosed: PopupHelpers.handlePopupClose()
        }
        HelpMenu {
            onOpened: PopupHelpers.handlePopupOpen()
            onClosed: PopupHelpers.handlePopupClose()
        }
    }

    Component.onCompleted: {
        app.activateColorScheme(UserInterfaceSettings.colorScheme);
        if (UserInterfaceSettings.fullScreenAtStartup) {
            window.isFullScreenMode = true;
        }
    }

    SystemPalette {
        id: systemPalette

        colorGroup: SystemPalette.Active
    }
    SettingsEditor {
        id: settingsEditor

    }
    SaveAsCPlayFile {
        id: saveAsCPlayFileWindow

    }
    ViewPlaylistItem {
        id: viewPlaylistItemWindow

    }
    SlidesQtItem {
        id: slidesViewItem

        Component.onCompleted: {
            slidesViewItem.initializeWithControlWindow(window, app.slides);
        }
    }
    Actions {
        id: actions

    }
    Actions {
        id: actionsAlternate
        isPrimary: false
    }
    BackgroundImage {
        id: bgImage
        visible: !viewLayersIn3DRenderItem.visible

    }
    MpvVideo {
        id: mpv
        visible: !viewLayersIn3DRenderItem.visible

        onFileLoaded: {
            floatingOverlayImage.source = mpv.getOverlayFileUrl();
            floatingOverlayImage.opacity = (floatingOverlayImage.source !== "" ? 1 : 0);
        }

        Osd {
            id: osd

        }
        DropArea {
            id: dropAreaMpv

            anchors.fill: parent
            keys: ["text/uri-list"]

            onDropped: function(drop) {
                if (!drop.urls || drop.urls.length === 0)
                    return;
                openFile(app.pathToUrl(drop.urls[0]))
            }
        }
    }
    ForegroundImage {
        id: fgImage
        visible: !viewLayersIn3DRenderItem.visible

    }

    LayersRendererQtItem {
        id: viewLayersIn3DRenderItem
        visible: window.mainViewMode > 0

        anchors.left: (window.hideUI ? parent.left : PlaylistSettings.position === "left" ? (playSections.visible ? playSections.right : playList.right) : (layers.visible ? layers.right : slides.right))
        anchors.right: (window.hideUI ? parent.right : PlaylistSettings.position === "right" ? (playList.visible ? playList.left : playSections.left) : (slides.visible ? slides.left : layers.left))
        anchors.top: parent.top
        height: footer.visible ? parent.height - footer.height : parent.height
        width: parent.width

        // Drive the OpenGL renderer from the live PerspectiveCamera state
        fieldOfView: originCamera.fieldOfView
        cameraPosition: originCamera.scenePosition
        cameraEulerRotation: originCamera.sceneRotation.toEulerAngles()

        meshRadius: mpv.radius
        meshFov: mpv.fov
        meshAngle: mpv.angle

        // One-pass 180-degree fisheye (fulldome) rendering, selected by the main view mode menu.
        renderAsFisheye: window.mainViewMode === 2
        // All layers mapped on a sphere that the mouse rotates, selected the same way.
        renderAsSphere: window.mainViewMode === 3

        // C-Lux live light-color overlay in the 3D view (only when built with CLUX support): a
        // toggle initialised from UserInterfaceSettings.cluxShowIn3DViewAtStartup and flipped at
        // runtime from the C-Lux Editor, colors from the active client (the same source
        // CLuxPreview.qml uses). The item has no such properties
        // without CLUX support, so the bindings stay inactive then.
        Binding {
            target: viewLayersIn3DRenderItem
            property: "cluxPreviewVisible"
            value: app.cluxPreviewVisible
            when: CLUX_SUPPORT
        }
        Binding {
            target: viewLayersIn3DRenderItem
            property: "cluxPreviewFrame"
            value: app.cluxClient ? app.cluxClient.frame : []
            when: CLUX_SUPPORT
        }

        mpvObject: mpv
        backgroundImageFile: playerController.checkAndCorrectPath(playerController.backgroundImageFileUrl())
        foregroundImageFile: playerController.checkAndCorrectPath(playerController.foregroundImageFileUrl())

        Connections {
           function onBackgroundImageChanged() {
                viewLayersIn3DRenderItem.backgroundImageFile = playerController.checkAndCorrectPath(playerController.backgroundImageFileUrl());
           }
           function onForegroundImageChanged() {
                viewLayersIn3DRenderItem.foregroundImageFile = playerController.checkAndCorrectPath(playerController.foregroundImageFileUrl());
           }

            target: playerController
        }

        onVisibleChanged: {
            if (layerView.layerItem)
                layerView.layerItem.updateEnabled(!visible);
        }

        View3D {
            id: cameraView
            anchors.fill: parent

            environment: SceneEnvironment {
                backgroundMode: SceneEnvironment.Transparent
            }

            Node {
                id: originNode
                position: Qt.vector3d(0, 0, 0)

                PerspectiveCamera {
                    id: originCamera
                    fieldOfView: UserInterfaceSettings.fov3Dview
                    clipNear: 0.1
                    clipFar: 1000.0

                    // Startup camera pose from the settings (Window & UI). The bindings are
                    // evaluated when the view is created; once the user orbits/zooms, the
                    // assignments break them and the camera moves freely until restart.
                    position: Qt.vector3d(UserInterfaceSettings.cameraPosition3DviewX,
                                          UserInterfaceSettings.cameraPosition3DviewY,
                                          UserInterfaceSettings.cameraPosition3DviewZ)
                    rotation: cameraRotationFromEuler(UserInterfaceSettings.cameraRotation3DviewX,
                                                      UserInterfaceSettings.cameraRotation3DviewY,
                                                      UserInterfaceSettings.cameraRotation3DviewZ)
                }
            }

            OrbitCameraController {
                camera: originCamera
                origin: originNode
                panEnabled: false

                // Mouse input is handled by layerRenderMouseArea below (drag orbits the scene,
                // Ctrl+left-drag moves/aims the selected layer and Shift+left-drag resizes its
                // plane or moves its distance), so the controller's own input handlers are disabled.
                mouseEnabled: false

                // The built-in wheel/pinch zoom scales with the camera's distance from the
                // origin, which is 0 here (camera at sphere center), so it would do nothing;
                // zoom is handled by layerRenderMouseArea instead. automaticClipping must be
                // off because it forces camera.z to clipNear when z is 0 and shrinks clipFar
                // below the content radius.
                automaticClipping: false
            }
        }

        Component.onCompleted: {
            // Register with the Application so the settings dialog can read the live camera pose.
            app.setLayersRenderer(viewLayersIn3DRenderItem);

            // The settings decide which main view mode is active at startup; after this, only the
            // globe menu in the header taskbar (window.mainViewMode) changes it during playback.
            if(UserInterfaceSettings.show3DviewAtStartup){
                window.mainViewMode = UserInterfaceSettings.renderAsFisheyeIn3DView ? 2 :
                                      UserInterfaceSettings.renderAsSphereIn3DView ? 3 : 1;
            }

            // Showing the master NDI output on the nodes at startup also decides the master view.
            if(NDI_SUPPORT && UserInterfaceSettings.masterNdiOnNodesAtStartup > 0){
                window.mainViewMode = UserInterfaceSettings.masterNdiOnNodesAtStartup - 1;
                window.showNdiOnNodes = true;
            }
        }

        MouseArea {
            id: layerRenderMouseArea
            anchors.fill: parent

            // Interaction state machine:
            //  0 = idle, 1 = pressed (waiting to see if it becomes a drag),
            //  2 = orbiting the camera,
            //  3/4/5 = dragging the selected layer with Ctrl/Alt/Shift held.
            property int dragMode: 0
            property real pressX: 0
            property real pressY: 0
            property real lastX: 0
            property real lastY: 0
            property bool layerDragChanged: false
            // Which modifier was held when the current press started; decides between orbiting
            // and dragging the selected layer once the drag is committed, and which operation
            // (configured per modifier combo in the Presentation settings) that drag performs.
            property bool ctrlHeldAtPress: false
            property bool altHeldAtPress: false
            property bool shiftHeldAtPress: false

            // A press only becomes a drag after this much movement so that double-clicks
            // (camera reset) never trigger an orbit or a layer move.
            readonly property real dragThreshold: 4
            // Orbit speed in degrees per pixel, tuned to feel like the old OrbitCameraController.
            readonly property real orbitSpeed: 0.25

            cursorShape: {
                if (dragMode === 3 || dragMode === 4 || dragMode === 5)
                    return Qt.ClosedHandCursor;
                if (dragMode !== 0)
                    return Qt.OpenHandCursor;
                return Qt.ArrowCursor;
            }

            function orbitCamera(dx, dy) {
                // Same sign conventions as the old OrbitCameraController: dragging right
                // decreases yaw and dragging down decreases pitch.
                var rot = originNode.eulerRotation;
                rot.y -= dx * layerRenderMouseArea.orbitSpeed;
                rot.x -= dy * layerRenderMouseArea.orbitSpeed;
                originNode.setEulerRotation(rot);
            }

            function commitDragMode() {
                if (dragMode !== 1)
                    return;
                // Ctrl/Alt/Shift+left-drag manipulates the layer selected in the Layers panel
                // with the operation configured for that modifier combo (flat layers); sphere
                // and dome layers rotate instead. With no such layer selected they fall back to
                // orbiting, as does any other drag.
                if ((pressedButtons & Qt.LeftButton) && !(pressedButtons & Qt.RightButton)) {
                    let action = -1;
                    if (ctrlHeldAtPress)
                        action = PresentationSettings.ctrlDragLayerAction;
                    else if (altHeldAtPress)
                        action = PresentationSettings.altDragLayerAction;
                    else if (shiftHeldAtPress)
                        action = PresentationSettings.shiftDragLayerAction;
                    if (action >= 0) {
                        const started = viewLayersIn3DRenderItem.beginLayerDrag(action, pressX, pressY);
                        dragMode = started ? (ctrlHeldAtPress ? 3 : altHeldAtPress ? 4 : 5) : 2;
                    } else {
                        dragMode = 2;
                    }
                } else if (pressedButtons & (Qt.LeftButton | Qt.RightButton)) {
                    dragMode = 2;
                } else {
                    dragMode = 0;
                }
            }

            function refreshGridParamsDialog() {
                // Re-read the dragged values from BaseLayer so the Grid Parameters dialog and
                // the Layers list row reflect them (the no-op setter emits layerValueChanged).
                var li = layerView.layerItem;
                if (li && li.layerIdx === viewLayersIn3DRenderItem.selectedPlaneLayerIndex) {
                    li.layerPlaneAzimuth = li.layerPlaneAzimuth;   // flat layers
                    li.layerPlaneHorizontal = li.layerPlaneHorizontal;   // flat layers (move modes)
                    li.layerPlaneVertical = li.layerPlaneVertical;       // flat layers (move modes)
                    li.layerPlaneWidth = li.layerPlaneWidth;             // flat layers (resize operation)
                    li.layerPlaneHeight = li.layerPlaneHeight;           // flat layers (resize operation)
                    li.layerPlaneDistance = li.layerPlaneDistance;       // flat layers (distance operation)
                    li.layerRotatePitch = li.layerRotatePitch;     // sphere/dome layers
                    li.layerRotateYaw = li.layerRotateYaw;         // sphere/dome layers
                }
            }

            onPressed: (mouse) => {
                if (mouse.button !== Qt.LeftButton && mouse.button !== Qt.RightButton)
                    return;
                dragMode = 1;
                pressX = lastX = mouse.x;
                pressY = lastY = mouse.y;
                layerDragChanged = false;
                ctrlHeldAtPress = (mouse.modifiers & Qt.ControlModifier) !== 0;
                altHeldAtPress = (mouse.modifiers & Qt.AltModifier) !== 0;
                shiftHeldAtPress = (mouse.modifiers & Qt.ShiftModifier) !== 0;
            }

            onPositionChanged: (mouse) => {
                if (dragMode === 0)
                    return;

                if (dragMode === 1) {
                    // Not a drag until the pointer has moved far enough from the press point.
                    if (Math.hypot(mouse.x - pressX, mouse.y - pressY) < layerRenderMouseArea.dragThreshold)
                        return;
                    commitDragMode();
                    lastX = mouse.x;   // start orbiting/dragging from here
                    lastY = mouse.y;
                    return;            // no camera/layer movement on the committing event itself
                }

                const dx = mouse.x - lastX;
                const dy = mouse.y - lastY;
                lastX = mouse.x;
                lastY = mouse.y;

                if (dragMode === 2) {
                    // The sphere view rotates the sphere instead of the camera.
                    if (viewLayersIn3DRenderItem.renderAsSphere)
                        viewLayersIn3DRenderItem.rotateSphere(dx, dy);
                    else
                        orbitCamera(dx, dy);
                } else if (dragMode === 3 || dragMode === 4 || dragMode === 5) {
                    if (viewLayersIn3DRenderItem.dragPlaneTo(mouse.x, mouse.y))
                        layerDragChanged = true;
                    refreshGridParamsDialog();
                }
            }

            onReleased: (mouse) => {
                // A plain click (no drag) places the speaker selected in the Audio mapping dialog.
                if (dragMode === 1 && mouse.button === Qt.LeftButton && app.audioMapping.showSpeakersInView
                        && app.audioMapping.placeOnClick && app.audioMapping.selectedSpeaker >= 0)
                    speakerOverlay.placeSpeaker(app.audioMapping.selectedSpeaker, mouse.x, mouse.y, true);
                const wasLayerDrag = dragMode === 3 || dragMode === 4 || dragMode === 5;
                if (wasLayerDrag)
                    viewLayersIn3DRenderItem.endPlaneDrag();
                dragMode = 0;
                if (wasLayerDrag && layerDragChanged) {
                    app.slides.selected.setLayersNeedsSave(true);
                    refreshGridParamsDialog();
                }
            }

            // Dolly zoom along the camera's view axis (mouse wheel / touchpad scroll).
            // Scroll up moves the camera forward into the scene (zoom in), scroll down back.
            onWheel: (wheel) => {
                // The sphere view moves its camera toward or away from the sphere instead.
                if (viewLayersIn3DRenderItem.renderAsSphere) {
                    viewLayersIn3DRenderItem.zoomSphere(wheel.angleDelta.y);
                    wheel.accepted = true;
                    return;
                }
                // The renderer builds its sphere at meshRadius / 100 in world units, so all
                // zoom distances and limits are based on that actual radius.
                const radius = viewLayersIn3DRenderItem.meshRadius / 100;
                const step = radius * 0.001 * wheel.angleDelta.y;
                const limit = radius * 0.95;   // keep the camera inside the rendered sphere
                originCamera.z = Math.max(-limit, Math.min(limit, originCamera.z - step));
                wheel.accepted = true;
            }

            // Double-click restores the original camera position and orientation (the saved
            // startup pose from the settings). fieldOfView is left alone: it stays bound to
            // UserInterfaceSettings.fov3Dview and nothing in this view modifies it.
            onDoubleClicked: {
                // The sphere view restores its own default sphere pose and distance.
                if (viewLayersIn3DRenderItem.renderAsSphere) {
                    viewLayersIn3DRenderItem.resetSphereView();
                    return;
                }
                originNode.setEulerRotation(Qt.vector3d(0, 0, 0));
                originCamera.position = Qt.vector3d(UserInterfaceSettings.cameraPosition3DviewX,
                                                    UserInterfaceSettings.cameraPosition3DviewY,
                                                    UserInterfaceSettings.cameraPosition3DviewZ);
                originCamera.rotation = cameraRotationFromEuler(UserInterfaceSettings.cameraRotation3DviewX,
                                                                UserInterfaceSettings.cameraRotation3DviewY,
                                                                UserInterfaceSettings.cameraRotation3DviewZ);
                dragging = false;
            }

            DropArea {
                id: dropAreaMpv2

                anchors.fill: parent
                keys: ["text/uri-list"]

                onDropped: function(drop) {
                    if (!drop.urls || drop.urls.length === 0)
                        return;
                    openFile(app.pathToUrl(drop.urls[0]))
                }
            }
        }

        // Speakers of the spatial audio mapping, shown and placed from the Audio mapping dialog:
        // the surround layout speakers on the dome, or the speaker array on the dome or along
        // the circular screen (the unwrapped 360 degree strip of the circular surface mapping).
        // Dragging a marker moves the speaker, pressing it selects it. Speaker array markers
        // show their share of the layer selected in the Layers panel.
        Item {
            id: speakerOverlay
            anchors.fill: parent
            visible: app.audioMapping.showSpeakersInView && app.audioMapping.mode !== 0

            readonly property bool arrayMode: app.audioMapping.mode === 2
            readonly property bool circular: arrayMode && app.audioMapping.arrayLayout === 1
            readonly property var speakerList: arrayMode ? app.audioMapping.arraySpeakers : app.audioMapping.speakers
            property var shares: []
            // Bumped regularly so the markers follow layer distance and grid setting changes too.
            property int tick: 0

            // Moves speaker index to the dome/screen point under (x, y) in this item.
            function placeSpeaker(index, x, y, save) {
                if (circular) {
                    const c = viewLayersIn3DRenderItem.circularPointAtPoint(x, y);
                    if (c.valid)
                        app.audioMapping.setArraySpeakerCircularPosition(index, c.degrees, c.height, save);
                    return c.valid;
                }
                const d = viewLayersIn3DRenderItem.domeDirectionAtPoint(x, y);
                if (!d.valid)
                    return false;
                if (arrayMode)
                    app.audioMapping.setArraySpeakerDomePosition(index, d.azimuth, d.elevation, save);
                else
                    app.audioMapping.setSpeakerPosition(index, d.azimuth, d.elevation, save);
                return true;
            }

            Timer {
                interval: 250
                repeat: true
                running: speakerOverlay.visible && viewLayersIn3DRenderItem.visible
                onTriggered: {
                    speakerOverlay.tick++;
                    if (speakerOverlay.arrayMode)
                        speakerOverlay.shares = viewLayersIn3DRenderItem.audioSharesForSelectedLayer();
                }
            }

            Repeater {
                model: speakerOverlay.speakerList.length

                delegate: Rectangle {
                    id: speakerMarker

                    required property int index
                    readonly property var speaker: speakerOverlay.speakerList[index]
                    readonly property bool lfe: !speakerOverlay.arrayMode && !!speaker && speaker.lfe
                    readonly property bool selected: app.audioMapping.selectedSpeaker === index
                    readonly property string label: !speaker ? ""
                                                    : speakerOverlay.arrayMode ? String(speaker.channel) : speaker.label
                    readonly property var projected: {
                        // Re-project whenever the camera, the dome or the view size changes.
                        viewLayersIn3DRenderItem.cameraPosition;
                        viewLayersIn3DRenderItem.cameraEulerRotation;
                        viewLayersIn3DRenderItem.fieldOfView;
                        viewLayersIn3DRenderItem.meshAngle;
                        viewLayersIn3DRenderItem.meshRadius;
                        viewLayersIn3DRenderItem.meshFov;
                        viewLayersIn3DRenderItem.renderAsFisheye;
                        viewLayersIn3DRenderItem.renderAsSphere;
                        viewLayersIn3DRenderItem.sphereRotation;
                        viewLayersIn3DRenderItem.sphereDistance;
                        speakerOverlay.width;
                        speakerOverlay.height;
                        speakerOverlay.tick;
                        if (!speaker)
                            return { visible: false, x: 0, y: 0 };
                        if (speakerOverlay.circular)
                            return viewLayersIn3DRenderItem.projectCircularPoint(speaker.degrees, speaker.height);
                        return viewLayersIn3DRenderItem.projectDomeDirection(speaker.azimuth, speaker.elevation);
                    }
                    readonly property string shareText: speakerOverlay.arrayMode && index < speakerOverlay.shares.length
                                                        ? qsTr("%1%").arg(speakerOverlay.shares[index].toFixed(0)) : ""

                    visible: !!speaker && !lfe && projected.visible
                    width: Kirigami.Units.gridUnit * 2
                    height: width
                    radius: width / 2
                    x: projected.x - width / 2
                    y: projected.y - height / 2
                    color: selected ? Kirigami.Theme.highlightColor : Qt.rgba(0, 0, 0, 0.6)
                    border.color: "white"
                    border.width: selected ? 3 : 2

                    Label {
                        anchors.centerIn: parent
                        color: "white"
                        font.bold: true
                        text: speakerMarker.label
                    }
                    Label {
                        anchors.top: parent.bottom
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: text !== ""
                        color: "white"
                        style: Text.Outline
                        styleColor: "black"
                        font.bold: true
                        text: speakerMarker.shareText
                    }

                    MouseArea {
                        id: speakerMouseArea

                        property bool moved: false

                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor

                        onPressed: {
                            app.audioMapping.selectedSpeaker = speakerMarker.index;
                            moved = false;
                        }
                        onPositionChanged: (mouse) => {
                            if (!pressed)
                                return;
                            const p = mapToItem(speakerOverlay, mouse.x, mouse.y);
                            if (speakerOverlay.placeSpeaker(speakerMarker.index, p.x, p.y, false))
                                moved = true;
                        }
                        onReleased: {
                            if (!moved)
                                return;
                            if (speakerOverlay.arrayMode)
                                app.audioMapping.saveArraySpeakers();
                            else
                                app.audioMapping.saveSpeakerPositions();
                        }

                        ToolTip.visible: containsMouse && !pressed
                        ToolTip.delay: 300
                        ToolTip.text: {
                            const spk = speakerMarker.speaker;
                            if (!spk)
                                return "";
                            if (speakerOverlay.circular)
                                return qsTr("Speaker %1 (output channel %2): %3° on the screen, height %4 cm")
                                    .arg(speakerMarker.index + 1).arg(spk.channel).arg(spk.degrees.toFixed(1)).arg(spk.height.toFixed(0));
                            if (speakerOverlay.arrayMode)
                                return qsTr("Speaker %1 (output channel %2): azimuth %3°, elevation %4°")
                                    .arg(speakerMarker.index + 1).arg(spk.channel).arg(spk.azimuth.toFixed(0)).arg(spk.elevation.toFixed(0));
                            return qsTr("%1 (channel %2): azimuth %3°, elevation %4°")
                                .arg(spk.label).arg(speakerMarker.index + 1).arg(spk.azimuth.toFixed(0)).arg(spk.elevation.toFixed(0));
                        }
                    }
                }
            }
        }

    }

    // The Layers panel is the single source of truth for which flat layer the 3D view can
    // move with Ctrl+left-drag. The C++ side validates the index against the current slide
    // and clears it for non-plane rows, so selecting a dome/sphere row disables the movement.
    Connections {
        target: layers.layersView

        function onCurrentIndexChanged() {
            viewLayersIn3DRenderItem.setPlaneSelectionByIndex(layers.layersView.currentIndex);
        }
    }

    // Cluster node load-failure alert: a compact button in the top-left corner of the main view
    // (above the video/3D area) that appears as soon as any node reports a failed load, so the
    // problem is visible without the OSD. Pressing it opens the Logging window, where the failed
    // (node, path) rows are listed, and dismisses the button; it reappears on the next failure.
    // Anchored to the main viewport (same geometry as MpvVideo / the 3D view), so it never
    // covers the playlist or layer panels, and it stays visible in fullscreen/idle mode.
    Button {
        id: nodeLoadFailureButton

        visible: ConfigModel.isMaster && window.nodeLoadFailureSeen
        anchors.left: mpv.left
        anchors.top: mpv.top
        anchors.leftMargin: Kirigami.Units.smallSpacing
        anchors.topMargin: Kirigami.Units.smallSpacing
        z: 100   // above the video / 3D view / layer panels
        padding: Kirigami.Units.smallSpacing

        contentItem: Row {
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                source: "dialog-error"
                width: Kirigami.Units.iconSizes.small
                height: Kirigami.Units.iconSizes.small
                anchors.verticalCenter: parent.verticalCenter
            }
            Label {
                text: qsTr("Node load problem")
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        ToolTip {
            text: qsTr("One or more cluster nodes failed to load a file. Click to open the Logging window.")
        }

        onClicked: {
            loggingWindow.visible = true;
            window.nodeLoadFailureSeen = false;
        }
    }

    // Confirms a file opened with Open File, dropped on the master view or loaded from the
    // playlist was loaded (unless turned off in the settings), and warns
    // when nothing is shown because the media visibility is 0%. Centered at the top of the main
    // viewport (same geometry as MpvVideo / the 3D view).
    LoadNotice {
        id: loadNotice

        anchors.horizontalCenter: mpv.horizontalCenter
        anchors.top: mpv.top
        anchors.topMargin: Kirigami.Units.largeSpacing
    }

    PlaySections {
        id: playSections

    }
    PlayList {
        id: playList

    }
    Slides {
        id: slides

    }
    SlidesVisibilityView {
        id: slidesVisView

    }
    Layers {
        id: layers

    }
    LayersAddNew {
        id: layersAddNew
    }
    RestCommandsEditor {
        id: restCommandsEditor
    }
    // C-Lux editor dialog. Only instantiated when built with C-Lux support (BUILD_CPLAY_WITH_CLUX):
    // its bindings reference app.cluxClient, which does not exist on app without it, so the loader
    // stays inactive in that case and none of them are ever evaluated.
    Loader {
        id: cluxEditorLoader
        active: CLUX_SUPPORT
        source: "CLuxEditor.qml"
    }
    MediaMTXEditor {
        id: mediaMtxEditor
    }
    VdoNinjaRoomsEditor {
        id: vdoNinjaRoomsEditor
    }
    AudioMappingDialog {
        id: audioMappingDialog
    }
    LoggingWindow {
        id: loggingWindow

        // Opening the Logging window by any route (menu, ...) also counts as acknowledging the
        // node load failures, so the alert button disappears.
        onVisibleChanged: {
            if (visible)
                window.nodeLoadFailureSeen = false;
        }
    }

    LayerView {
        id: layerView

        onVisibleChanged: {
            if (layerView.layerItem)
                layerView.layerItem.updateEnabled(!viewLayersIn3DRenderItem.visible);
        }

    }
    LayerViewGridParams {
        id: layerViewGridParams

    }
    Footer {
        id: footer

    }

    // The presenter view, in builds with it (BUILD_CPLAY_WEB_VIEWS). Slide thumbnails are
    // rendered with the master view camera (perspective, fisheye or sphere view).
    Binding {
        when: WEB_VIEWS_SUPPORT
        target: WEB_VIEWS_SUPPORT ? playerController.presenter : null
        property: "layersRenderer"
        value: viewLayersIn3DRenderItem
    }
    Loader {
        id: presenterWindowLoader
        active: WEB_VIEWS_SUPPORT
        source: active ? Qt.resolvedUrl("PresenterWindow.qml") : ""
    }
    // The movie view of the playlist and its sections (web/movie_mode).
    Loader {
        id: movieWindowLoader
        active: WEB_VIEWS_SUPPORT
        source: active ? Qt.resolvedUrl("PresenterWindow.qml") : ""
        onLoaded: item.view = "movie"
    }

    Window {
        id: floatingTextureWindow
        visible: false
        flags: Qt.FramelessWindowHint | Qt.Window
        color: "black"
        x: UserInterfaceSettings.floatingWindowPosX;
        y: UserInterfaceSettings.floatingWindowPosY;
        width: UserInterfaceSettings.floatingWindowWidth;
        height: UserInterfaceSettings.floatingWindowHeight;

        LayerQtItem {
            id: floatingLayerViewItem
            visible: !UserInterfaceSettings.floatingWindowShowsMainVideoLayer && !UserInterfaceSettings.floatingWindowShows3DView && floatingTextureWindow.visible

            height: parent.height
            width: parent.width

            // Started/stopped here rather than with the window, so switching the floating
            // window content while it is shown also starts or stops the custom layer.
            onVisibleChanged: {
                if (visible)
                    floatingLayerViewItem.start()
                else
                    floatingLayerViewItem.stop()
            }

            Component.onCompleted: {
                if(UserInterfaceSettings.floatingWindowLayerType >= 0 && UserInterfaceSettings.floatingWindowLayerPath !== ""){
                    floatingLayerViewItem.createLayer(UserInterfaceSettings.floatingWindowLayerType, UserInterfaceSettings.floatingWindowLayerPath);
                    floatingLayerViewItem.layerVolume = UserInterfaceSettings.floatingWindowVolume;
                }
                if(UserInterfaceSettings.floatingWindowVisibleAtStartup){
                    floatingTextureWindow.visible = true;
                }
            }
        }

        MpvView {
            id: floatingMpvView
            visible: UserInterfaceSettings.floatingWindowShowsMainVideoLayer && !UserInterfaceSettings.floatingWindowShows3DView && floatingTextureWindow.visible
            anchors.fill: parent
            mpvObject: mpv
            renderingPriority: 1
        }

        Image {
            id: floatingOverlayImage
            visible: UserInterfaceSettings.floatingWindowShowsMainVideoLayer && !UserInterfaceSettings.floatingWindowShows3DView && floatingTextureWindow.visible
            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
            opacity: 1
        }

        // The 3D master view: the perspective, fisheye or sphere view selected for the master UI,
        // rendered by viewLayersIn3DRenderItem at this window's size.
        LayersMirrorQtItem {
            id: floatingLayersMirrorItem
            visible: UserInterfaceSettings.floatingWindowShows3DView && floatingTextureWindow.visible
            anchors.fill: parent
            source: viewLayersIn3DRenderItem
        }
    }

    Dialog {
        id: openFileValuesDialog
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: 250

        onOpened: PopupHelpers.handlePopupOpen()
        onClosed: PopupHelpers.handlePopupClose()

        GridLayout {
            anchors.fill: parent
            anchors.margins: 15
            columnSpacing: 2
            columns: 2
            rowSpacing: 8

            RowLayout {
                Label {
                    text: qsTr("Mappings of ")
                    font.bold: true
                }
                Label {
                    id: openFileValuesDialogLabel
                    text: "..."
                    font.italic: true
                    Layout.fillWidth: true
                }
                Layout.columnSpan: 2
                Layout.fillWidth: true
            }

            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Stereo:")
            }
            ComboBox {
                id: stereoscopicMode

                Layout.fillWidth: true
                focusPolicy: Qt.NoFocus
                textRole: "mode"

                model: ListModel {
                    id: stereoscopicModeList

                    ListElement {
                        mode: "2D (mono)"
                        value: 0
                    }
                    ListElement {
                        mode: "3D (side-by-side)"
                        value: 1
                    }
                    ListElement {
                        mode: "3D (top-bottom)"
                        value: 2
                    }
                    ListElement {
                        mode: "3D (top-bottom+flip)"
                        value: 3
                    }
                }

                onActiveFocusChanged: {
                    if (activeFocus) {
                        PopupHelpers.handlePopupOpen();
                    } else {
                        PopupHelpers.handlePopupClose();  
                    }
                }
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Grid:")
            }
            ComboBox {
                id: gridMode

                Layout.fillWidth: true
                focusPolicy: Qt.NoFocus
                textRole: "mode"

                model: ListModel {
                    id: gridModeList

                    ListElement {
                        mode: "None/Pre-split"
                        value: 0
                    }
                    ListElement {
                        mode: "Plane/Flat"
                        value: 1
                    }
                    ListElement {
                        mode: "Dome"
                        value: 2
                    }
                    ListElement {
                        mode: "Sphere EQR"
                        value: 3
                    }
                    ListElement {
                        mode: "Sphere EAC"
                        value: 4
                    }
                }

                onActiveFocusChanged: {
                    if (activeFocus) {
                        PopupHelpers.handlePopupOpen();
                    } else {
                        PopupHelpers.handlePopupClose();  
                    }
                }
            }
        }

        onVisibleChanged: {
            for (let i = 0; i < stereoscopicModeList.count; ++i) {
                if (stereoscopicModeList.get(i).value === playerController.backgroundStereoMode()) {
                    stereoscopicMode.currentIndex = i;
                    break;
                }
            }
            for (let i = 0; i < gridModeList.count; ++i) {
                if (gridModeList.get(i).value === playerController.backgroundGridMode()) {
                    gridMode.currentIndex = i;
                    break;
                }
            }
        }

        onAccepted: {
            if (stereoscopicMode.currentIndex < 0 || gridMode.currentIndex < 0)
                return;
            loadNotice.expect(newMediaFileToOpen);
            openMediaFile(newMediaFileToOpen.toString(), true, PlaylistSettings.loadSiblings);
            mpv.stereoscopicMode = stereoscopicModeList.get(stereoscopicMode.currentIndex).value;
            mpv.gridToMapOn = gridModeList.get(gridMode.currentIndex).value;
            // the timer scrolls the playlist to the playing file
            // once the table view rows are loaded
            playList.scrollPositionTimer.start();
            mpv.focus = true;
            LocationSettings.fileDialogLastLocation = app.pathToUrl(newMediaFileToOpen);
            LocationSettings.save();
        }
    }

    // Opened from AudioMenu ("Check JACK server"); a popup rather than the OSD, which can be hidden.
    Dialog {
        id: jackStatusDialog

        property bool running: false
        property alias text: jackStatusDialogLabel.text

        function showStatus() {
            const status = app.jackServerStatus();
            running = status.running;
            text = status.text;
            open();
        }

        title: running ? qsTr("JACK server running") : qsTr("JACK server not found")
        modal: true
        width: 520
        anchors.centerIn: parent
        standardButtons: Dialog.Ok

        onOpened: PopupHelpers.handlePopupOpen()
        onClosed: PopupHelpers.handlePopupClose()

        Label {
            id: jackStatusDialogLabel

            width: parent.width
            wrapMode: Text.WordWrap
        }
    }

    Dialog {
        id: loggingCloseDialog
        title: qsTr("Logging enabled")
        modal: true
        width: 520
        standardButtons: Dialog.Yes | Dialog.No

        onOpened: PopupHelpers.handlePopupOpen()
        onClosed: PopupHelpers.handlePopupClose()

        Label {
            text: qsTr("General logging, MPV performance statistics and/or node telemetry are currently enabled.\n\nDo you want to turn them off so they are not active on next start?")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        onAccepted: {
            LoggingSettings.generalLoggingEnabled = false;
            LoggingSettings.performanceMetricsEnabled = false;
            LoggingSettings.nodeTelemetryEnabled = false;
            LoggingSettings.save();
            app.sendQuitToNodes();
            closeTimer.start();
        }

        onRejected: {
            app.sendQuitToNodes();
            closeTimer.start();
        }
    }

    CPlayFileDialog {
        id: openFileDialog

        parentWindow: window
        fileMode: CPlayFileDialog.OpenFile
        currentFolder: LocationSettings.fileDialogLocation !== "" ? app.pathToUrl(LocationSettings.fileDialogLocation) : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        title: "Open File"

        onAccepted: {
            openFile(openFileDialog.selectedFile);
        }
        onRejected: mpv.focus = true
    }
    CPlayFileDialog {
        id: addToPlaylistDialog

        parentWindow: window
        fileMode: CPlayFileDialog.OpenFile
        currentFolder: LocationSettings.cPlayFileLocation !== "" ? app.pathToUrl(LocationSettings.cPlayFileLocation) : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        nameFilters: ["C-Play file (*.cplayfile)", "Uniview file (*.fdv)", "All files (*)"]
        title: "Add file to playlist"

        onAccepted: {
            mpv.addFileToPlaylist(addToPlaylistDialog.selectedFile.toString());
            mpv.focus = true;
        }
        onRejected: mpv.focus = true
    }
    CPlayFileDialog {
        id: saveCPlayFileDialog

        parentWindow: window
        fileMode: CPlayFileDialog.SaveFile
        currentFolder: LocationSettings.cPlayFileLocation !== "" ? app.pathToUrl(LocationSettings.cPlayFileLocation) : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        nameFilters: ["C-Play file (*.cplayfile)"]
        title: "Save C-Play File Config"

        onAccepted: {
            saveCPlayFile(saveCPlayFileDialog.selectedFile.toString());
            mpv.focus = true;
            saveCPlayFileDialog.visible = false;
            if (saveCPlayFileDialog.visible) {
                saveCPlayFileDialog.close();
            }
            saveAsCPlayFileWindow.visible = false;
        }
        onRejected: mpv.focus = true
    }
    CPlayFileDialog {
        id: openCPlayPlaylistDialog

        parentWindow: window
        fileMode: CPlayFileDialog.OpenFile
        currentFolder: LocationSettings.cPlayFileLocation !== "" ? app.pathToUrl(LocationSettings.cPlayFileLocation) : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        nameFilters: ["C-Play playlist (*.cplaylist)"]
        title: "Open C-Playlist"

        onAccepted: {
            mpv.loadFile(openCPlayPlaylistDialog.selectedFile.toString());
        }
        onRejected: mpv.focus = true
    }
    CPlayFileDialog {
        id: saveCPlayPlaylistDialog

        parentWindow: window
        fileMode: CPlayFileDialog.SaveFile
        currentFolder: LocationSettings.cPlayFileLocation !== "" ? app.pathToUrl(LocationSettings.cPlayFileLocation) : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        nameFilters: ["C-Play playlist (*.cplaylist)"]
        title: "Save C-Playlist"

        onAccepted: {
            saveCPlayPlaylist(saveCPlayPlaylistDialog.selectedFile.toString());
            mpv.focus = true;
        }
        onRejected: mpv.focus = true
    }
    Timer {
        id: garbageCollectionTimer

        interval: 5000
        repeat: false

        onTriggered: {
            gc();
        }
    }
    Timer {
        id: idleModeTimer

        interval: UserInterfaceSettings.idleModeTime * 1000
        running: UserInterfaceSettings.idleModeOn && !window.isIdleMode
        repeat: false

        onTriggered: {
            window.isIdleMode = true;
            garbageCollectionTimer.restart();
        }
    }
}
