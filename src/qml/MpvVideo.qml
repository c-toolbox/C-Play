/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import mpv 1.0
import QtQuick.Scene3D 2.0

import org.ctoolbox.cplay 1.0
import org.kde.kirigami 2.10 as Kirigami

MpvObject {
    id: root

    property int mouseX: mouseArea.mouseX
    property int mouseY: mouseArea.mouseY
    property bool sphereGrid: false

    signal setSubtitle(int id)
    signal setSecondarySubtitle(int id)
    signal setAudio(int id)

    width: parent.width
    height: window.isFullScreen() ? parent.height : parent.height - footer.height
    anchors.left: PlaylistSettings.position === "left" ? playSections.right : parent.left
    anchors.right: PlaylistSettings.position === "right" ? playList.left : parent.right
    anchors.top: parent.top
    volume: GeneralSettings.volume

    onResetOrientation: {
        if(mpv.gridToMapOn < 3){
            mpv.angle = VideoSettings.surfaceAngle
            mpv.radius = VideoSettings.surfaceRadius
            mpv.fov = VideoSettings.surfaceFov
            mpv.rotate = Qt.vector3d(VideoSettings.surfaceRotateX, VideoSettings.surfaceRotateY, VideoSettings.surfaceRotateZ)
            mpv.translate = Qt.vector3d(VideoSettings.surfaceTranslateX, VideoSettings.surfaceTranslateY, VideoSettings.surfaceTranslateZ)
        }
        else{
            mpv.rotate = Qt.vector3d(0, -90, 0);
        }
    }

    onGridToMapOnChanged: {
        if(mpv.gridToMapOn < 3){
            if(root.sphereGrid)
                mpv.resetOrientation()
            root.sphereGrid = false
        }
        else if(!root.sphereGrid){
            mpv.resetOrientation()
            root.sphereGrid = true
        }
    }

    onSetSubtitle: {
        setProperty("sid", id)
    }

    onSetSecondarySubtitle: {
        setProperty("secondary-sid", id)
    }

    onSetAudio: {
        setProperty("aid", id)
    }

    onReady: {
        setProperty("screenshot-template", VideoSettings.screenshotTemplate)
        setProperty("screenshot-format", VideoSettings.screenshotFormat)
        const preferredAudioTrack = AudioSettings.preferredTrack
        setProperty("aid", preferredAudioTrack === 0 ? "auto" : preferredAudioTrack)
        setProperty("alang", AudioSettings.preferredLanguage)

        const preferredSubTrack = SubtitlesSettings.preferredTrack
        setProperty("sid", preferredSubTrack === 0 ? "auto" : preferredSubTrack)
        setProperty("slang", SubtitlesSettings.preferredLanguage)
        setProperty("sub-file-paths", SubtitlesSettings.subtitlesFolders.join(":"))

        if(PlaybackSettings.playlistToLoadOnStartup !== ""){
            window.openFile(PlaybackSettings.playlistToLoadOnStartup, false, PlaylistSettings.loadSiblings)
        }
    }

    onYoutubePlaylistLoaded: {
        mpv.command(["loadfile", playlistModel.getPath(GeneralSettings.lastPlaylistIndex)])
        playlistModel.setPlayingVideo(GeneralSettings.lastPlaylistIndex)

        playList.setPlayListScrollPosition()
    }

    onPlaylistModelChanged: {
        if (playList.playlistView.count > 0 && playList.state === "hidden") {
            actions.togglePlaylistAction.trigger()
        }
        /*else if(playList.state !== "hidden") {
            actions.togglePlaylistAction.trigger()
        }*/
    }

    onPlaySectionsModelChanged: {
        if (playSections.sectionsView.count > 0 && playSections.state === "hidden") {
            actions.toggleSectionsAction.trigger()
        }
        /*else if(playSections.state !== "hidden") {
            actions.toggleSectionsAction.trigger()
        }*/
    }

    onFileStarted: {
        if (playList.isYouTubePlaylist) {
            loadingIndicatorParent.visible = true
        }
    }

    onFileLoaded: {
        loadingIndicatorParent.visible = false
        header.audioTracks = getProperty("track-list").filter(track => track["type"] === "audio")
        header.subtitleTracks = getProperty("track-list").filter(track => track["type"] === "sub")

        mpv.pause = true
        position = loadTimePosition()

        overlayImage.source = mpv.getOverlayFileUrl();
        overlayImage.opacity = (!overlayImage.source.empty() ? 1 : 0);
    }

    onChapterChanged: {
        if (!PlaybackSettings.skipChapters) {
            return
        }

        const chapters = mpv.getProperty("chapter-list")
        const chaptersToSkip = PlaybackSettings.chaptersToSkip
        if (chapters.length === 0 || chaptersToSkip === "") {
            return
        }

        const words = chaptersToSkip.split(",")
        for (let i = 0; i < words.length; ++i) {
            if (chapters[mpv.chapter] && chapters[mpv.chapter].title.toLowerCase().includes(words[i].trim())) {
                actions.seekNextChapterAction.trigger()
                if (PlaybackSettings.showOsdOnSkipChapters) {
                    osd.message(qsTr("Skipped chapter: %1").arg(chapters[mpv.chapter-1].title))
                }
                // a chapter title can match multiple words
                // return to prevent skipping multiple chapters
                return
            }
        }
    }

    Timer {
       id: playItem
       interval: PlaybackSettings.autoPlayAfterTime
       onTriggered: {
           mpv.pause = false
       }
    }

    onEndFile: {
        if (reason === "error") {
            if (playlistModel.rowCount() === 0) {
                return
            }

            const title = playlistModel.mediaTitle(playlistModel.getPlayingVideo())
            osd.message(qsTr("Could not play: %1").arg(title))
            // only skip to next video if it's a youtube playList
            // to do: figure out why playback fails and act accordingly
            if (!playList.isYouTubePlaylist) {
                return
            }
        }

        if (playList.playlistView.count <= 1) {
            return;
        }

        const loopMode = playlistModel.loopMode(playlistModel.getPlayingVideo())
        if(loopMode===1){ // Continue
            const nextFileRow = playlistModel.getPlayingVideo() + 1
            if (nextFileRow < playList.playlistView.count) {
                mpv.pause = true
                mpv.position = 0
                mpv.loadItem(nextFileRow)
                mpv.playlistModel.setPlayingVideo(nextFileRow)
                if(PlaylistSettings.autoPlayNext)
                    playItem.start()
            } else {
                // Last file in playlist
                if (PlaylistSettings.repeat) {
                    mpv.pause = true
                    mpv.position = 0
                    mpv.loadItem(0)
                    mpv.playlistModel.setPlayingVideo(0)
                    if(PlaylistSettings.autoPlayNext)
                        playItem.start()
                }
            }
        }
        //Pause and Loop are covered in onFileLoaded
    }

    onPauseChanged: {
        if (pause) {
            footer.playPauseButton.icon.name = "media-playback-start"
            lockManager.setInhibitionOff()
        } else {
            footer.playPauseButton.icon.name = "media-playback-pause"
            lockManager.setInhibitionOn()
        }
    }

    Timer {
        id: saveWatchLaterFileTimer

        interval: 1000
        running: !mpv.pause
        repeat: true

        onTriggered: handleTimePosition()
    }

    Timer {
        id: hideCursorTimer

        property double tx: mouseArea.mouseX
        property double ty: mouseArea.mouseY
        property int timeNotMoved: 0

        running: window.isFullScreen() && mouseArea.containsMouse
        repeat: true
        interval: 50

        onTriggered: {
            if (mouseArea.mouseX === tx && mouseArea.mouseY === ty) {
                if (timeNotMoved > 2000) {
                    app.hideCursor()
                }
            } else {
                app.showCursor()
                timeNotMoved = 0
            }
            tx = mouseArea.mouseX
            ty = mouseArea.mouseY
            timeNotMoved += interval
        }
    }

    MouseArea {
        id: mouseArea

        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        anchors.fill: parent
        hoverEnabled: true

        onPositionChanged: {
        }

        onWheel: {
        }

        onPressed: {
            focus = true
            if (mouse.button === Qt.LeftButton) {
                if (MouseSettings.left) {
                    actions[MouseSettings.left].trigger()
                }
            } else if (mouse.button === Qt.MiddleButton) {
                if (MouseSettings.middle) {
                    actions[MouseSettings.middle].trigger()
                }
            } else if (mouse.button === Qt.RightButton) {
                if (MouseSettings.right) {
                    actions[MouseSettings.right].trigger()
                }
            }
        }

        onDoubleClicked: {
            if (mouse.button === Qt.LeftButton) {
                if (MouseSettings.leftx2) {
                    actions[MouseSettings.leftx2].trigger()
                }
            } else if (mouse.button === Qt.MiddleButton) {
                if (MouseSettings.middlex2) {
                    actions[MouseSettings.middlex2].trigger()
                }
            } else if (mouse.button === Qt.RightButton) {
                if (MouseSettings.rightx2) {
                    actions[MouseSettings.rightx2].trigger()
                }
            }
        }
    }

    DropArea {
        id: dropArea

        property var acceptedSubtitleTypes: ["application/x-subrip", "text/x-ssa"]

        anchors.fill: parent
        keys: ["text/uri-list"]

        onDropped: {
            if (acceptedSubtitleTypes.includes(app.mimeType(drop.urls[0]))) {
                const subFile = drop.urls[0].replace("file://", "")
                command(["sub-add", drop.urls[0], "select"])
            }

            if (app.mimeType(drop.urls[0]).startsWith("video/")) {
                window.openFile(drop.urls[0], true, PlaylistSettings.loadSiblings)
            }
        }
    }

    Connections {
        target: playerController

        function onPlaypause() {
            actions.playPauseAction.trigger()
        }
        function onPlay() {
            root.pause = false
        }
        function onPause() {
            root.pause = true
        }
        function onStop() {
            root.position = 0
            root.pause = true
        }
        function onNext() {
            actions.playNextAction.trigger()
        }
        function onPrevious() {
            actions.playPreviousAction.trigger()
        }
        function onSeek(offset) {
            root.command(["add", "time-pos", offset])
        }
        function onLoadFromPlaylist(index) {
            root.pause = true
            root.position = 0
            root.loadItem(index)
            root.playlistModel.setPlayingVideo(index)
        }
        function onLoadFromSections(index) {
            root.pause = true
            root.loadSection(index)
            root.playSectionsModel.setPlayingSection(index)
        }
        function onBackgroundVisibilityChanged(){
            root.opacity = 1 - playerController.backgroundVisibilityOnMaster()
        }
    }

    Rectangle {
        id: loadingIndicatorParent

        visible: false
        anchors.centerIn: parent
        color: {
            let color = Kirigami.Theme.backgroundColor
            Qt.hsla(color.hslHue, color.hslSaturation, color.hslLightness, 0.2)
        }

        Kirigami.Icon {
            id: loadingIndicator

            source: "view-refresh"
            anchors.centerIn: parent
            width: Kirigami.Units.iconSizes.large
            height: Kirigami.Units.iconSizes.large

            RotationAnimator {
                target: loadingIndicator;
                from: 0;
                to: 360;
                duration: 1500
                loops: Animation.Infinite
                running: true
            }

            Component.onCompleted: {
                parent.width = width + 10
                parent.height = height + 10
            }
        }
    }

    Image {
        id: overlayImage
        fillMode: Image.PreserveAspectFit
        anchors.fill: parent
        opacity: 1
    }

    /*Shortcut {
        sequence: StandardKey.NextChild
        onActivated: view.currentIndex++
    }*/

    Component.onCompleted: {
        playerController.mpv = root
        mpv.gridToMapOnChanged()
    }

    function handleTimePosition() {
        if (mpv.position < mpv.duration - 10) {
            saveTimePosition()
        } else {
            resetTimePosition()
        }
    }
}
