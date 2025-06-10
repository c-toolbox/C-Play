/*
 * SPDX-FileCopyrightText:
 * 2021-2025 Erik Sunden <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts

import org.ctoolbox.cplay
import org.kde.kirigami as Kirigami

MpvObject {
    id: root

    property int mouseX: mouseArea.mouseX
    property int mouseY: mouseArea.mouseY
    property bool sphereGrid: false

    signal setAudio(int id)

    function handleTimePosition() {
        if (mpv.position < mpv.duration - 10) {
            saveTimePosition();
        } else {
            resetTimePosition();
        }
    }

    anchors.left: PlaylistSettings.position === "left" ? (playSections.visible ? playSections.right : playList.right) : (layers.visible ? layers.right : slides.right)
    anchors.right: PlaylistSettings.position === "right" ? (playList.visible ? playList.left : playSections.left) : (slides.visible ? slides.left : layers.left)
    anchors.top: parent.top
    height: window.isFullScreen() ? parent.height : parent.height - footer.height
    volume: AudioSettings.volume
    width: parent.width

    /*Shortcut {
        sequence: StandardKey.NextChild
        onActivated: view.currentIndex++
    }*/

    Component.onCompleted: {
        playerController.mpv = root;
        playerController.slides = app.slides;
        mpv.gridToMapOnChanged();
    }
    onEndFile: {
        if (reason === "error") {
            if (playlistModel.rowCount() === 0) {
                return;
            }
            const title = playlistModel.mediaTitle(playlistModel.getPlayingVideo());
            osd.message(qsTr("Could not play: %1").arg(title));
            // only skip to next video if it's a youtube playList
            // to do: figure out why playback fails and act accordingly
            if (!playList.isYouTubePlaylist) {
                return;
            }
            return;
        }
        if (mpv.eofMode === 0 && playerController.rewindMediaOnEOF()) {
            mpv.performRewind();
        } else if (mpv.eofMode === 1) {
            // Continue
            if (playList.playlistView.count <= 1) {
                return;
            }
            const nextFileRow = playlistModel.getPlayingVideo() + 1;
            if (nextFileRow < playList.playlistView.count) {
                mpv.pause = true;
                mpv.position = 0;
                mpv.loadItem(nextFileRow);
                mpv.playlistModel.setPlayingVideo(nextFileRow);
                if (PlaylistSettings.autoPlayNext)
                    playItem.start();
            } else {
                // Last file in playlist
                if (PlaylistSettings.repeat) {
                    mpv.pause = true;
                    mpv.position = 0;
                    mpv.loadItem(0);
                    mpv.playlistModel.setPlayingVideo(0);
                    if (PlaylistSettings.autoPlayNext)
                        playItem.start();
                }
            }
        }
    }
    onFadeDownTheRewind: {
        mpv.fadeImageDown();
        rewindAfterFades.start();
    }
    onFileLoaded: {
        loadingIndicatorParent.visible = false;
        header.audioTracks = getProperty("track-list").filter(track => track["type"] === "audio");
        mpv.pause = true;
        position = loadTimePosition();
        overlayImage.source = mpv.getOverlayFileUrl();
        overlayImage.opacity = (overlayImage.source !== "" ? 1 : 0);
    }
    onFileStarted: {
        if (playList.isYouTubePlaylist) {
            loadingIndicatorParent.visible = true;
        }
    }
    onGridToMapOnChanged: {
        if (mpv.gridToMapOn < 3) {
            if (root.sphereGrid)
                mpv.resetOrientation();
            root.sphereGrid = false;
        } else if (!root.sphereGrid) {
            mpv.resetOrientation();
            root.sphereGrid = true;
        }
    }
    onPauseChanged: {
        if (pause) {
            footer.playPauseButton.icon.name = "media-playback-start";
        } else {
            footer.playPauseButton.icon.name = "media-playback-pause";
        }
    }
    onPlaySectionsModelChanged: {
        if (playSections.sectionsView.count > 0 && playSections.state === "hidden") {
            actions.toggleSectionsAction.trigger();
        }
    }
    onPlaylistModelChanged: {
        if (playList.playlistView.count > 0 && playList.state === "hidden") {
            actions.togglePlaylistAction.trigger();
        }
    }
    onReady: {
        setProperty("screenshot-template", LocationSettings.screenshotTemplate);
        setProperty("screenshot-format", LocationSettings.screenshotFormat);
        const preferredAudioTrack = AudioSettings.preferredTrack;
        setProperty("aid", preferredAudioTrack === 0 ? "auto" : preferredAudioTrack);
        setProperty("alang", AudioSettings.preferredLanguage);
        if (app.getStartupFile() !== "") {
            window.openFile(app.getStartupFile(), false, PlaylistSettings.loadSiblings);
        }
    }
    onResetOrientation: {
        if (mpv.gridToMapOn < 3) {
            mpv.angle = GridSettings.surfaceAngle;
            mpv.radius = GridSettings.surfaceRadius;
            mpv.fov = GridSettings.surfaceFov;
            mpv.rotate = Qt.vector3d(GridSettings.surfaceRotateX, GridSettings.surfaceRotateY, GridSettings.surfaceRotateZ);
            mpv.translate = Qt.vector3d(GridSettings.surfaceTranslateX, GridSettings.surfaceTranslateY, GridSettings.surfaceTranslateZ);
        } else {
            mpv.rotate = Qt.vector3d(0, -90, 0);
        }
    }
    onSetAudio: {
        setProperty("aid", id);
    }

    Timer {
        id: rewindAfterFades

        interval: PlaybackSettings.fadeDuration

        onTriggered: {
            mpv.pause = true;
            mpv.position = 0;
            mpv.rewind();
        }
    }
    Timer {
        id: playItem

        interval: PlaylistSettings.autoPlayAfterTime * 1000

        onTriggered: {
            mpv.pause = false;
        }
    }
    Timer {
        id: saveWatchLaterFileTimer

        interval: 1000
        repeat: true
        running: !mpv.pause

        onTriggered: handleTimePosition()
    }
    Timer {
        id: hideCursorTimer

        property int timeNotMoved: 0
        property double tx: mouseArea.mouseX
        property double ty: mouseArea.mouseY

        interval: 50
        repeat: true
        running: window.isFullScreen() && mouseArea.containsMouse

        onTriggered: {
            if (mouseArea.mouseX === tx && mouseArea.mouseY === ty) {
                if (timeNotMoved > 2000) {
                    app.hideCursor();
                }
            } else {
                app.showCursor();
                timeNotMoved = 0;
            }
            tx = mouseArea.mouseX;
            ty = mouseArea.mouseY;
            timeNotMoved += interval;
        }
    }
    MouseArea {
        id: mouseArea

        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        anchors.fill: parent
        hoverEnabled: true

        onDoubleClicked: {
            if (mouse.button === Qt.LeftButton) {
                if (MouseSettings.leftx2) {
                    actions[MouseSettings.leftx2].trigger();
                }
            } else if (mouse.button === Qt.MiddleButton) {
                if (MouseSettings.middlex2) {
                    actions[MouseSettings.middlex2].trigger();
                }
            } else if (mouse.button === Qt.RightButton) {
                if (MouseSettings.rightx2) {
                    actions[MouseSettings.rightx2].trigger();
                }
            }
        }
        onPositionChanged: {}
        onPressed: {
            focus = true;
            if (mouse.button === Qt.LeftButton) {
                if (MouseSettings.left) {
                    actions[MouseSettings.left].trigger();
                }
            } else if (mouse.button === Qt.MiddleButton) {
                if (MouseSettings.middle) {
                    actions[MouseSettings.middle].trigger();
                }
            } else if (mouse.button === Qt.RightButton) {
                if (MouseSettings.right) {
                    actions[MouseSettings.right].trigger();
                }
            }
        }
        onWheel: {}
    }
    DropArea {
        id: dropArea

        anchors.fill: parent
        keys: ["text/uri-list"]

        onDropped: {
            if (app.mimeType(drop.urls[0]).startsWith("video/")) {
                window.openFile(drop.urls[0], true, PlaylistSettings.loadSiblings);
            }
        }
    }
    Connections {
        function onBackgroundVisibilityChanged() {
            root.opacity = 1 - playerController.backgroundVisibilityOnMaster();
        }
        function onLoadFromPlaylist(index) {
            root.pause = true;
            root.position = 0;
            root.loadItem(index);
            root.playlistModel.setPlayingVideo(index);
            if (mpv.autoPlay)
                playItem.start();
        }
        function onLoadFromSections(index) {
            root.pause = true;
            root.loadSection(index);
        }
        function onNext() {
            actions.playNextAction.trigger();
        }
        function onPause() {
            root.pause = true;
        }
        function onPlay() {
            root.pause = false;
        }
        function onPlaypause() {
            actions.playPauseAction.trigger();
        }
        function onPrevious() {
            actions.playPreviousAction.trigger();
        }
        function onSeek(offset) {
            root.command(["add", "time-pos", offset]);
        }
        function onStop() {
            root.position = 0;
            root.pause = true;
        }

        target: playerController
    }
    Rectangle {
        id: loadingIndicatorParent

        anchors.centerIn: parent
        color: {
            let color = Kirigami.Theme.backgroundColor;
            Qt.hsla(color.hslHue, color.hslSaturation, color.hslLightness, 0.2);
        }
        visible: false

        Kirigami.Icon {
            id: loadingIndicator

            anchors.centerIn: parent
            height: Kirigami.Units.iconSizes.large
            source: "view-refresh"
            width: Kirigami.Units.iconSizes.large

            Component.onCompleted: {
                parent.width = width + 10;
                parent.height = height + 10;
            }

            RotationAnimator {
                duration: 1500
                from: 0
                loops: Animation.Infinite
                running: true
                target: loadingIndicator
                to: 360
            }
        }
    }
    Image {
        id: overlayImage

        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        opacity: 1
    }
}
