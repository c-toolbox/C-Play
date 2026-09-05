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

    property var appActions: actions.list
    property var configure: app.action("configure")
    property bool isFullScreenMode: false
    property bool isIdleMode: false
    property bool hideUI: (isFullScreenMode || isIdleMode)
    property url newMediaFileToOpen: ""
    property bool uiPopupOpen: false  // True when any popup (Menu, ComboBox dropdown or Dialog) is open in this ApplicationWindow

    onClosing: {
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

    title: mpv.mediaTitle || qsTr("C-Play")
    visible: true
    visibility: window.isFullScreenMode ? Window.FullScreen : Window.Windowed
    color: window.hideUI ? "black" : Kirigami.Theme.alternateBackgroundColor
    minimumHeight: 621 > Screen.height / 2 ? Screen.height / 2 : 621
    minimumWidth: 1104 > Screen.width / 2 ? Screen.width / 2 : 1104
    height: 909 * Screen.devicePixelRatio > Screen.height ? Screen.height - 60 : 909
    width: 1640 * Screen.devicePixelRatio > Screen.width ? Screen.width - 20 : 1640

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
        SettingsMenu {
            onOpened: PopupHelpers.handlePopupOpen()
            onClosed: PopupHelpers.handlePopupClose()
        }
        HelpMenu {
            onOpened: PopupHelpers.handlePopupOpen()
            onClosed: PopupHelpers.handlePopupClose()
        }
    }

    Component.onCompleted: app.activateColorScheme(UserInterfaceSettings.colorScheme)

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
        visible: false

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
                    position: Qt.vector3d(0, 0, 0)
                }
            }

            OrbitCameraController {
                camera: originCamera
                origin: originNode
                panEnabled: false

                // Mouse input is handled by layerRenderMouseArea below (drag orbits the scene,
                // Ctrl+left-drag moves the flat layer selected in the Layers panel), so the
                // controller's own input handlers are disabled.
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
            if(UserInterfaceSettings.show3DviewAtStartup){
                viewLayersIn3DRenderItem.visible = true;
            }
        }

        MouseArea {
            id: layerRenderMouseArea
            anchors.fill: parent

            // Interaction state machine:
            //  0 = idle, 1 = pressed (waiting to see if it becomes a drag),
            //  2 = orbiting the camera, 3 = moving the selected flat layer.
            property int dragMode: 0
            property real pressX: 0
            property real pressY: 0
            property real lastX: 0
            property real lastY: 0
            property bool layerDragChanged: false
            // Whether Ctrl was held when the current press started; decides between orbiting
            // and moving the selected flat layer once the drag is committed.
            property bool ctrlHeldAtPress: false

            // A press only becomes a drag after this much movement so that double-clicks
            // (camera reset) never trigger an orbit or a layer move.
            readonly property real dragThreshold: 4
            // Orbit speed in degrees per pixel, tuned to feel like the old OrbitCameraController.
            readonly property real orbitSpeed: 0.25

            cursorShape: {
                if (dragMode === 3)
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
                // Ctrl+left-drag moves the flat layer selected in the Layers panel; with no
                // plane layer selected it falls back to orbiting. Any other drag orbits too.
                if ((pressedButtons & Qt.LeftButton) && !(pressedButtons & Qt.RightButton) && ctrlHeldAtPress) {
                    const started = viewLayersIn3DRenderItem.beginPlaneDrag(pressX, pressY);
                    dragMode = started ? 3 : 2;
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
                    orbitCamera(dx, dy);
                } else if (dragMode === 3) {
                    if (viewLayersIn3DRenderItem.dragPlaneTo(mouse.x, mouse.y))
                        layerDragChanged = true;
                    refreshGridParamsDialog();
                }
            }

            onReleased: (mouse) => {
                const wasLayerDrag = dragMode === 3;
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
                // The renderer builds its sphere at meshRadius / 100 in world units, so all
                // zoom distances and limits are based on that actual radius.
                const radius = viewLayersIn3DRenderItem.meshRadius / 100;
                const step = radius * 0.001 * wheel.angleDelta.y;
                const limit = radius * 0.95;   // keep the camera inside the rendered sphere
                originCamera.z = Math.max(-limit, Math.min(limit, originCamera.z - step));
                wheel.accepted = true;
            }

            // Double-click restores the original camera position and orientation.
            // fieldOfView is left alone: it stays bound to UserInterfaceSettings.fov3Dview
            // and nothing in this view modifies it.
            onDoubleClicked: {
                originNode.setEulerRotation(Qt.vector3d(0, 0, 0));
                originCamera.position = Qt.vector3d(0, 0, 0);
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

    PlaySections {
        id: playSections

    }
    PlayList {
        id: playList

        DropArea {
            id: dropAreaPlaylist

            anchors.fill: parent
            keys: ["text/uri-list"]

            onDropped: function(drop) {
                if (!drop.urls || drop.urls.length === 0)
                    return;
                for(var i in drop.urls){
                    mpv.addFileToPlaylist(drop.urls[i].toString());      
                }
                mpv.focus = true;
            }
        }
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

    Window {
        id: floatingTextureWindow
        visible: false
        flags: Qt.FramelessWindowHint | Qt.Window
        x: UserInterfaceSettings.floatingWindowPosX; 
        y: UserInterfaceSettings.floatingWindowPosY; 
        width: UserInterfaceSettings.floatingWindowWidth;
        height: UserInterfaceSettings.floatingWindowHeight;

        LayerQtItem {
            id: floatingLayerViewItem
            visible: !UserInterfaceSettings.floatingWindowShowsMainVideoLayer && floatingTextureWindow.visible

            height: parent.height
            width: parent.width

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
            visible: UserInterfaceSettings.floatingWindowShowsMainVideoLayer && floatingTextureWindow.visible
            anchors.fill: parent
            mpvObject: mpv
            renderingPriority: 1
        }

        Image {
            id: floatingOverlayImage
            visible: UserInterfaceSettings.floatingWindowShowsMainVideoLayer && floatingTextureWindow.visible
            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
            opacity: 1
        }

        onVisibleChanged: {
            if(!UserInterfaceSettings.floatingWindowShowsMainVideoLayer){
                if (visible){
                    floatingLayerViewItem.start()
                }
                else {
                    floatingLayerViewItem.stop()
                }
            }
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
