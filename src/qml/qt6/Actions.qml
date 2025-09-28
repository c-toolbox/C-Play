/*
 * SPDX-FileCopyrightText:
 * 2021-2025 Erik Sunden <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Controls

import org.ctoolbox.cplay

QtObject {
    id: root

    property bool isPrimary: true
    property real winOpacity: 1.0
    property var list: ({})

    property Action clearRecentMediaFilesAction: Action {
        id: clearRecentMediaFilesAction

        text: qsTr("Clear Recent Media Files")

        onTriggered: {
            mpv.clearRecentMediaFilelist();
        }
    }
    property Action clearRecentPlaylistsAction: Action {
        id: clearRecentPlaylistsAction

        text: qsTr("Clear Recent Playlists")

        onTriggered: {
            mpv.clearRecentPlaylist();
        }
    }
    property Action clearRecentPresentationsAction: Action {
        id: clearRecentPresentationsAction

        text: qsTr("Clear Recent Presentations")

        onTriggered: {
            app.slides.clearRecentPresentations();
        }
    }
    property Action syncAction: Action {
        id: syncAction

        icon.name: playerController.syncProperties ? "network-connect" : "network-disconnect"
        icon.color: playerController.syncProperties ? "lime" : "crimson"
        text: playerController.syncProperties ? qsTr("Sync properties between master and nodes is ON") : qsTr("Sync properties between master and nodes is OFF")

        onTriggered: {
            playerController.syncProperties = !playerController.syncProperties;
        }
    }
    property Action windowOnTopAction: Action {
        id: windowOnTopAction

        text: playerController.nodeWindowsOnTop ? qsTr("Node windows on-top feature is ON") : qsTr("Node windows on-top feature is OFF")
        icon.name: playerController.nodeWindowsOnTop ? "window-restore-pip" : "window-minimize-pip"
        icon.color: playerController.nodeWindowsOnTop ? "lime" : "crimson"

        onTriggered: {
            playerController.nodeWindowsOnTop = !playerController.nodeWindowsOnTop;
        }
    }
    property Action windowOpacityAction: Action {
        id: windowOpacityAction

        text: qsTr("Node windows are VISIBLE")
        icon.name: "view-visible"
        icon.color: "lime"

        onTriggered: {
            if(playerController.nodeWindowsOpacity > 0.0){
                window_fade_out_animation.start();
            }
            else {
                window_fade_in_animation.start();
            }
        }
    }

    onWinOpacityChanged : { 
        playerController.nodeWindowsOpacity = winOpacity;

        if(winOpacity == 0.0){
            windowOpacityAction.text = qsTr("Node windows are HIDDEN");
            windowOpacityAction.icon.name = "view-hidden";
            windowOpacityAction.icon.color = "crimson";
        }
        else if(winOpacity == 1.0){
            windowOpacityAction.text = qsTr("Node windows are VISIBLE");
            windowOpacityAction.icon.name = "view-visible";
            windowOpacityAction.icon.color = "lime";
        }
        else {
            windowOpacityAction.text = qsTr("Node windows are TRANSPARENT");
            windowOpacityAction.icon.name = "view-visible";
            windowOpacityAction.icon.color = "orange";
        }
    }

    property PropertyAnimation winFadeIn: PropertyAnimation {
        id: window_fade_in_animation
        duration: UserInterfaceSettings.windowFadeDuration
        property: "winOpacity"
        target: root
        to: 1.0
    }

    property PropertyAnimation winFadeOut: PropertyAnimation {
        id: window_fade_out_animation
        duration: UserInterfaceSettings.windowFadeDuration
        property: "winOpacity"
        target: root
        to: 0.0
    }

    property Action aboutCPlayAction: Action {
        id: aboutCPlayAction

        property var qaction: app.action("aboutCPlay")

        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["aboutCPlayAction"] = aboutCPlayAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: {
            app.updateAboutOtherText(mpv.getProperty("mpv-version"), mpv.getProperty("ffmpeg-version"));
            qaction.trigger();
        }
    }
    property Action configureAction: Action {
        id: configureAction

        property var qaction: app.action("configure")

        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["configureAction"] = configureAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: {
            settingsEditor.visible = true;
        }
    }
    property Action configureShortcutsAction: Action {
        id: configureShortcutsAction

        property var qaction: app.action("options_configure_keybinding")

        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["configureShortcutsAction"] = configureShortcutsAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: qaction.trigger()
    }
    property Action muteAction: Action {
        id: muteAction

        property var qaction: app.action("mute")

        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["muteAction"] = muteAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: {
            mpv.mute = !mpv.mute;
            if (mpv.mute) {
                text = qsTr("Unmute");
                icon.name = "player-volume-muted";
            } else {
                text = qaction.text;
                icon.name = qaction.iconName();
            }
        }
    }
    property Action openAction: Action {
        id: openAction

        property var qaction: app.action("openFile")

        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["openAction"] = openAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: openFileDialog.open()
    }
    property Action playNextAction: Action {
        id: playNextAction

        property var qaction: app.action("playNext")

        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["playNextAction"] = playNextAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: {
            const nextFileRow = mpv.playlistModel.getPlayingVideo() + 1;
            const updateLastPlayedFile = !playList.isYouTubePlaylist;
            if (nextFileRow < playList.playlistView.count) {
                mpv.pause = true;
                mpv.position = 0;
                mpv.playlistModel.setPlayingVideo(nextFileRow);
                mpv.loadItem(nextFileRow);
            } else {
                // Last file in playlist
                if (PlaylistSettings.repeat) {
                    mpv.pause = true;
                    mpv.position = 0;
                    mpv.playlistModel.setPlayingVideo(0);
                    mpv.loadItem(0);
                }
            }
        }
    }
    property Action playPauseAction: Action {
        id: playPauseAction

        property var qaction: app.action("play_pause")

        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["playPauseAction"] = playPauseAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: mpv.togglePlayPause()
    }
    property Action playPreviousAction: Action {
        id: playPreviousAction

        property var qaction: app.action("playPrevious")

        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["playPreviousAction"] = playPreviousAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: {
            if (mpv.playlistModel.getPlayingVideo() !== 0) {
                const previousFileRow = mpv.playlistModel.getPlayingVideo() - 1;
                const updateLastPlayedFile = !playList.isYouTubePlaylist;
                mpv.pause = true;
                mpv.position = 0;
                mpv.playlistModel.setPlayingVideo(previousFileRow);
                mpv.loadItem(previousFileRow);
            }
        }
    }
    property Action quitApplicationAction: Action {
        id: quitApplicationAction

        property var qaction: app.action("file_quit")

        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["quitApplicationAction"] = quitApplicationAction;
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: {
            qaction.trigger();
        }
    }
    property Action saveAsCPlayFileAction: Action {
        id: saveAsCPlayFileAction

        property var qaction: app.action("saveAsCPlayFile")

        enabled: false
        icon.color: mpv.playSectionsModel.currentEditItemIsEdited ? "orange" : "lime"
        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["saveAsCPlayFileAction"] = saveAsCPlayFileAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: {
            saveCPlayFileDialog.currentFile = mpv.playSectionsModel.getSuggestedFileURL();
            mpv.setLoadedAsCurrentEditItem();
            if (!mpv.playSectionsModel.isEmpty())
                saveAsCPlayFileWindow.visible = true;
        }
    }
    property Action seekBackwardBigAction: Action {
        id: seekBackwardBigAction

        property var qaction: app.action("seekBackwardBig")

        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["seekBackwardBigAction"] = seekBackwardBigAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: mpv.seek(-PlaybackSettings.seekBigStep)
    }
    property Action seekBackwardMediumAction: Action {
        id: seekBackwardMediumAction

        property var qaction: app.action("seekBackwardMedium")

        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["seekBackwardMediumAction"] = seekBackwardMediumAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: mpv.seek(-PlaybackSettings.seekMediumStep)
    }
    property Action seekBackwardSmallAction: Action {
        id: seekBackwardSmallAction

        property var qaction: app.action("seekBackwardSmall")

        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["seekBackwardSmallAction"] = seekBackwardSmallAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: mpv.seek(-PlaybackSettings.seekSmallStep)
    }
    property Action seekForwardBigAction: Action {
        id: seekForwardBigAction

        property var qaction: app.action("seekForwardBig")

        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["seekForwardBigAction"] = seekForwardBigAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: mpv.seek(PlaybackSettings.seekBigStep)
    }
    property Action seekForwardMediumAction: Action {
        id: seekForwardMediumAction

        property var qaction: app.action("seekForwardMedium")

        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["seekForwardMediumAction"] = seekForwardMediumAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: mpv.seek(PlaybackSettings.seekMediumStep)
    }
    property Action seekForwardSmallAction: Action {
        id: seekForwardSmallAction

        property var qaction: app.action("seekForwardSmall")

        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["seekForwardSmallAction"] = seekForwardSmallAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: mpv.seek(PlaybackSettings.seekSmallStep)
    }
    property Action stopRewindAction: Action {
        id: stopRewindAction

        property var qaction: app.action("stop_rewind")

        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["stopRewindAction"] = stopRewindAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: mpv.performRewind()
    }
    property Action toggleFullScreenAction: Action {
        id: toggleFullScreenAction

        property var qaction: app.action("toggleFullScreen")

        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["toggleFullScreenAction"] = toggleFullScreenAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: {
            if (!window.isFullScreenMode) {
                window.isFullScreenMode = true;
                app.hideCursor();
            } else {
                window.isFullScreenMode = false;
                app.showCursor();
            }
        }
    }
    property Action toggleHeaderAction: Action {
        id: toggleHeaderAction

        property var qaction: app.action("toggleHeader")

        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["toggleHeaderAction"] = toggleHeaderAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: UserInterfaceSettings.showHeader = !header.visible
    }
    property Action toggleFooterAction: Action {
        id: toggleFooterAction

        property var qaction: app.action("toggleFooter")

        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["toggleFooterAction"] = toggleFooterAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: UserInterfaceSettings.showFooter = !footer.visible
    }
    property Action toggleLayersAction: Action {
        id: toggleLayersAction

        property var qaction: app.action("toggleLayers")

        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["toggleLayersAction"] = toggleLayersAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: {
            if (layers.state === "hidden" && slides.state === "hidden") {
                layers.state = "visible-without-partner";
            } else if (layers.state === "hidden" && slides.state === "visible-without-partner") {
                slides.state = "visible-with-partner";
                layers.state = "visible-with-partner";
            } else if (slides.state === "visible-with-partner") {
                slides.state = "visible-without-partner";
                layers.state = "hidden";
            } else {
                layers.state = "hidden";
            }
        }
    }
    property Action toggleMenuBarAction: Action {
        id: toggleMenuBarAction

        property var qaction: app.action("toggleMenuBar")

        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["toggleMenuBarAction"] = toggleMenuBarAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: menuBar.hide = !menuBar.hide
    }
    property Action togglePlaylistAction: Action {
        id: togglePlaylistAction

        property var qaction: app.action("togglePlaylist")

        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["togglePlaylistAction"] = togglePlaylistAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: {
            if (playList.state === "hidden" && playSections.state === "hidden") {
                playList.state = "visible-without-partner";
            } else if (playList.state === "hidden" && playSections.state === "visible-without-partner") {
                playList.state = "visible-with-partner";
                playSections.state = "visible-with-partner";
            } else if (playSections.state === "visible-with-partner") {
                playSections.state = "visible-without-partner";
                playList.state = "hidden";
            } else {
                playList.state = "hidden";
            }
        }
    }
    property Action toggleSectionsAction: Action {
        id: toggleSectionsAction

        property var qaction: app.action("toggleSections")

        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["toggleSectionsAction"] = toggleSectionsAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: {
            if (playSections.state === "hidden" && playList.state === "hidden") {
                playSections.state = "visible-without-partner";
            } else if (playSections.state === "hidden" && playList.state === "visible-without-partner") {
                playList.state = "visible-with-partner";
                playSections.state = "visible-with-partner";
            } else if (playList.state === "visible-with-partner") {
                playList.state = "visible-without-partner";
                playSections.state = "hidden";
            } else {
                playSections.state = "hidden";
            }
        }
    }
    property Action toggleSlidesAction: Action {
        id: toggleSlidesAction

        property var qaction: app.action("toggleSlides")

        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["toggleSlidesAction"] = toggleSlidesAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: {
            if (slides.state === "hidden" && layers.state === "hidden") {
                slides.state = "visible-without-partner";
            } else if (slides.state === "hidden" && layers.state === "visible-without-partner") {
                slides.state = "visible-with-partner";
                layers.state = "visible-with-partner";
            } else if (layers.state === "visible-with-partner") {
                layers.state = "visible-without-partner";
                slides.state = "hidden";
            } else {
                slides.state = "hidden";
            }
        }
    }
    property Action layerCopyAction: Action {
        id: layerCopyAction

        property var qaction: app.action("layerCopy")

        enabled: qaction.enabled
        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["layerCopyAction"] = layerCopyAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: {
            if(enabled){
                app.slides.copyLayer();
                app.action("layerPaste").enabled = true;
                app.action("layerPasteProperties").enabled = true;
            }
        }
    }
    property Action layerPasteAction: Action {
        id: layerPasteAction

        property var qaction: app.action("layerPaste")

        enabled: qaction.enabled
        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["layerPasteAction"] = layerPasteAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: {
            if(enabled){
                app.slides.pasteLayer();
            }
        }
    }
    property Action layerPastePropertiesAction: Action {
        id: layerPastePropertiesAction

        property var qaction: app.action("layerPasteProperties")

        enabled: qaction.enabled
        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["layerPastePropertiesAction"] = layerPastePropertiesAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: {
            if(enabled){
                app.slides.pasteLayerAsProperties(app.slides.selected.layerToCopy);
            }
        }
    }
    property Action slidePreviousAction: Action {
        id: slidePreviousAction

        property var qaction: app.action("slidePrevious")

        enabled: qaction.enabled
        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["slidePreviousAction"] = slidePreviousAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: {
            if(enabled && (app.slides.selectedSlideIdx > -2)){
                layers.layersView.currentIndex = -1;
                if(app.slides.selectedSlideIdx === app.slides.triggeredSlideIdx){
                    app.slides.slideFadeTime = PresentationSettings.fadeDurationToPreviousSlide;
                    app.slides.selectedSlideIdx = app.slides.selectedSlideIdx - 1;
                    app.slides.triggeredSlideIdx = app.slides.triggeredSlideIdx - 1;
                }
                else {
                    app.slides.slideFadeTime = PresentationSettings.fadeDurationToNextSlide;
                    app.slides.triggeredSlideIdx = app.slides.selectedSlideIdx;
                }   
            }
        }
    }
    property Action slideNextAction: Action {
        id: slideNextAction

        property var qaction: app.action("slideNext")

        enabled: qaction.enabled
        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["slideNextAction"] = slideNextAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: {
            if(enabled && (app.slides.selectedSlideIdx < app.slides.numberOfSlides() - 1)){
                layers.layersView.currentIndex = -1;
                if(app.slides.selectedSlideIdx === app.slides.triggeredSlideIdx){
                    app.slides.slideFadeTime = PresentationSettings.fadeDurationToNextSlide;
                    app.slides.selectedSlideIdx = app.slides.selectedSlideIdx + 1;
                    app.slides.triggeredSlideIdx = app.slides.triggeredSlideIdx + 1;
                }
                else {
                    app.slides.slideFadeTime = PresentationSettings.fadeDurationToNextSlide;
                    app.slides.triggeredSlideIdx = app.slides.selectedSlideIdx;
                }
            }
        }
    }
    property Action visibilityFadeDownAction: Action {
        id: visibilityFadeDownAction

        property var qaction: app.action("visibilityFadeDown")

        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["visibilityFadeDownAction"] = visibilityFadeDownAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: {
            mpv.fadeImageDown();
        }
    }
    property Action visibilityFadeUpAction: Action {
        id: visibilityFadeUpAction

        property var qaction: app.action("visibilityFadeUp")

        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["visibilityFadeUpAction"] = visibilityFadeUpAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: {
            mpv.fadeImageUp();
        }
    }
    property Action volumeDownAction: Action {
        id: volumeDownAction

        property var qaction: app.action("volumeDown")

        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["volumeDownAction"] = volumeDownAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: {
            mpv.command(["add", "volume", -AudioSettings.volumeStep]);
            osd.message(`Volume: ${parseInt(mpv.getProperty("volume"))}`);
        }
    }
    property Action volumeFadeDownAction: Action {
        id: volumeFadeDownAction

        property var qaction: app.action("volumeFadeDown")

        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["volumeFadeDownAction"] = volumeFadeDownAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: {
            mpv.fadeVolumeDown();
        }
    }
    property Action volumeFadeUpAction: Action {
        id: volumeFadeUpAction

        property var qaction: app.action("volumeFadeUp")

        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["volumeFadeUpAction"] = volumeFadeUpAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: {
            mpv.fadeVolumeUp();
        }
    }
    property Action volumeUpAction: Action {
        id: volumeUpAction

        property var qaction: app.action("volumeUp")

        icon.name: qaction.iconName()
        shortcut: root.isPrimary ? qaction.shortcutName() : qaction.alternateName()
        text: qaction.text

        Component.onCompleted: list["volumeUpAction"] = volumeUpAction
        function updateShortcuts() { shortcut = root.isPrimary ? qaction.shortcutName() : qaction.alternateName() }
        onTriggered: {
            mpv.command(["add", "volume", AudioSettings.volumeStep]);
            osd.message(`Volume: ${parseInt(mpv.getProperty("volume"))}`);
        }
    }

    function updateShortcuts() {
        aboutCPlayAction.updateShortcuts();
        configureAction.updateShortcuts();
        configureShortcutsAction.updateShortcuts();
        muteAction.updateShortcuts();
        openAction.updateShortcuts();
        playNextAction.updateShortcuts();
        playPauseAction.updateShortcuts();
        playPreviousAction.updateShortcuts();
        quitApplicationAction.updateShortcuts();
        saveAsCPlayFileAction.updateShortcuts();
        seekBackwardBigAction.updateShortcuts();
        seekBackwardMediumAction.updateShortcuts();
        seekBackwardSmallAction.updateShortcuts();
        seekForwardBigAction.updateShortcuts();
        seekForwardMediumAction.updateShortcuts();
        seekForwardSmallAction.updateShortcuts();
        stopRewindAction.updateShortcuts();
        toggleHeaderAction.updateShortcuts();
        toggleLayersAction.updateShortcuts();
        toggleMenuBarAction.updateShortcuts();
        togglePlaylistAction.updateShortcuts();
        toggleSectionsAction.updateShortcuts();
        toggleSlidesAction.updateShortcuts();
        layerCopyAction.updateShortcuts();
        layerPasteAction.updateShortcuts();
        layerPastePropertiesAction.updateShortcuts();
        slidePreviousAction.updateShortcuts();
        slideNextAction.updateShortcuts();
        visibilityFadeDownAction.updateShortcuts();
        visibilityFadeUpAction.updateShortcuts();
        volumeDownAction.updateShortcuts();
        volumeFadeDownAction.updateShortcuts();
        volumeFadeUpAction.updateShortcuts();
        volumeUpAction.updateShortcuts();
    }
}
